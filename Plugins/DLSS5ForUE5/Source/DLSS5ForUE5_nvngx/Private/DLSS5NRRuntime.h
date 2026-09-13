#pragma once

#include "CoreMinimal.h"
#include "DLSS5NRDiagnostics.h"
#include "DLSS5NRNGXABI.h"
#include "RHIResources.h"

struct ID3D12Device;
struct ID3D12GraphicsCommandList;
struct ID3D12Resource;

struct FDLSS5NREvaluateDesc
{
    uint64 HistoryKey = 0; // Stable view-state key plus sequential pass index.
    uint64 FrameNumber = 0;
    FGPUFenceRHIRef CompletionFence;
    ID3D12GraphicsCommandList* CommandList = nullptr;
    ID3D12Resource* Color = nullptr;
    ID3D12Resource* Output = nullptr;
    ID3D12Resource* Depth = nullptr;
    ID3D12Resource* MotionVectors = nullptr;

    FIntRect ColorRect;
    FIntRect OutputRect;
    FIntRect DepthRect;
    FIntRect MotionRect;

    float MotionScaleX = 1.0f;
    float MotionScaleY = 1.0f;
    bool bDepthInverted = true;
    bool bReset = false;

    // The supplied NR runtime does not expose a dedicated DLSSNR.Exposure key.
    // These values are kept for telemetry/history decisions in the UE integration.
    FVector2f JitterPixels = FVector2f::ZeroVector;
    float PreExposure = 1.0f;

    float Intensity = 1.0f;
    float LocalTone = 1.0f;
    float LocalStructure = 1.0f;
    float SkinStructure = 1.0f;
    bool bAutoMask = true;
    int32 Style = 0;
    bool bUICorrection = false;
    int32 Preset = 1;
};

class FDLSS5NRRuntime
{
public:
    static FDLSS5NRRuntime& Get();

    bool Load();
    void Unload();
    bool EnsureInitialized(ID3D12Device* Device);
    bool Evaluate(const FDLSS5NREvaluateDesc& Desc);
    FCriticalSection& GetExecutionMutex() { return ExecutionMutex; }
    // Call only after submitting and waiting for all GPU work during module shutdown.
    void ReleaseAllFeaturesAfterGPUIdle();

    void UpdateFrameTelemetry(const FDLSS5NRFrameTelemetry& Telemetry);
    FDLSS5NRDiagnosticsSnapshot GetDiagnosticsSnapshot() const;

    bool IsLoaded() const { return DllHandle != nullptr; }
    bool IsInitialized() const { return bInitialized; }
    bool IsCoreFound() const { return CoreModuleHandle != nullptr; }
    bool HasCoreParameters() const { return CoreParameters != nullptr; }
    bool IsSnippetPopulated() const { return bSnippetPopulated; }
    bool IsCallerGatePathReady() const { return bCallerGatePathReady; }
    FString GetRuntimePath() const { return RuntimePath; }
    FString GetCallerModulePath() const { return CallerModulePath; }
    FString GetLastError() const { return LastError; }
    uint64 GetSuccessfulFrames() const { return SuccessfulFrames; }
    FIntPoint GetFeatureSize() const { return FeatureSize; }
    FNGXResult GetLastNGXResult() const { return LastNGXResult; }

private:
    friend class FDLSS5NRRuntimeRegressionTest;
    FDLSS5NRRuntime() = default;
    ~FDLSS5NRRuntime() = default;

    using PFN_InitExt = FNGXResult(__cdecl*)(uint64, const wchar_t*, ID3D12Device*, uint32, const FNGXParameter*);
    using PFN_Shutdown1 = FNGXResult(__cdecl*)(ID3D12Device*);
    using PFN_PopulateParameters = FNGXResult(__cdecl*)(FNGXParameter*);
    using PFN_CreateFeature = FNGXResult(__cdecl*)(ID3D12GraphicsCommandList*, FNGXFeature, FNGXParameter*, FNGXHandle**);
    using PFN_EvaluateFeature = FNGXResult(__cdecl*)(ID3D12GraphicsCommandList*, const FNGXHandle*, const FNGXParameter*, void*);
    using PFN_ReleaseFeature = FNGXResult(__cdecl*)(FNGXHandle*);
    using PFN_GetAPIVersion = uint32(__cdecl*)();
    using PFN_GetApplicationId = uint64(__cdecl*)();

    using PFN_GetCapabilityParameters = FNGXResult(__cdecl*)(FNGXParameter**);
    using PFN_GetParameters = FNGXResult(__cdecl*)(FNGXParameter**);
    using PFN_DestroyParameters = FNGXResult(__cdecl*)(FNGXParameter*);

    bool ResolveNGXCoreParameters();
    struct FFeatureState
    {
        FNGXHandle* Handle = nullptr;
        FIntPoint Size = FIntPoint::ZeroValue;
        int32 Preset = INDEX_NONE;
        bool bUsesDepth = false;
        bool bUsesMotion = false;
        FIntPoint DepthSize = FIntPoint::ZeroValue;
        FIntPoint MotionSize = FIntPoint::ZeroValue;
        bool bNeedsReset = true;
        uint64 LastFrame = 0;
        FGPUFenceRHIRef CompletionFence;
    };
    FFeatureState* CreateFeature(const FDLSS5NREvaluateDesc& Desc);
    void CollectCompletedFeatures(uint64 FrameNumber);
    void DestroyFeature(FFeatureState& Feature);
    void SetError(const FString& Error, FNGXResult Result = 0);
    void DetectCallerModulePath();

    void* DllHandle = nullptr;
    void* CoreModuleHandle = nullptr; // Borrowed handle owned by NVIDIA/NGX.
    FString RuntimePath;
    FString CallerModulePath;
    FString LastError;

    PFN_InitExt InitExt = nullptr;
    PFN_Shutdown1 Shutdown1 = nullptr;
    PFN_PopulateParameters PopulateParametersFn = nullptr;
    PFN_CreateFeature CreateFeatureFn = nullptr;
    PFN_EvaluateFeature EvaluateFeatureFn = nullptr;
    PFN_ReleaseFeature ReleaseFeatureFn = nullptr;
    PFN_GetAPIVersion GetAPIVersion = nullptr;
    PFN_GetApplicationId GetApplicationId = nullptr;

    PFN_GetCapabilityParameters CoreGetCapabilityParameters = nullptr;
    PFN_GetParameters CoreGetParameters = nullptr;
    PFN_DestroyParameters CoreDestroyParameters = nullptr;
    FNGXParameter* CoreParameters = nullptr;
    bool bOwnCoreParameters = false;

    ID3D12Device* InitializedDevice = nullptr;
    TMap<uint64, FFeatureState> Features;
    TArray<FFeatureState> RetiredFeatures;
    // Parameter bindings belong to the shared CPU parameter block, not a view.
    bool bParametersHadDepth = false;
    bool bParametersHadMotion = false;
    FIntPoint FeatureSize = FIntPoint::ZeroValue;
    bool bInitialized = false;
    bool bSnippetPopulated = false;
    bool bCallerGatePathReady = false;
    uint64 SuccessfulFrames = 0;
    FNGXResult LastNGXResult = 0;

    mutable FCriticalSection DiagnosticsMutex;
    mutable FCriticalSection ExecutionMutex;
    FDLSS5NRFrameTelemetry LastTelemetry;
    uint64 EvaluateAttempts = 0;
    uint64 FailedFrames = 0;
    uint64 ResetFrames = 0;
    bool bDepthUsedLastFrame = false;
    bool bMotionUsedLastFrame = false;
};
