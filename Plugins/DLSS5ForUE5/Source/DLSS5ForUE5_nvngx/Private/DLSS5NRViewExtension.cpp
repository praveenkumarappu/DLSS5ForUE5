#include "DLSS5NRViewExtension.h"

#include "DLSS5ForUE5Module.h"
#include "DLSS5NRCVars.h"
#include "DLSS5NRMotionVectorPass.h"
#include "DLSS5NRRuntime.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "SceneTexturesConfig.h"

#if DLSS5NR_WITH_D3D12
#include "ID3D12DynamicRHI.h"
#endif

namespace
{
    struct FDLSS5NRFrameConfig
    {
        int32 Preset = 1;
        int32 PassCount = 1;
        float Intensity = 1.0f;
        float LocalTone = 1.0f;
        float LocalStructure = 1.0f;
        float SkinStructure = 1.0f;
        bool bAutoMask = true;
        int32 Style = 0;
        bool bUICorrection = false;
        bool bDepthInverted = true;
        bool bReset = false;
        float MVecScaleX = 1.0f;
        float MVecScaleY = 1.0f;
        FVector2f JitterPixels = FVector2f::ZeroVector;
        float PreExposure = 1.0f;
    };

    BEGIN_SHADER_PARAMETER_STRUCT(FDLSS5NRExternalPassParameters, )
        RDG_TEXTURE_ACCESS(InputColor, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputDepth, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputMotion, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(OutputColor, ERHIAccess::UAVCompute)
    END_SHADER_PARAMETER_STRUCT()

    FIntRect ClampRectToExtent(const FIntRect& Rect, const FIntPoint& Extent)
    {
        return FIntRect(
            FMath::Clamp(Rect.Min.X, 0, Extent.X),
            FMath::Clamp(Rect.Min.Y, 0, Extent.Y),
            FMath::Clamp(Rect.Max.X, 0, Extent.X),
            FMath::Clamp(Rect.Max.Y, 0, Extent.Y));
    }

    bool IsGuideOutputCompatible(const FIntPoint& GuideSize, const FIntPoint& OutputSize)
    {
        if (GuideSize.X <= 0 || GuideSize.Y <= 0 || OutputSize.X <= 0 || OutputSize.Y <= 0)
        {
            return false;
        }

        // The generic RenoDX NR path explicitly rejects incompatible guide/output dimensions.
        // Keep the native UE integration conservative: guides may be lower resolution than the
        // final image, but they must fit inside it and preserve the same display aspect ratio.
        if (GuideSize.X > OutputSize.X || GuideSize.Y > OutputSize.Y)
        {
            return false;
        }

        const double GuideAspect = static_cast<double>(GuideSize.X) / static_cast<double>(GuideSize.Y);
        const double OutputAspect = static_cast<double>(OutputSize.X) / static_cast<double>(OutputSize.Y);
        return FMath::Abs(GuideAspect - OutputAspect) <= FMath::Max<double>(0.01, OutputAspect * 0.01);
    }

    FVector2f ProjectionJitterToPixels(const FSceneView& View, const FIntRect& RenderRect)
    {
        // FViewMatrices stores jitter as projection-matrix offsets. UE inserts pixel jitter as:
        //   ProjectionJitterX = PixelX *  2 / Width
        //   ProjectionJitterY = PixelY * -2 / Height
        // Convert it back to the pixel-space convention expected by NGX/Streamline diagnostics.
        const FVector2D ProjectionJitter = View.ViewMatrices.GetTemporalAAJitter();
        return FVector2f(
            static_cast<float>(ProjectionJitter.X * 0.5 * RenderRect.Width()),
            static_cast<float>(ProjectionJitter.Y * -0.5 * RenderRect.Height()));
    }

    uint32 GetStableViewKey(const FSceneView& View)
    {
        if (View.State)
        {
            const uint32 Key = View.State->GetViewKey();
            if (Key != 0)
            {
                return Key;
            }
        }
        return PointerHash(View.Family);
    }
}

FDLSS5NRViewExtension::FDLSS5NRViewExtension(const FAutoRegister& AutoRegister)
    : FSceneViewExtensionBase(AutoRegister)
{
}

void FDLSS5NRViewExtension::SubscribeToPostProcessingPass(
    EPostProcessingPass Pass,
    const FSceneView& InView,
    FDLSS5NRPostProcessDelegateArray& InOutPassCallbacks,
    bool bIsPassEnabled)
{
    if (Pass != EPostProcessingPass::Tonemap)
    {
        return;
    }

    if (DLSS5NR::GetEnable() == 0)
    {
        // If NR is disabled for one or more frames, do not reuse stale history when it comes back.
        HistoryByView.Reset();
        return;
    }

    // Tonemap remains the v0.4 insertion point: temporal upscaling / DLSS has already produced
    // the current color path, while Slate UI is still downstream. Scene depth/velocity remain
    // available through FPostProcessMaterialInputs::SceneTextures.
    if (bIsPassEnabled)
    {
        InOutPassCallbacks.Add(
            FAfterPassCallbackDelegate::CreateRaw(this, &FDLSS5NRViewExtension::AfterTonemap_RenderThread));
    }
}

FScreenPassTexture FDLSS5NRViewExtension::AfterTonemap_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& Inputs)
{
    const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(
        GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
    if (!SceneColor.IsValid() || DLSS5NR::GetEnable() == 0)
    {
        return SceneColor;
    }

#if !DLSS5NR_WITH_D3D12
    return SceneColor;
#else
    if (!IsRHID3D12())
    {
        static bool bLoggedWrongRHI = false;
        if (!bLoggedWrongRHI)
        {
            UE_LOG(LogDLSS5, Warning, TEXT("DLSS5 is enabled but the active RHI is not D3D12. Pass-through mode is active."));
            bLoggedWrongRHI = true;
        }
        return SceneColor;
    }

    const FIntRect ColorRect = SceneColor.ViewRect;
    if (ColorRect.Width() <= 0 || ColorRect.Height() <= 0)
    {
        return SceneColor;
    }

    // The scene guide buffers belong to the primary render view. With DLSS/TSR active this may
    // be lower resolution than the post-upscale color rect, so keep independent subrects.
    const FIntRect RenderRect = View.UnscaledViewRect;
    if (RenderRect.Width() <= 0 || RenderRect.Height() <= 0)
    {
        return SceneColor;
    }

    FRDGTextureRef SceneDepth = nullptr;
    FRDGTextureRef SceneVelocity = nullptr;
    if (TRDGUniformBuffer<FSceneTextureUniformParameters>* SceneTextureUB = Inputs.SceneTextures.SceneTextures.GetUniformBuffer())
    {
        if (const FSceneTextureUniformParameters* SceneTextureContents = SceneTextureUB->GetContents())
        {
            SceneDepth = SceneTextureContents->SceneDepthTexture;
            SceneVelocity = SceneTextureContents->GBufferVelocityTexture;
        }
    }

    const bool bSceneGuidesEnabled = DLSS5NR::GetEnableSceneGuides() != 0;
    const bool bFeedSceneGuides = bSceneGuidesEnabled && DLSS5NR::GetFeedSceneGuides() != 0;
    // Generate requested guides whenever the master switch is enabled so diagnostics can verify
    // them without immediately handing fresh D3D12 resources to NGX. Feeding is a separate,
    // deliberately conservative switch in v0.4.6.
    const bool bDepthRequested = bSceneGuidesEnabled && DLSS5NR::GetUseDepth() != 0;
    const bool bMotionRequested = bSceneGuidesEnabled && DLSS5NR::GetUseMotionVectors() != 0;
    const bool bDepthAvailable = SceneDepth != nullptr;
    const bool bVelocityAvailable = SceneVelocity != nullptr;

    // FSceneView::UnscaledViewRect is the public UE5.6 view rectangle. Scene guides can be
    // lower resolution than the post-upscale color output, so clamp the active rectangle to
    // the actual scene-depth allocation before resolving compact zero-based guide textures.
    const FIntRect GuideSourceRect = bDepthAvailable
        ? ClampRectToExtent(RenderRect, SceneDepth->Desc.Extent)
        : FIntRect();

    FRDGTextureRef ResolvedDepth = nullptr;
    FIntRect DepthRect;
    if (bDepthRequested && bDepthAvailable && GuideSourceRect.Width() > 0 && GuideSourceRect.Height() > 0)
    {
        ResolvedDepth = AddDLSS5NRDepthResolvePass(GraphBuilder, View, SceneDepth, GuideSourceRect);
        if (ResolvedDepth)
        {
            DepthRect = FIntRect(FIntPoint::ZeroValue, ResolvedDepth->Desc.Extent);
        }
    }

    FRDGTextureRef DenseMotion = nullptr;
    FIntRect MotionRect;
    if (bMotionRequested && bDepthAvailable && bVelocityAvailable &&
        GuideSourceRect.Width() > 0 && GuideSourceRect.Height() > 0)
    {
        DenseMotion = AddDLSS5NRMotionVectorResolvePass(
            GraphBuilder, View, SceneDepth, SceneVelocity, GuideSourceRect);
        if (DenseMotion)
        {
            MotionRect = FIntRect(FIntPoint::ZeroValue, DenseMotion->Desc.Extent);
        }
    }

    const bool bDepthCompatible = ResolvedDepth && IsGuideOutputCompatible(ResolvedDepth->Desc.Extent, ColorRect.Size());
    const bool bMotionCompatible = DenseMotion && IsGuideOutputCompatible(DenseMotion->Desc.Extent, ColorRect.Size());
    const bool bUseDepth = bFeedSceneGuides && bDepthCompatible;
    const bool bUseMotion = bFeedSceneGuides && bMotionCompatible;

    const FVector2f JitterPixels = ProjectionJitterToPixels(View, RenderRect);
    const float PreExposure = View.State ? FMath::Max(View.State->GetPreExposure(), KINDA_SMALL_NUMBER) : 1.0f;

    FString ResetReason;
    bool bReset = false;
    const uint32 ViewKey = GetStableViewKey(View);
    const FViewHistoryState* Previous = HistoryByView.Find(ViewKey);

    // A first evaluation always starts a new NR history, independent of the automatic-reset toggle.
    if (!Previous || !Previous->bValid)
    {
        bReset = true;
        ResetReason = TEXT("first NR frame");
    }

    if (View.bCameraCut)
    {
        bReset = true;
        ResetReason = TEXT("UE camera cut");
    }
    else if (View.bForceCameraVisibilityReset)
    {
        bReset = true;
        ResetReason = TEXT("UE forced camera visibility reset");
    }
    else if (DLSS5NR::GetAutoResetHistory() != 0 && Previous && Previous->bValid)
    {
        if (Previous->ColorRect != ColorRect || Previous->RenderRect != RenderRect)
        {
            bReset = true;
            ResetReason = TEXT("view/subrect changed");
        }
        else if (Previous->DepthExtent != (SceneDepth ? SceneDepth->Desc.Extent : FIntPoint::ZeroValue) ||
                 Previous->VelocityExtent != (SceneVelocity ? SceneVelocity->Desc.Extent : FIntPoint::ZeroValue))
        {
            bReset = true;
            ResetReason = TEXT("scene guide extent changed");
        }
        else if (Previous->bDepthAvailable != bDepthAvailable || Previous->bMotionAvailable != (DenseMotion != nullptr))
        {
            bReset = true;
            ResetReason = TEXT("scene guide availability changed");
        }
        else
        {
            const float ExposureRatioLimit = FMath::Max(1.1f, DLSS5NR::GetExposureResetRatio());
            const float ExposureRatio = FMath::Max(
                PreExposure / FMath::Max(Previous->PreExposure, KINDA_SMALL_NUMBER),
                Previous->PreExposure / FMath::Max(PreExposure, KINDA_SMALL_NUMBER));
            if (ExposureRatio > ExposureRatioLimit)
            {
                bReset = true;
                ResetReason = FString::Printf(TEXT("pre-exposure discontinuity x%.2f"), ExposureRatio);
            }
        }
    }

    FViewHistoryState& CurrentHistory = HistoryByView.FindOrAdd(ViewKey);
    CurrentHistory.bValid = true;
    CurrentHistory.ColorRect = ColorRect;
    CurrentHistory.RenderRect = RenderRect;
    CurrentHistory.DepthExtent = SceneDepth ? SceneDepth->Desc.Extent : FIntPoint::ZeroValue;
    CurrentHistory.VelocityExtent = SceneVelocity ? SceneVelocity->Desc.Extent : FIntPoint::ZeroValue;
    CurrentHistory.bDepthAvailable = bDepthAvailable;
    CurrentHistory.bMotionAvailable = DenseMotion != nullptr;
    CurrentHistory.PreExposure = PreExposure;

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
    const FScreenPassViewInfo ViewInfo(View.GetFeatureLevel());
#else
    const FScreenPassViewInfo ViewInfo(View);
#endif

    // NR input/output remain FP16 UAV-capable proxies. Output is prefilled so every failure mode
    // remains a visual pass-through rather than turning the viewport black.
    const FRDGTextureDesc ProxyDesc = FRDGTextureDesc::Create2D(
        SceneColor.Texture->Desc.Extent,
        PF_FloatRGBA,
        FClearValueBinding::None,
        TexCreate_ShaderResource | TexCreate_UAV | TexCreate_RenderTargetable);

    FRDGTextureRef NRInput = GraphBuilder.CreateTexture(ProxyDesc, TEXT("DLSS5NR.InputFP16"));
    const FScreenPassTexture NRInputScreen(NRInput, ColorRect);

    AddDrawTexturePass(GraphBuilder, ViewInfo, SceneColor, NRInputScreen);

    FDLSS5NRFrameConfig Config;
    Config.Preset = FMath::Clamp(DLSS5NR::GetPreset(), 0, 3);
    Config.PassCount = FMath::Clamp(DLSS5NR::GetPassCount(), 1, 4);
    Config.Intensity = DLSS5NR::GetIntensity();
    Config.LocalTone = DLSS5NR::GetLocalTone();
    Config.LocalStructure = DLSS5NR::GetLocalStructure();
    Config.SkinStructure = DLSS5NR::GetSkinStructure();
    Config.bAutoMask = DLSS5NR::GetAutoMask() != 0;
    Config.Style = DLSS5NR::GetStyle();
    Config.bUICorrection = DLSS5NR::GetUICorrection() != 0;
    Config.bDepthInverted = DLSS5NR::GetDepthInverted() != 0;
    Config.bReset = bReset;
    Config.JitterPixels = JitterPixels;
    Config.PreExposure = PreExposure;

    // Dense motion output is in pixels. Normalize by its own render-resolution dimensions,
    // then apply the user multiplier for debugging unusual conventions.
    if (DenseMotion && DenseMotion->Desc.Extent.X > 0 && DenseMotion->Desc.Extent.Y > 0)
    {
        Config.MVecScaleX = (1.0f / static_cast<float>(DenseMotion->Desc.Extent.X)) * DLSS5NR::GetMVecScaleX();
        Config.MVecScaleY = (1.0f / static_cast<float>(DenseMotion->Desc.Extent.Y)) * DLSS5NR::GetMVecScaleY();
    }

    FDLSS5NRFrameTelemetry Telemetry;
    Telemetry.bDepthAvailable = bDepthAvailable;
    Telemetry.bMotionAvailable = DenseMotion != nullptr;
    Telemetry.bDepthRequested = bDepthRequested;
    Telemetry.bMotionRequested = bMotionRequested;
    Telemetry.bReset = bReset;
    Telemetry.bCameraCut = View.bCameraCut;
    Telemetry.bForceCameraReset = View.bForceCameraVisibilityReset;
    Telemetry.ColorRect = ColorRect;
    Telemetry.OutputRect = ColorRect;
    Telemetry.DepthRect = DepthRect;
    Telemetry.MotionRect = MotionRect;
    Telemetry.SceneDepthExtent = SceneDepth ? SceneDepth->Desc.Extent : FIntPoint::ZeroValue;
    Telemetry.SceneVelocityExtent = SceneVelocity ? SceneVelocity->Desc.Extent : FIntPoint::ZeroValue;
    Telemetry.JitterPixels = JitterPixels;
    Telemetry.PreExposure = PreExposure;
    Telemetry.MotionScaleX = Config.MVecScaleX;
    Telemetry.MotionScaleY = Config.MVecScaleY;
    Telemetry.ResetReason = ResetReason;
    FDLSS5NRRuntime::Get().UpdateFrameTelemetry(Telemetry);

    // Sequential multi-pass mode. Pass 1 consumes the original post-tonemap color proxy;
    // each additional pass consumes the previous NR output. Every output is prefilled from
    // its input so an NGX failure degrades to pass-through instead of black.
    FRDGTextureRef CurrentPassInput = NRInput;
    FRDGTextureRef FinalNROutput = NRInput;

    for (int32 PassIndex = 0; PassIndex < Config.PassCount; ++PassIndex)
    {
        FRDGTextureRef PassOutput = GraphBuilder.CreateTexture(ProxyDesc, TEXT("DLSS5NR.PassOutputFP16"));

        const FScreenPassTexture CurrentInputScreen(CurrentPassInput, ColorRect);
        const FScreenPassTexture PassOutputScreen(PassOutput, ColorRect);
        AddDrawTexturePass(GraphBuilder, ViewInfo, CurrentInputScreen, PassOutputScreen);

        FDLSS5NRExternalPassParameters* PassParameters = GraphBuilder.AllocParameters<FDLSS5NRExternalPassParameters>();
        PassParameters->InputColor = CurrentPassInput;
        PassParameters->InputDepth = bUseDepth ? ResolvedDepth : nullptr;
        PassParameters->InputMotion = bUseMotion ? DenseMotion : nullptr;
        PassParameters->OutputColor = PassOutput;

        FRDGTextureRef ThisPassInput = CurrentPassInput;
        FRDGTextureRef ThisPassOutput = PassOutput;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("DLSS5NR Native NGX Feature 18 Pass %d/%d", PassIndex + 1, Config.PassCount),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ThisPassInput, ThisPassOutput, ResolvedDepth, DenseMotion, ColorRect, DepthRect, MotionRect, bUseDepth, bUseMotion, Config, PassIndex](FRHIComputeCommandList& RHICmdList)
            {
                FRHITexture* InputRHI = ThisPassInput->GetRHI();
                FRHITexture* OutputRHI = ThisPassOutput->GetRHI();
                FRHITexture* DepthRHI = bUseDepth && ResolvedDepth ? ResolvedDepth->GetRHI() : nullptr;
                FRHITexture* MotionRHI = bUseMotion && DenseMotion ? DenseMotion->GetRHI() : nullptr;
                if (!InputRHI || !OutputRHI)
                {
                    return;
                }

                RHICmdList.EnqueueLambda(
                    TEXT("DLSS5NR.NGXEvaluate"),
                    [InputRHI, OutputRHI, DepthRHI, MotionRHI, ColorRect, DepthRect, MotionRect, Config, PassIndex](FRHICommandListBase& ExecutingCmdList)
                    {
                        ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
                        if (!D3D12RHI)
                        {
                            return;
                        }

                        const uint32 DeviceIndex = D3D12RHI->RHIGetResourceDeviceIndex(OutputRHI);
                        ID3D12Device* Device = D3D12RHI->RHIGetDevice(DeviceIndex);

                        ID3D12GraphicsCommandList* NativeCmd = D3D12RHI->RHIGetGraphicsCommandList(ExecutingCmdList, DeviceIndex);
                        ID3D12Resource* InputResource = D3D12RHI->RHIGetResource(InputRHI);
                        ID3D12Resource* OutputResource = D3D12RHI->RHIGetResource(OutputRHI);
                        ID3D12Resource* DepthResource = DepthRHI ? D3D12RHI->RHIGetResource(DepthRHI) : nullptr;
                        ID3D12Resource* MotionResource = MotionRHI ? D3D12RHI->RHIGetResource(MotionRHI) : nullptr;

                        if (!Device || !NativeCmd || !InputResource || !OutputResource)
                        {
                            UE_LOG(LogDLSS5, Error, TEXT("Could not resolve D3D12 device/command list/resources for DLSS-NR pass %d."), PassIndex + 1);
                            return;
                        }

                        FDLSS5NRRuntime& Runtime = FDLSS5NRRuntime::Get();
                        if (!Runtime.EnsureInitialized(Device))
                        {
                            return;
                        }

                        FDLSS5NREvaluateDesc Desc;
                        Desc.CommandList = NativeCmd;
                        Desc.Color = InputResource;
                        Desc.Output = OutputResource;
                        Desc.Depth = DepthResource;
                        Desc.MotionVectors = MotionResource;
                        Desc.ColorRect = ColorRect;
                        Desc.OutputRect = ColorRect;
                        Desc.DepthRect = DepthRect;
                        Desc.MotionRect = MotionRect;
                        Desc.Preset = Config.Preset;
                        Desc.Intensity = Config.Intensity;
                        Desc.LocalTone = Config.LocalTone;
                        Desc.LocalStructure = Config.LocalStructure;
                        Desc.SkinStructure = Config.SkinStructure;
                        Desc.bAutoMask = Config.bAutoMask;
                        Desc.Style = Config.Style;
                        Desc.bUICorrection = Config.bUICorrection;
                        Desc.bDepthInverted = Config.bDepthInverted;
                        Desc.MotionScaleX = Config.MVecScaleX;
                        Desc.MotionScaleY = Config.MVecScaleY;
                        // Only the first sequential pass owns the frame-level history reset.
                        Desc.bReset = Config.bReset && PassIndex == 0;
                        Desc.JitterPixels = Config.JitterPixels;
                        Desc.PreExposure = Config.PreExposure;

                        Runtime.Evaluate(Desc);

                        // NGX records D3D12 work directly. Restore UE descriptor/state caches
                        // before the next RDG pass, including the next sequential NR pass.
                        D3D12RHI->RHIFinishExternalComputeWork(ExecutingCmdList, DeviceIndex, NativeCmd);
                    });
            });

        CurrentPassInput = PassOutput;
        FinalNROutput = PassOutput;
    }

    const FScreenPassTexture NROutputScreen(FinalNROutput, ColorRect);

    if (Inputs.OverrideOutput.IsValid())
    {
        AddDrawTexturePass(GraphBuilder, ViewInfo, NROutputScreen, Inputs.OverrideOutput);
        return Inputs.OverrideOutput;
    }

    return NROutputScreen;
#endif
}
