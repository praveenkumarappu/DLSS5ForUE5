#pragma once

#include "CoreMinimal.h"
#include "SceneViewExtension.h"
#include "ScreenPass.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "Runtime/Launch/Resources/Version.h"

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
using FDLSS5NRPostProcessDelegateArray = FAfterPassCallbackDelegateArray;
#else
using FDLSS5NRPostProcessDelegateArray = FPostProcessingPassDelegateArray;
#endif

class FDLSS5NRViewExtension final : public FSceneViewExtensionBase
{
public:
    FDLSS5NRViewExtension(const FAutoRegister& AutoRegister);

#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 6
    // UE 5.5 keeps these ISceneViewExtension hooks pure virtual.
    // UE 5.6+ supplies base implementations, so only provide the compatibility
    // no-op overrides on pre-5.6 builds.
    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
    virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}
#endif

    virtual void SubscribeToPostProcessingPass(
        EPostProcessingPass Pass,
        const FSceneView& InView,
        FDLSS5NRPostProcessDelegateArray& InOutPassCallbacks,
        bool bIsPassEnabled) override;

private:
    struct FViewHistoryState
    {
        bool bValid = false;
        uint64 LastFrame = 0;
        FIntRect ColorRect;
        FIntRect RenderRect;
        FIntPoint DepthExtent = FIntPoint::ZeroValue;
        FIntPoint VelocityExtent = FIntPoint::ZeroValue;
        bool bDepthAvailable = false;
        bool bMotionAvailable = false;
        float PreExposure = 1.0f;
    };

    FScreenPassTexture AfterTonemap_RenderThread(
        FRDGBuilder& GraphBuilder,
        const FSceneView& View,
        const FPostProcessMaterialInputs& Inputs);

    TMap<uint32, FViewHistoryState> HistoryByView;
};
