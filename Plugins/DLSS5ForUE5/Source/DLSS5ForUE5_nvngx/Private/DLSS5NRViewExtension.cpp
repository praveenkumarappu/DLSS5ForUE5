#include "DLSS5NRViewExtension.h"

#include "DLSS5ForUE5Module.h"
#include "DLSS5NRCVars.h"
#include "DLSS5NRMotionVectorPass.h"
#include "DLSS5NRRuntime.h"
#include "DLSS5NRSettings.h"
#include "Engine/Texture.h"
#include "TextureResource.h"

#include "RenderGraphBuilder.h"
#include "RenderGraphResources.h"
#include "RenderGraphUtils.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RHI.h"
#include "SceneManagement.h"
#include "SceneTexturesConfig.h"
#include "SceneRendering.h"
#include "Engine/World.h"
#include "Misc/ScopeLock.h"

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
        float ScalingRatio = 1.0f;
    };

    BEGIN_SHADER_PARAMETER_STRUCT(FDLSS5NRExternalPassParameters, )
        RDG_TEXTURE_ACCESS(InputColor, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputDepth, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputMotion, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputMask, ERHIAccess::SRVCompute)
        RDG_TEXTURE_ACCESS(InputField, ERHIAccess::SRVCompute)
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

    bool IsSupportedNRView(const FSceneView& View)
    {
        if (!View.bIsViewInfo || !View.State || !View.Family || !View.Family->Scene ||
            View.bIsSceneCapture || View.bIsReflectionCapture || View.bIsPlanarReflection ||
            !View.IsPerspectiveProjection() || View.Family->EngineShowFlags.HitProxies)
        {
            return false;
        }
        const UWorld* World = View.Family->Scene->GetWorld();
        // Asset thumbnails and material/mesh previews must not consume NR history.
        return World && (World->WorldType == EWorldType::Editor ||
            World->WorldType == EWorldType::PIE || World->WorldType == EWorldType::Game);
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

void FDLSS5NRViewExtension::BeginRenderViewFamily(FSceneViewFamily& InViewFamily)
{
    FTextureResource* Resource = nullptr;
    if (const UDLSS5NRSettings* Settings = GetDefault<UDLSS5NRSettings>())
    {
        UTexture* Texture = Settings->DistortionFieldTexture.Get();
        if (Texture != DistortionFieldKeepAlive.Get())
        {
            DistortionFieldKeepAlive.Reset(Texture);
        }
        if (Texture)
        {
            Resource = Texture->GetResource();
        }
    }

    // FTextureResource is released through a later render command, so reading TextureRHI here is ordered.
    TSharedRef<FDLSS5NRViewExtension, ESPMode::ThreadSafe> Self = StaticCastSharedRef<FDLSS5NRViewExtension>(AsShared());
    ENQUEUE_RENDER_COMMAND(DLSS5NRDistortionField)([Self, Resource](FRHICommandListImmediate&)
    {
        Self->DistortionFieldRHI = Resource ? Resource->TextureRHI : nullptr;
    });
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
    if (bIsPassEnabled && IsSupportedNRView(InView))
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
    if (!SceneColor.IsValid() || DLSS5NR::GetEnable() == 0 || !IsSupportedNRView(View))
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

    FDLSS5NRFrameConfig Config;

    // The scene guide buffers belong to the primary render view. With DLSS/TSR active this may
    // be lower resolution than the post-upscale color rect, so keep independent subrects.
    const FIntRect RenderRect = static_cast<const FViewInfo&>(View).ViewRect;
    if (RenderRect.Width() <= 0 || RenderRect.Height() <= 0)
    {
        return SceneColor;
    }

    FRDGTextureRef SceneDepth = nullptr;
    FRDGTextureRef SceneVelocity = nullptr;
    FRDGTextureSRVRef CustomStencil = nullptr;
    if (TRDGUniformBuffer<FSceneTextureUniformParameters>* SceneTextureUB = Inputs.SceneTextures.SceneTextures.GetUniformBuffer())
    {
        if (const FSceneTextureUniformParameters* SceneTextureContents = SceneTextureUB->GetContents())
        {
            SceneDepth = SceneTextureContents->SceneDepthTexture;
            SceneVelocity = SceneTextureContents->GBufferVelocityTexture;
            CustomStencil = SceneTextureContents->CustomStencilTexture;
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

    // Use the actual pre-upscale rectangle, not the display rectangle or allocation extent.
    // Editor scene textures can contain padding and other viewports.
    const FIntRect GuideSourceRect = bDepthAvailable
        ? ClampRectToExtent(RenderRect, SceneDepth->Desc.Extent)
        : FIntRect();

    // Experimental scaling ratio. Mode 0 hands NR a downscaled input (and guides) and lets it
    // produce the full-size output, DLSS-style. Mode 1 keeps the full input and only sets the hint.
    Config.ScalingRatio = FMath::Clamp(DLSS5NR::GetScalingRatio(), 0.25f, 1.0f);
    const bool bScaledInput = Config.ScalingRatio < 0.999f && DLSS5NR::GetScalingRatioMode() == 0;
    const FIntRect InputRect = bScaledInput
        ? FIntRect(FIntPoint::ZeroValue, FIntPoint(
            FMath::Max(1, FMath::RoundToInt(ColorRect.Width() * Config.ScalingRatio)),
            FMath::Max(1, FMath::RoundToInt(ColorRect.Height() * Config.ScalingRatio))))
        : ColorRect;

    // Guides are resolved at the NR input resolution so NR receives a same-resolution
    // colour/depth/motion contract, the way every working DLSS-NR feeder supplies it.
    const FIntPoint GuideSize = InputRect.Size();

    FRDGTextureRef ResolvedDepth = nullptr;
    FIntRect DepthRect;
    if (bDepthRequested && bDepthAvailable && GuideSourceRect.Width() > 0 && GuideSourceRect.Height() > 0)
    {
        ResolvedDepth = AddDLSS5NRDepthResolvePass(GraphBuilder, View, SceneDepth, GuideSourceRect, GuideSize);
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
            GraphBuilder, View, SceneDepth, SceneVelocity, GuideSourceRect, GuideSize);
        if (DenseMotion)
        {
            MotionRect = FIntRect(FIntPoint::ZeroValue, DenseMotion->Desc.Extent);
        }
    }

    // Experimental control mask from custom depth stencil. Only meaningful alongside the guides.
    FRDGTextureRef ControlMask = nullptr;
    FIntRect MaskRect;
    const int32 MaskStencil = DLSS5NR::GetControlMaskStencil();
    if (bFeedSceneGuides && MaskStencil != 0 && GuideSourceRect.Width() > 0 && GuideSourceRect.Height() > 0)
    {
        ControlMask = AddDLSS5NRControlMaskResolvePass(
            GraphBuilder, View, CustomStencil, GuideSourceRect, GuideSize,
            FMath::Clamp(MaskStencil, -1, 255), DLSS5NR::GetControlMaskInvert() != 0);
        if (ControlMask)
        {
            MaskRect = FIntRect(FIntPoint::ZeroValue, ControlMask->Desc.Extent);
        }
    }

    // Optional lens-distortion field supplied by the project (see r.DLSS5.DistortionFieldTexture).
    FRDGTextureRef DistortionField = nullptr;
    FIntRect FieldRect;
    if (bFeedSceneGuides && DistortionFieldRHI.IsValid())
    {
        DistortionField = RegisterExternalTexture(GraphBuilder, DistortionFieldRHI, TEXT("DLSS5NR.DistortionField"));
        FieldRect = FIntRect(FIntPoint::ZeroValue, DistortionField->Desc.Extent);
    }

    const bool bDepthCompatible = ResolvedDepth && IsGuideOutputCompatible(ResolvedDepth->Desc.Extent, InputRect.Size());
    const bool bMotionCompatible = DenseMotion && IsGuideOutputCompatible(DenseMotion->Desc.Extent, InputRect.Size());
    const bool bUseDepth = bFeedSceneGuides && bDepthCompatible;
    const bool bUseMotion = bFeedSceneGuides && bMotionCompatible;

    // NR receives post-upscale color, so its input has no sampling jitter.
    const FVector2f JitterPixels = FVector2f::ZeroVector;
    const float PreExposure = View.State ? FMath::Max(View.State->GetPreExposure(), KINDA_SMALL_NUMBER) : 1.0f;

    FString ResetReason;
    bool bReset = false;
    const uint32 ViewKey = GetStableViewKey(View);
    const uint64 FrameNumber = GFrameNumberRenderThread;
    for (auto It = HistoryByView.CreateIterator(); It; ++It)
    {
        if (FrameNumber > It.Value().LastFrame + 120) It.RemoveCurrent();
    }
    const FViewHistoryState* Previous = HistoryByView.Find(ViewKey);

    // A first evaluation always starts a new NR history, independent of the automatic-reset toggle.
    if (!Previous || !Previous->bValid || FrameNumber > Previous->LastFrame + 1)
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
        else if (Previous->DepthExtent != (bUseDepth ? ResolvedDepth->Desc.Extent : FIntPoint::ZeroValue) ||
                 Previous->VelocityExtent != (bUseMotion ? DenseMotion->Desc.Extent : FIntPoint::ZeroValue))
        {
            bReset = true;
            ResetReason = TEXT("scene guide extent changed");
        }
        else if (Previous->bDepthAvailable != bUseDepth || Previous->bMotionAvailable != bUseMotion)
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
    CurrentHistory.LastFrame = FrameNumber;
    CurrentHistory.ColorRect = ColorRect;
    CurrentHistory.RenderRect = RenderRect;
    CurrentHistory.DepthExtent = bUseDepth ? ResolvedDepth->Desc.Extent : FIntPoint::ZeroValue;
    CurrentHistory.VelocityExtent = bUseMotion ? DenseMotion->Desc.Extent : FIntPoint::ZeroValue;
    CurrentHistory.bDepthAvailable = bUseDepth;
    CurrentHistory.bMotionAvailable = bUseMotion;
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

    Config.Preset = FMath::Clamp(DLSS5NR::GetPreset(), 0, 3);
    // Style 0..2 (default/natural/cinematic). Feature creation keys on Preset; Style is per-evaluate.
    // A scaled input only makes sense for the first pass; later passes would need a second downscale.
    Config.PassCount = bScaledInput ? 1 : FMath::Clamp(DLSS5NR::GetPassCount(), 1, 4);
    Config.Intensity = DLSS5NR::GetIntensity();
    Config.LocalTone = DLSS5NR::GetLocalTone();
    Config.LocalStructure = DLSS5NR::GetLocalStructure();
    Config.SkinStructure = DLSS5NR::GetSkinStructure();
    Config.bAutoMask = DLSS5NR::GetAutoMask() != 0;
    Config.Style = FMath::Clamp(DLSS5NR::GetStyle(), 0, 2);
    Config.bUICorrection = DLSS5NR::GetUICorrection() != 0;
    Config.bDepthInverted = DLSS5NR::GetDepthInverted() != 0;
    Config.bReset = bReset;
    Config.JitterPixels = JitterPixels;
    Config.PreExposure = PreExposure;

    // Dense motion output is already in display pixels, which is the NGX convention with a unit
    // scale (NVIDIA's UE DLSS plugin sets MotionVectorScale = 1). The CVar is a debug multiplier only.
    Config.MVecScaleX = DLSS5NR::GetMVecScaleX();
    Config.MVecScaleY = DLSS5NR::GetMVecScaleY();

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
    FIntRect CurrentInputRect = ColorRect;
    FRDGTextureRef FinalNROutput = NRInput;

    if (bScaledInput)
    {
        // Point-sampled downscale into the top-left of a same-extent proxy. ponytail: bilinear/box
        // filter if the scaled path proves useful.
        FRDGTextureRef ScaledInput = GraphBuilder.CreateTexture(ProxyDesc, TEXT("DLSS5NR.InputScaledFP16"));
        AddDrawTexturePass(GraphBuilder, ViewInfo, NRInputScreen, FScreenPassTexture(ScaledInput, InputRect));
        CurrentPassInput = ScaledInput;
        CurrentInputRect = InputRect;
    }

    for (int32 PassIndex = 0; PassIndex < Config.PassCount; ++PassIndex)
    {
        FRDGTextureRef PassOutput = GraphBuilder.CreateTexture(ProxyDesc, TEXT("DLSS5NR.PassOutputFP16"));

        const FScreenPassTexture CurrentInputScreen(CurrentPassInput, CurrentInputRect);
        const FScreenPassTexture PassOutputScreen(PassOutput, ColorRect);
        AddDrawTexturePass(GraphBuilder, ViewInfo, CurrentInputScreen, PassOutputScreen);

        FDLSS5NRExternalPassParameters* PassParameters = GraphBuilder.AllocParameters<FDLSS5NRExternalPassParameters>();
        PassParameters->InputColor = CurrentPassInput;
        PassParameters->InputDepth = bUseDepth ? ResolvedDepth : nullptr;
        PassParameters->InputMotion = bUseMotion ? DenseMotion : nullptr;
        PassParameters->InputMask = ControlMask;
        PassParameters->InputField = DistortionField;
        PassParameters->OutputColor = PassOutput;

        FRDGTextureRef ThisPassInput = CurrentPassInput;
        FRDGTextureRef ThisPassOutput = PassOutput;

        GraphBuilder.AddPass(
            RDG_EVENT_NAME("DLSS5NR Native NGX Feature 18 Pass %d/%d", PassIndex + 1, Config.PassCount),
            PassParameters,
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [ThisPassInput, ThisPassOutput, ResolvedDepth, DenseMotion, ControlMask, DistortionField, CurrentInputRect, ColorRect, DepthRect, MotionRect, MaskRect, FieldRect, bUseDepth, bUseMotion, Config, PassIndex, ViewKey, FrameNumber](FRHIComputeCommandList& RHICmdList)
            {
                FRHITexture* InputRHI = ThisPassInput->GetRHI();
                FRHITexture* OutputRHI = ThisPassOutput->GetRHI();
                FRHITexture* DepthRHI = bUseDepth && ResolvedDepth ? ResolvedDepth->GetRHI() : nullptr;
                FRHITexture* MotionRHI = bUseMotion && DenseMotion ? DenseMotion->GetRHI() : nullptr;
                FRHITexture* MaskRHI = ControlMask ? ControlMask->GetRHI() : nullptr;
                FRHITexture* FieldRHI = DistortionField ? DistortionField->GetRHI() : nullptr;
                if (!InputRHI || !OutputRHI)
                {
                    return;
                }

                const FGPUFenceRHIRef CompletionFence = RHICreateGPUFence(TEXT("DLSS5NR.EvaluationComplete"));
                RHICmdList.EnqueueLambda(
                    TEXT("DLSS5NR.NGXEvaluate"),
                    [InputRHI, OutputRHI, DepthRHI, MotionRHI, MaskRHI, FieldRHI, CurrentInputRect, ColorRect, DepthRect, MotionRect, MaskRect, FieldRect, Config, PassIndex, ViewKey, FrameNumber, CompletionFence](FRHICommandListBase& ExecutingCmdList)
                    {
                        ID3D12DynamicRHI* D3D12RHI = GetID3D12DynamicRHI();
                        if (!D3D12RHI)
                        {
                            return;
                        }

                        const uint32 DeviceIndex = D3D12RHI->RHIGetResourceDeviceIndex(OutputRHI);
                        ID3D12Device* Device = D3D12RHI->RHIGetDevice(DeviceIndex);

                        // UE 5.5 retains its existing RDG transition path; these public
                        // residency/barrier methods are available from UE 5.6.
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 6)
                        // Native NGX dispatches bypass UE's normal shader binding path.
                        // Match NVIDIA's UE integration: register residency and flush RDG barriers.
                        for (FRHITexture* Texture : {InputRHI, OutputRHI, DepthRHI, MotionRHI, MaskRHI, FieldRHI})
                        {
                            if (Texture) D3D12RHI->RHIUpdateResourceResidency(ExecutingCmdList, DeviceIndex, Texture);
                        }
                        D3D12RHI->RHIFlushResourceBarriers(ExecutingCmdList, DeviceIndex);
#endif

                        ID3D12GraphicsCommandList* NativeCmd = D3D12RHI->RHIGetGraphicsCommandList(ExecutingCmdList, DeviceIndex);
                        ID3D12Resource* InputResource = D3D12RHI->RHIGetResource(InputRHI);
                        ID3D12Resource* OutputResource = D3D12RHI->RHIGetResource(OutputRHI);
                        ID3D12Resource* DepthResource = DepthRHI ? D3D12RHI->RHIGetResource(DepthRHI) : nullptr;
                        ID3D12Resource* MotionResource = MotionRHI ? D3D12RHI->RHIGetResource(MotionRHI) : nullptr;
                        ID3D12Resource* MaskResource = MaskRHI ? D3D12RHI->RHIGetResource(MaskRHI) : nullptr;
                        ID3D12Resource* FieldResource = FieldRHI ? D3D12RHI->RHIGetResource(FieldRHI) : nullptr;

                        if (!Device || !NativeCmd || !InputResource || !OutputResource)
                        {
                            UE_LOG(LogDLSS5, Error, TEXT("Could not resolve D3D12 device/command list/resources for DLSS-NR pass %d."), PassIndex + 1);
                            return;
                        }

                        FDLSS5NRRuntime& Runtime = FDLSS5NRRuntime::Get();
                        // UE can translate separate command lists concurrently; the NGX
                        // parameter block and feature pool require serialized CPU access.
                        FScopeLock ExecutionLock(&Runtime.GetExecutionMutex());
                        if (!Runtime.EnsureInitialized(Device))
                        {
                            return;
                        }

                        FDLSS5NREvaluateDesc Desc;
                        Desc.HistoryKey = (static_cast<uint64>(ViewKey) << 32) | static_cast<uint32>(PassIndex);
                        Desc.FrameNumber = FrameNumber;
                        Desc.CompletionFence = CompletionFence;
                        Desc.CommandList = NativeCmd;
                        Desc.Color = InputResource;
                        Desc.Output = OutputResource;
                        Desc.Depth = DepthResource;
                        Desc.MotionVectors = MotionResource;
                        Desc.ControlMask = MaskResource;
                        Desc.DistortionField = FieldResource;
                        Desc.ColorRect = CurrentInputRect;
                        Desc.OutputRect = ColorRect;
                        Desc.DepthRect = DepthRect;
                        Desc.MotionRect = MotionRect;
                        Desc.MaskRect = MaskRect;
                        Desc.FieldRect = FieldRect;
                        Desc.ScalingRatio = Config.ScalingRatio;
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
                        // Each pass owns a separate temporal history and must receive resets.
                        Desc.bReset = Config.bReset;
                        Desc.JitterPixels = Config.JitterPixels;
                        Desc.PreExposure = Config.PreExposure;

                        Runtime.Evaluate(Desc);

                        // NGX records D3D12 work directly. Restore UE descriptor/state caches
                        // before the next RDG pass, including the next sequential NR pass.
                        D3D12RHI->RHIFinishExternalComputeWork(ExecutingCmdList, DeviceIndex, NativeCmd);
                    });
                // Signal after the native work, on the same ordered RHI command list.
                RHICmdList.WriteGPUFence(CompletionFence);
                RHICmdList.EnqueueLambda(TEXT("DLSS5NR.KeepFenceAlive"),
                    [CompletionFence](FRHICommandListBase&) {});
            });

        CurrentPassInput = PassOutput;
        CurrentInputRect = ColorRect;
        FinalNROutput = PassOutput;
    }

    // Keep tonemapped highlights (lights, emissive panels) and the tonemapper alpha.
    FinalNROutput = AddDLSS5NRHighlightPreservePass(
        GraphBuilder, View, NRInput, FinalNROutput, ColorRect,
        DLSS5NR::GetHighlightPreserveStart(), DLSS5NR::GetHighlightPreserveEnd());

    const FScreenPassTexture NROutputScreen(FinalNROutput, ColorRect);

    if (Inputs.OverrideOutput.IsValid())
    {
        AddDrawTexturePass(GraphBuilder, ViewInfo, NROutputScreen, Inputs.OverrideOutput);
        return Inputs.OverrideOutput;
    }

    return NROutputScreen;
#endif
}
