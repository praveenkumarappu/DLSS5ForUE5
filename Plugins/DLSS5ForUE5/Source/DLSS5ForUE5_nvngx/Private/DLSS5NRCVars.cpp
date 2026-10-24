#include "DLSS5NRCVars.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarDLSS5NREnable(
    TEXT("r.DLSS5.Enable"), 0,
    TEXT("Enable experimental DLSS Neural Rendering post-pass. 0=Off 1=On"),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRPreset(
    TEXT("r.DLSS5.Preset"), 1,
    TEXT("DLSS-NR render preset hint, 0..3. No effect with the 310.8 runtime, which embeds a single network; kept for future builds. Changing it rebuilds the feature."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRPassCount(
    TEXT("r.DLSS5.PassCount"), 1,
    TEXT("Sequential DLSS 5 Neural Rendering pass count. 1..4. Higher values increase GPU cost."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRIntensity(
    TEXT("r.DLSS5.Intensity"), 1.0f,
    TEXT("Neural rendering intensity."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRLocalTone(
    TEXT("r.DLSS5.LocalTone"), 1.0f,
    TEXT("Local tone strength."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRLocalStructure(
    TEXT("r.DLSS5.LocalStructure"), 1.0f,
    TEXT("Local structure strength."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRSkinStructure(
    TEXT("r.DLSS5.SkinStructure"), 1.0f,
    TEXT("Skin structure strength. -1 leaves the runtime default (NVIDIA reference)."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRAutoMask(
    TEXT("r.DLSS5.AutoMask"), 1,
    TEXT("Use automatic mask."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRStyle(
    TEXT("r.DLSS5.Style"), 0,
    TEXT("Style 0..2. Measured: 0 closest to source, 1 warmer/sharper (NVIDIA reference default), 2 muted/softer (cinematic). Community names Default/Natural/Cinematic are unverified. Changing it resets NR history."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRUICorrection(
    TEXT("r.DLSS5.UICorrection"), 0,
    TEXT("Enable DLSS-NR UI correction hint. UE Slate is downstream of this pass."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NREnableSceneGuides(
    TEXT("r.DLSS5.EnableSceneGuides"), 0,
    TEXT("Master safety switch for experimental depth/motion scene guides. 0=Color-only fallback 1=Allow guides."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRFeedSceneGuides(
    TEXT("r.DLSS5.FeedSceneGuides"), 0,
    TEXT("Feed generated scene guides into the NR runtime. 0=Generate/diagnose only 1=Feed guides. Safe default is 0."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRUseDepth(
    TEXT("r.DLSS5.UseDepth"), 1,
    TEXT("Feed UE scene depth to DLSS-NR when available. 0=Off 1=On."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRUseMotionVectors(
    TEXT("r.DLSS5.UseMotionVectors"), 0,
    TEXT("Build and feed dense DLSS-compatible UE motion vectors when available. 0=Off 1=On."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRAutoResetHistory(
    TEXT("r.DLSS5.AutoResetHistory"), 1,
    TEXT("Reset NR history on camera cuts, forced camera reset, first frame, rect changes, and large exposure discontinuities."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRDepthInverted(
    TEXT("r.DLSS5.DepthInverted"), 1,
    TEXT("UE uses reversed-Z on D3D12, so default is 1."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRMVecScaleX(
    TEXT("r.DLSS5.MVecScaleX"), 1.0f,
    TEXT("Debug multiplier for X motion vectors. Guides are display-pixel units, so 1.0 is the NGX convention."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRMVecScaleY(
    TEXT("r.DLSS5.MVecScaleY"), 1.0f,
    TEXT("Debug multiplier for Y motion vectors. Guides are display-pixel units, so 1.0 is the NGX convention."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRExposureResetRatio(
    TEXT("r.DLSS5.ExposureResetRatio"), 2.0f,
    TEXT("Reset history when pre-exposure changes by more than this ratio. Minimum 1.1."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<FString> CVarDLSS5NRDistortionFieldTexture(
    TEXT("r.DLSS5.DistortionFieldTexture"), TEXT(""),
    TEXT("Asset path of an RGBA16F texture/render target fed as DLSSNR.BidirectionalDistortionField. RG = distorted-to-undistorted displacement, BA = inverse, in motion-vector units (display pixels). Empty = none."),
    ECVF_Default);

static TAutoConsoleVariable<float> CVarDLSS5NRScalingRatio(
    TEXT("r.DLSS5.ScalingRatio"), 1.0f,
    TEXT("Experimental DLSSNR.ScalingRatio (0.25..1). Below 1 the network runs at a lower resolution. Forces pass count 1 while active."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRScalingRatioMode(
    TEXT("r.DLSS5.ScalingRatioMode"), 0,
    TEXT("0 = downsample the NR colour input and guides to the ratio (DLSS-style, NR upscales to the output). 1 = full-resolution input, ratio hint only."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRControlMaskStencil(
    TEXT("r.DLSS5.ControlMaskStencil"), 0,
    TEXT("Feed a DLSSNR.ControlMask guide built from UE custom depth stencil. 0=Off, -1=any non-zero stencil, 1..255=that stencil value. Actors need Render CustomDepth Pass enabled. Experimental: mask semantics are undocumented."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRControlMaskInvert(
    TEXT("r.DLSS5.ControlMaskInvert"), 0,
    TEXT("Invert the control mask (1.0 outside the tagged actors instead of inside)."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRHighlightPreserveStart(
    TEXT("r.DLSS5.HighlightPreserveStart"), 0.7f,
    TEXT("Display-encoded brightness (0..1) where the original tonemapped colour starts blending back over the NR result. Set >= End to disable."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRHighlightPreserveEnd(
    TEXT("r.DLSS5.HighlightPreserveEnd"), 0.9f,
    TEXT("Display-encoded brightness (0..1) from which the original tonemapped colour fully replaces the NR result."),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRDebug(
    TEXT("r.DLSS5.Debug"), 1,
    TEXT("Verbose DLSS5 logging."), ECVF_RenderThreadSafe);

namespace DLSS5NR
{
    int32 GetEnable() { return CVarDLSS5NREnable.GetValueOnRenderThread(); }
    int32 GetPreset() { return CVarDLSS5NRPreset.GetValueOnRenderThread(); }
    int32 GetPassCount() { return CVarDLSS5NRPassCount.GetValueOnRenderThread(); }
    float GetIntensity() { return CVarDLSS5NRIntensity.GetValueOnRenderThread(); }
    float GetLocalTone() { return CVarDLSS5NRLocalTone.GetValueOnRenderThread(); }
    float GetLocalStructure() { return CVarDLSS5NRLocalStructure.GetValueOnRenderThread(); }
    float GetSkinStructure() { return CVarDLSS5NRSkinStructure.GetValueOnRenderThread(); }
    int32 GetAutoMask() { return CVarDLSS5NRAutoMask.GetValueOnRenderThread(); }
    int32 GetStyle() { return CVarDLSS5NRStyle.GetValueOnRenderThread(); }
    int32 GetUICorrection() { return CVarDLSS5NRUICorrection.GetValueOnRenderThread(); }
    int32 GetEnableSceneGuides() { return CVarDLSS5NREnableSceneGuides.GetValueOnRenderThread(); }
    int32 GetFeedSceneGuides() { return CVarDLSS5NRFeedSceneGuides.GetValueOnRenderThread(); }
    int32 GetUseDepth() { return CVarDLSS5NRUseDepth.GetValueOnRenderThread(); }
    int32 GetUseMotionVectors() { return CVarDLSS5NRUseMotionVectors.GetValueOnRenderThread(); }
    int32 GetAutoResetHistory() { return CVarDLSS5NRAutoResetHistory.GetValueOnRenderThread(); }
    int32 GetDepthInverted() { return CVarDLSS5NRDepthInverted.GetValueOnRenderThread(); }
    float GetMVecScaleX() { return CVarDLSS5NRMVecScaleX.GetValueOnRenderThread(); }
    float GetMVecScaleY() { return CVarDLSS5NRMVecScaleY.GetValueOnRenderThread(); }
    float GetExposureResetRatio() { return CVarDLSS5NRExposureResetRatio.GetValueOnRenderThread(); }
    FString GetDistortionFieldTexture() { return CVarDLSS5NRDistortionFieldTexture.GetValueOnGameThread(); }
    float GetScalingRatio() { return CVarDLSS5NRScalingRatio.GetValueOnRenderThread(); }
    int32 GetScalingRatioMode() { return CVarDLSS5NRScalingRatioMode.GetValueOnRenderThread(); }
    int32 GetControlMaskStencil() { return CVarDLSS5NRControlMaskStencil.GetValueOnRenderThread(); }
    int32 GetControlMaskInvert() { return CVarDLSS5NRControlMaskInvert.GetValueOnRenderThread(); }
    float GetHighlightPreserveStart() { return CVarDLSS5NRHighlightPreserveStart.GetValueOnRenderThread(); }
    float GetHighlightPreserveEnd() { return CVarDLSS5NRHighlightPreserveEnd.GetValueOnRenderThread(); }
    int32 GetDebug() { return CVarDLSS5NRDebug.GetValueOnRenderThread(); }
}
