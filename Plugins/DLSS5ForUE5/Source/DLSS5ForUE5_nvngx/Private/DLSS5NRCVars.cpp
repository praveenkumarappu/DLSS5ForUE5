#include "DLSS5NRCVars.h"
#include "HAL/IConsoleManager.h"

static TAutoConsoleVariable<int32> CVarDLSS5NREnable(
    TEXT("r.DLSS5.Enable"), 0,
    TEXT("Enable experimental DLSS Neural Rendering post-pass. 0=Off 1=On"),
    ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRPreset(
    TEXT("r.DLSS5.Preset"), 1,
    TEXT("DLSS-NR render preset hint. Experimental: 0..3."),
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
    TEXT("Skin structure strength."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRAutoMask(
    TEXT("r.DLSS5.AutoMask"), 1,
    TEXT("Use automatic mask."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarDLSS5NRStyle(
    TEXT("r.DLSS5.Style"), 0,
    TEXT("Style. 0=Natural, 1=Cinematic/alternate."), ECVF_RenderThreadSafe);

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
    TEXT("Multiplier applied to automatic X motion-vector normalization (1/view width)."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRMVecScaleY(
    TEXT("r.DLSS5.MVecScaleY"), 1.0f,
    TEXT("Multiplier applied to automatic Y motion-vector normalization (1/view height)."), ECVF_RenderThreadSafe);

static TAutoConsoleVariable<float> CVarDLSS5NRExposureResetRatio(
    TEXT("r.DLSS5.ExposureResetRatio"), 2.0f,
    TEXT("Reset history when pre-exposure changes by more than this ratio. Minimum 1.1."), ECVF_RenderThreadSafe);

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
    int32 GetDebug() { return CVarDLSS5NRDebug.GetValueOnRenderThread(); }
}
