#include "DLSS5NRRuntime.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    class FTestNRFence final : public FRHIGPUFence
    {
    public:
        FTestNRFence() : FRHIGPUFence(TEXT("DLSS5NR.TestFence")) {}
        bool bComplete = false;
        virtual void Clear() override { bComplete = false; }
        virtual bool Poll() const override { return bComplete; }
        // Omit override to also support engines without FRHIGPUFence::Wait.
        virtual void Wait(FRHICommandListImmediate&, FRHIGPUMask) const {}
    };

    uint32 NextHandle = 0;
    uint32 ReleasedHandles = 0;
    int LastReset = -1;
    float LastJitterX = 1.0f;
    float LastJitterY = 1.0f;
    ID3D12Resource* LastDepth = nullptr;
    bool bFailNextEvaluation = false;

    FNGXResult __cdecl TestCreate(ID3D12GraphicsCommandList*, FNGXFeature, FNGXParameter*, FNGXHandle** Out)
    {
        *Out = new FNGXHandle{++NextHandle};
        return DLSS5NRNGX::Success;
    }
    FNGXResult __cdecl TestEvaluate(ID3D12GraphicsCommandList*, const FNGXHandle*, const FNGXParameter* P, void*)
    {
        P->Get("DLSSNR.Reset", &LastReset);
        P->Get("Jitter.Offset.X", &LastJitterX);
        P->Get("Jitter.Offset.Y", &LastJitterY);
        P->Get("DLSSNR.Depth", &LastDepth);
        if (bFailNextEvaluation)
        {
            bFailNextEvaluation = false;
            return DLSS5NRNGX::FailInvalidParameter;
        }
        return DLSS5NRNGX::Success;
    }
    FNGXResult __cdecl TestRelease(FNGXHandle* Handle)
    {
        ++ReleasedHandles;
        delete Handle;
        return DLSS5NRNGX::Success;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDLSS5NRRuntimeRegressionTest,
    "DLSS5.Runtime.HistoryAndGPURetirement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDLSS5NRRuntimeRegressionTest::RunTest(const FString&)
{
    // Exercise the real runtime with a deterministic NGX double and controllable GPU fences.
    // No NVIDIA DLL calls or GPU work are performed by this test.
    NextHandle = ReleasedHandles = 0;
    LastDepth = nullptr;
    bFailNextEvaluation = false;
    FDLSS5NRParameterMap Parameters;
    FDLSS5NRRuntime Runtime;
    Runtime.bInitialized = Runtime.bSnippetPopulated = true;
    Runtime.CoreParameters = &Parameters;
    Runtime.CreateFeatureFn = &TestCreate;
    Runtime.EvaluateFeatureFn = &TestEvaluate;
    Runtime.ReleaseFeatureFn = &TestRelease;

    FDLSS5NREvaluateDesc D;
    D.HistoryKey = uint64(10) << 32;
    D.FrameNumber = 1;
    D.CommandList = reinterpret_cast<ID3D12GraphicsCommandList*>(UPTRINT(1));
    D.Color = reinterpret_cast<ID3D12Resource*>(UPTRINT(2));
    D.Output = reinterpret_cast<ID3D12Resource*>(UPTRINT(3));
    D.ColorRect = D.OutputRect = FIntRect(0, 0, 1280, 720);
    D.CompletionFence = new FTestNRFence;
    D.JitterPixels = FVector2f(0.25f, -0.25f);
    TestTrue(TEXT("First evaluation succeeds"), Runtime.Evaluate(D));
    TestEqual(TEXT("New feature always resets history"), LastReset, 1);
    TestEqual(TEXT("Post-upscale X jitter is zero"), LastJitterX, 0.0f);
    TestEqual(TEXT("Post-upscale Y jitter is zero"), LastJitterY, 0.0f);
    const uint32 MainHandle = Runtime.Features.FindChecked(D.HistoryKey).Handle->Id;

    ++D.FrameNumber;
    D.CompletionFence = new FTestNRFence;
    Runtime.Evaluate(D);
    TestEqual(TEXT("Consecutive frame retains history"), LastReset, 0);
    TestEqual(TEXT("Consecutive frame reuses feature"), NextHandle, 1u);

    ++D.HistoryKey; // Sequential pass 2, same view and resolution.
    Runtime.Evaluate(D);
    TestEqual(TEXT("Second pass gets its own feature"), NextHandle, 2u);
    TestEqual(TEXT("Second pass initializes its own history"), LastReset, 1);
    D.HistoryKey = uint64(11) << 32; // Another view, same resolution.
    Runtime.Evaluate(D);
    TestEqual(TEXT("Second view gets its own feature"), NextHandle, 3u);
    D.HistoryKey = uint64(10) << 32;
    Runtime.Evaluate(D);
    TestEqual(TEXT("Returning to main view preserves its handle"),
        Runtime.Features.FindChecked(D.HistoryKey).Handle->Id, MainHandle);
    TestEqual(TEXT("Other views do not reset main-view history"), LastReset, 0);

    const FGPUFenceRHIRef OldFence = D.CompletionFence;
    D.Preset = 2;
    D.CompletionFence = new FTestNRFence;
    Runtime.Evaluate(D);
    TestEqual(TEXT("Preset change recreates feature"), NextHandle, 4u);
    TestEqual(TEXT("Replacement starts with reset even without camera cut"), LastReset, 1);
    TestEqual(TEXT("In-flight feature is not released"), ReleasedHandles, 0u);
    TestEqual(TEXT("Old feature is retained for GPU completion"), Runtime.RetiredFeatures.Num(), 1);
    static_cast<FTestNRFence*>(OldFence.GetReference())->bComplete = true;
    OldFence->NumPendingWriteCommands.Increment();
    Runtime.CollectCompletedFeatures(D.FrameNumber);
    TestEqual(TEXT("Pending RHI fence write prevents release"), ReleasedHandles, 0u);
    OldFence->NumPendingWriteCommands.Decrement();
    Runtime.CollectCompletedFeatures(D.FrameNumber);
    TestEqual(TEXT("Completed GPU fence permits release"), ReleasedHandles, 1u);

    D.Depth = reinterpret_cast<ID3D12Resource*>(UPTRINT(4));
    D.DepthRect = D.ColorRect;
    Runtime.Evaluate(D);
    TestTrue(TEXT("Depth-enabled view binds its guide"), LastDepth == D.Depth);
    D.HistoryKey = uint64(11) << 32;
    D.Preset = 1;
    D.Depth = nullptr;
    D.DepthRect = FIntRect();
    Runtime.Evaluate(D);
    TestTrue(TEXT("Color-only view clears another view's stale depth pointer"), LastDepth == nullptr);

    D.FrameNumber += 10;
    Runtime.Evaluate(D);
    TestEqual(TEXT("Returning after a frame gap resets history"), LastReset, 1);
    ++D.FrameNumber;
    bFailNextEvaluation = true;
    AddExpectedError(TEXT("DLSS-NR EvaluateFeature failed"), EAutomationExpectedErrorFlags::Contains, 1);
    TestFalse(TEXT("NGX failure is reported"), Runtime.Evaluate(D));
    ++D.FrameNumber;
    Runtime.Evaluate(D);
    TestEqual(TEXT("Recovery after failed evaluate resets history"), LastReset, 1);

    // Test double simulates the caller's shutdown GPU drain.
    Runtime.ReleaseAllFeaturesAfterGPUIdle();
    TestEqual(TEXT("Shutdown releases every created handle exactly once"), ReleasedHandles, NextHandle);
    TestTrue(TEXT("Shutdown empties active and retired pools"), Runtime.Features.IsEmpty() && Runtime.RetiredFeatures.IsEmpty());
    Runtime.CoreParameters = nullptr;
    Runtime.bInitialized = false;
    return true;
}
#endif
