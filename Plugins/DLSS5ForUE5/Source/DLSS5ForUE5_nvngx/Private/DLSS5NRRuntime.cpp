#include "DLSS5NRRuntime.h"

#include "Interfaces/IPluginManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Misc/ScopeLock.h"
#include "DLSS5ForUE5Module.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <Windows.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
#if PLATFORM_WINDOWS
    int32 GDLSS5NRCallerAnchor = 0;
#endif
}

FDLSS5NRRuntime& FDLSS5NRRuntime::Get()
{
    static FDLSS5NRRuntime Instance;
    return Instance;
}

void FDLSS5NRRuntime::SetError(const FString& Error, FNGXResult Result)
{
    LastError = Error;
    if (Result != 0)
    {
        LastNGXResult = Result;
    }
    UE_LOG(LogDLSS5, Error, TEXT("%s"), *Error);
}

void FDLSS5NRRuntime::DetectCallerModulePath()
{
#if PLATFORM_WINDOWS
    HMODULE ThisModule = nullptr;
    if (::GetModuleHandleExW(
        GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&GDLSS5NRCallerAnchor),
        &ThisModule) && ThisModule)
    {
        wchar_t Buffer[32768] = {};
        const DWORD Count = ::GetModuleFileNameW(ThisModule, Buffer, static_cast<DWORD>(UE_ARRAY_COUNT(Buffer)));
        if (Count > 0)
        {
            CallerModulePath = FString(Buffer);
            bCallerGatePathReady = CallerModulePath.Contains(TEXT("nvngx.dll"), ESearchCase::IgnoreCase);
        }
    }
#endif
}

bool FDLSS5NRRuntime::Load()
{
#if !DLSS5NR_WITH_D3D12
    SetError(TEXT("DLSS5ForUE5 currently supports Win64/D3D12 only."));
    return false;
#else
    if (DllHandle)
    {
        return true;
    }

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DLSS5ForUE5"));
    if (!Plugin.IsValid())
    {
        SetError(TEXT("Could not resolve DLSS5ForUE5 plugin directory."));
        return false;
    }

    DetectCallerModulePath();

    RuntimePath = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Binaries/ThirdParty/Win64/nvngx_dlssnr.dll"));
    if (!FPaths::FileExists(RuntimePath))
    {
        SetError(FString::Printf(TEXT("Missing neural runtime: %s"), *RuntimePath));
        return false;
    }

    DllHandle = FPlatformProcess::GetDllHandle(*RuntimePath);
    if (!DllHandle)
    {
        SetError(FString::Printf(TEXT("Failed to load %s"), *RuntimePath));
        return false;
    }

    InitExt = reinterpret_cast<PFN_InitExt>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_Init_Ext")));
    Shutdown1 = reinterpret_cast<PFN_Shutdown1>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_Shutdown1")));
    PopulateParametersFn = reinterpret_cast<PFN_PopulateParameters>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_PopulateParameters_Impl")));
    CreateFeatureFn = reinterpret_cast<PFN_CreateFeature>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_CreateFeature")));
    EvaluateFeatureFn = reinterpret_cast<PFN_EvaluateFeature>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_EvaluateFeature")));
    ReleaseFeatureFn = reinterpret_cast<PFN_ReleaseFeature>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_D3D12_ReleaseFeature")));
    GetAPIVersion = reinterpret_cast<PFN_GetAPIVersion>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_GetAPIVersion")));
    GetApplicationId = reinterpret_cast<PFN_GetApplicationId>(FPlatformProcess::GetDllExport(DllHandle, TEXT("NVSDK_NGX_GetApplicationId")));

    if (!InitExt || !Shutdown1 || !PopulateParametersFn || !CreateFeatureFn || !EvaluateFeatureFn || !ReleaseFeatureFn || !GetAPIVersion || !GetApplicationId)
    {
        SetError(TEXT("nvngx_dlssnr.dll is missing one or more required D3D12 NGX snippet exports."));
        Unload();
        return false;
    }

    UE_LOG(LogDLSS5, Log, TEXT("Loaded DLSS-NR runtime: %s (NGX API 0x%X, AppId 0x%llX)"),
        *RuntimePath, GetAPIVersion(), GetApplicationId());
    UE_LOG(LogDLSS5, Display, TEXT("DLSS-NR caller module path: %s"), *CallerModulePath);
    UE_LOG(LogDLSS5, Display, TEXT("DLSS-NR caller-gate path contains 'nvngx.dll': %s"), bCallerGatePathReady ? TEXT("YES") : TEXT("NO"));

    if (!bCallerGatePathReady)
    {
        UE_LOG(LogDLSS5, Warning,
            TEXT("The supplied DLSS-NR 310.8 snippet checks the caller module path for 'nvngx.dll'. DLSS5ForUE5 satisfies this through its internal *_nvngx module name."));
    }

    LastError.Reset();
    return true;
#endif
}

bool FDLSS5NRRuntime::ResolveNGXCoreParameters()
{
#if !PLATFORM_WINDOWS
    return false;
#else
    if (CoreParameters)
    {
        return true;
    }

    // NVIDIA's official UE DLSS plugin initializes the driver NGX core. We borrow that
    // already-loaded core rather than initializing a competing NGX application session.
    HMODULE Core = ::GetModuleHandleW(L"_nvngx.dll");
    if (!Core)
    {
        SetError(TEXT("NGX driver core '_nvngx.dll' is not loaded yet. Make sure NVIDIA DLSS is enabled and actively running, then retry."));
        return false;
    }

    CoreModuleHandle = reinterpret_cast<void*>(Core);

    // GetProcAddress returns FARPROC. On Windows this is the required mechanism for resolving
    // dynamically loaded exports, but MSVC emits C4191 for the typed function-pointer cast.
    // These signatures mirror the NGX D3D12 ABI validated by the working integration path.
#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable:4191)
#endif
    CoreGetCapabilityParameters = reinterpret_cast<PFN_GetCapabilityParameters>(
        ::GetProcAddress(Core, "NVSDK_NGX_D3D12_GetCapabilityParameters"));
    CoreGetParameters = reinterpret_cast<PFN_GetParameters>(
        ::GetProcAddress(Core, "NVSDK_NGX_D3D12_GetParameters"));
    CoreDestroyParameters = reinterpret_cast<PFN_DestroyParameters>(
        ::GetProcAddress(Core, "NVSDK_NGX_D3D12_DestroyParameters"));
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

    if (!CoreGetCapabilityParameters && !CoreGetParameters)
    {
        SetError(TEXT("Loaded _nvngx.dll does not expose D3D12 capability/parameter accessors."));
        return false;
    }

    FNGXParameter* Params = nullptr;
    FNGXResult Result = DLSS5NRNGX::FailNotInitialized;

    if (CoreGetCapabilityParameters)
    {
        Result = CoreGetCapabilityParameters(&Params);
        if (DLSS5NRNGX::Succeeded(Result) && Params)
        {
            CoreParameters = Params;
            bOwnCoreParameters = CoreDestroyParameters != nullptr;
            LastNGXResult = Result;
            UE_LOG(LogDLSS5, Log, TEXT("Acquired NGX core capability parameter block."));
            return true;
        }
    }

    // Older cores may expose the deprecated shared GetParameters path instead.
    if (CoreGetParameters)
    {
        Params = nullptr;
        const FNGXResult FallbackResult = CoreGetParameters(&Params);
        if (DLSS5NRNGX::Succeeded(FallbackResult) && Params)
        {
            CoreParameters = Params;
            bOwnCoreParameters = false; // Lifetime belongs to NGX for GetParameters().
            LastNGXResult = FallbackResult;
            UE_LOG(LogDLSS5, Warning, TEXT("Using deprecated NGX core GetParameters fallback."));
            return true;
        }
        Result = FallbackResult;
    }

    SetError(FString::Printf(TEXT("Could not acquire NGX core parameters: 0x%08X"), Result), Result);
    return false;
#endif
}

bool FDLSS5NRRuntime::EnsureInitialized(ID3D12Device* Device)
{
    if (!Device)
    {
        SetError(TEXT("Cannot initialize DLSS-NR: D3D12 device is null."));
        return false;
    }
    if (!Load())
    {
        return false;
    }
    if (bInitialized && InitializedDevice == Device)
    {
        return true;
    }

    if (bInitialized)
    {
        // Never tear down another device's in-flight features from an evaluation callback.
        SetError(TEXT("DLSS-NR device changed; restart the editor to reinitialize safely."));
        return false;
    }

    if (!bCallerGatePathReady)
    {
        SetError(FString::Printf(TEXT("DLSS-NR caller gate is not satisfied. Caller module path is: %s"), *CallerModulePath), DLSS5NRNGX::FailBase | 2u);
        return false;
    }

    if (!ResolveNGXCoreParameters())
    {
        return false;
    }

    const FString AppDataDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("DLSS5NR"));
    IFileManager::Get().MakeDirectory(*AppDataDir, true);

    const uint64 AppId = GetApplicationId();
    const uint32 ApiVersion = GetAPIVersion();

    // DLSS-NR is an NGX feature snippet, not the NGX core. Its Init_Ext expects the
    // core's populated parameter block. Calling the snippet with nullptr is rejected.
    const FNGXResult Result = InitExt(AppId, *AppDataDir, Device, ApiVersion, CoreParameters);
    LastNGXResult = Result;
    if (!DLSS5NRNGX::Succeeded(Result))
    {
        SetError(FString::Printf(TEXT("DLSS-NR snippet Init_Ext failed: 0x%08X"), Result), Result);
        return false;
    }

    const FNGXResult PopulateResult = PopulateParametersFn(CoreParameters);
    LastNGXResult = PopulateResult;
    if (!DLSS5NRNGX::Succeeded(PopulateResult))
    {
        SetError(FString::Printf(TEXT("DLSS-NR PopulateParameters_Impl failed: 0x%08X"), PopulateResult), PopulateResult);
        Shutdown1(Device);
        return false;
    }

    bSnippetPopulated = true;
    bInitialized = true;
    InitializedDevice = Device;
    LastError.Reset();
    UE_LOG(LogDLSS5, Display, TEXT("DLSS-NR snippet initialized through existing NGX core (API=0x%X, AppId=0x%llX)."), ApiVersion, AppId);
    return true;
}

FDLSS5NRRuntime::FFeatureState* FDLSS5NRRuntime::CreateFeature(const FDLSS5NREvaluateDesc& D)
{
    const FIntPoint OutputSize = D.OutputRect.Size();
    const bool bUseDepth = D.Depth != nullptr && D.DepthRect.Width() > 0 && D.DepthRect.Height() > 0;
    const bool bUseMotion = D.MotionVectors != nullptr && D.MotionRect.Width() > 0 && D.MotionRect.Height() > 0;
    const FIntPoint DepthSize = bUseDepth ? D.DepthRect.Size() : FIntPoint::ZeroValue;
    const FIntPoint MotionSize = bUseMotion ? D.MotionRect.Size() : FIntPoint::ZeroValue;

    if (!bInitialized || !bSnippetPopulated || !CoreParameters || !D.CommandList || OutputSize.X <= 0 || OutputSize.Y <= 0)
    {
        return nullptr;
    }

    // The NR snippet specializes internal resources around the active guide contract. Recreate
    // Feature 18 whenever guide availability/dimensions change instead of hot-swapping depth/MV
    // resources into a feature that was created in color-only mode.
    CollectCompletedFeatures(D.FrameNumber);
    FFeatureState* Existing = Features.Find(D.HistoryKey);
    const bool bSameContract = Existing && Existing->Handle &&
        Existing->Size == OutputSize &&
        Existing->Preset == D.Preset &&
        Existing->bUsesDepth == bUseDepth &&
        Existing->bUsesMotion == bUseMotion &&
        Existing->DepthSize == DepthSize &&
        Existing->MotionSize == MotionSize;

    if (bSameContract)
    {
        return Existing;
    }

    if (Existing)
    {
        // Recording another frame does not mean the GPU finished the previous one.
        RetiredFeatures.Add(MoveTemp(*Existing));
        Features.Remove(D.HistoryKey);
    }

    FNGXParameter* P = CoreParameters;
    if (!P)
    {
        return nullptr;
    }

    const auto SetRect = [P](const char* Prefix, const FIntRect& R)
    {
        const FString Base = UTF8_TO_TCHAR(Prefix);
        auto SetU = [P](const FString& Name, uint32 V)
        {
            FTCHARToUTF8 Utf8(*Name);
            P->Set(Utf8.Get(), V);
        };
        SetU(Base + TEXT("SubrectBaseX"), FMath::Max(0, R.Min.X));
        SetU(Base + TEXT("SubrectBaseY"), FMath::Max(0, R.Min.Y));
        SetU(Base + TEXT("SubrectWidth"), FMath::Max(0, R.Width()));
        SetU(Base + TEXT("SubrectHeight"), FMath::Max(0, R.Height()));
    };

    // Creation-time contract. RenoDX/NGX NR builds use these same keys when selecting the
    // network and allocating guide resources. Supplying them before CreateFeature avoids a
    // color-only feature being reused with a different D3D12 resource layout one frame later.
    P->Set("DLSSNR.Width", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.Height", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.InputWidth", static_cast<unsigned int>(D.ColorRect.Width()));
    P->Set("DLSSNR.InputHeight", static_cast<unsigned int>(D.ColorRect.Height()));
    P->Set("DLSSNR.OutputWidth", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.OutputHeight", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.Output.Width", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.Output.Height", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.Scale", 1.0f);
    P->Set("DLSSNR.ScalingRatio", 1.0f);
    P->Set("DLSSNR.Hint.Render.Preset", D.Preset);
    P->Set("CreationNodeMask", 1u);
    P->Set("VisibilityNodeMask", 1u);

    P->Set("DLSSNR.Color", D.Color);
    P->Set("DLSSNR.Output", D.Output);
    SetRect("DLSSNR.Color", D.ColorRect);
    SetRect("DLSSNR.Output", D.OutputRect);

    if (bUseDepth)
    {
        P->Set("DLSSNR.Depth", D.Depth);
        SetRect("DLSSNR.Depth", D.DepthRect);
    }
    else if (bParametersHadDepth)
    {
        // Clear a pointer left by the previous guide-enabled feature, but do not inject a new
        // null key into the proven color-only creation path.
        P->Set("DLSSNR.Depth", static_cast<ID3D12Resource*>(nullptr));
        SetRect("DLSSNR.Depth", FIntRect());
    }

    if (bUseMotion)
    {
        P->Set("DLSSNR.MVec", D.MotionVectors);
        SetRect("DLSSNR.MVec", D.MotionRect);
    }
    else if (bParametersHadMotion)
    {
        P->Set("DLSSNR.MVec", static_cast<ID3D12Resource*>(nullptr));
        SetRect("DLSSNR.MVec", FIntRect());
    }

    P->Set("DLSSNR.MVecScaleX", D.MotionScaleX);
    P->Set("DLSSNR.MVecScaleY", D.MotionScaleY);
    P->Set("DLSSNR.DepthInverted", D.bDepthInverted ? 1 : 0);
    P->Set("DLSSNR.Enabled", 1);
    P->Set("DLSSNR.Reset", 1);
    P->Set("Jitter.Offset.X", 0.0f);
    P->Set("Jitter.Offset.Y", 0.0f);
    bParametersHadDepth = bUseDepth;
    bParametersHadMotion = bUseMotion;

    FNGXHandle* NewHandle = nullptr;
    const FNGXResult Result = CreateFeatureFn(D.CommandList, DLSS5NRNGX::NeuralRenderingFeature, P, &NewHandle);
    LastNGXResult = Result;
    if (!DLSS5NRNGX::Succeeded(Result) || !NewHandle)
    {
        if (NewHandle)
        {
            FFeatureState FailedFeature;
            FailedFeature.Handle = NewHandle;
            FailedFeature.CompletionFence = D.CompletionFence;
            RetiredFeatures.Add(MoveTemp(FailedFeature));
        }
        SetError(FString::Printf(TEXT("DLSS-NR CreateFeature(18) failed at %dx%d preset=%d depth=%s(%dx%d) motion=%s(%dx%d): 0x%08X"),
            OutputSize.X, OutputSize.Y, D.Preset,
            bUseDepth ? TEXT("YES") : TEXT("NO"), DepthSize.X, DepthSize.Y,
            bUseMotion ? TEXT("YES") : TEXT("NO"), MotionSize.X, MotionSize.Y,
            Result), Result);
        return nullptr;
    }

    FFeatureState& Feature = Features.Add(D.HistoryKey);
    Feature.Handle = NewHandle;
    Feature.Size = OutputSize;
    Feature.Preset = D.Preset;
    Feature.bUsesDepth = bUseDepth;
    Feature.bUsesMotion = bUseMotion;
    Feature.DepthSize = DepthSize;
    Feature.MotionSize = MotionSize;
    Feature.CompletionFence = D.CompletionFence;
    Feature.LastFrame = D.FrameNumber;
    LastError.Reset();

    UE_LOG(LogDLSS5, Display,
        TEXT("Created NGX feature 18 for %dx%d preset=%d depth=%s(%dx%d) motion=%s(%dx%d) handle=%u."),
        OutputSize.X, OutputSize.Y, D.Preset,
        bUseDepth ? TEXT("YES") : TEXT("NO"), DepthSize.X, DepthSize.Y,
        bUseMotion ? TEXT("YES") : TEXT("NO"), MotionSize.X, MotionSize.Y,
        Feature.Handle->Id);
    return &Feature;
}

bool FDLSS5NRRuntime::Evaluate(const FDLSS5NREvaluateDesc& D)
{
    {
        FScopeLock Lock(&DiagnosticsMutex);
        ++EvaluateAttempts;
        bDepthUsedLastFrame = D.Depth != nullptr;
        bMotionUsedLastFrame = D.MotionVectors != nullptr;
        LastTelemetry.ColorRect = D.ColorRect;
        LastTelemetry.OutputRect = D.OutputRect;
        LastTelemetry.DepthRect = D.DepthRect;
        LastTelemetry.MotionRect = D.MotionRect;
        LastTelemetry.MotionScaleX = D.MotionScaleX;
        LastTelemetry.MotionScaleY = D.MotionScaleY;
        LastTelemetry.bReset = D.bReset;
        LastTelemetry.JitterPixels = D.JitterPixels;
        LastTelemetry.PreExposure = D.PreExposure;
        if (D.bReset)
        {
            ++ResetFrames;
        }
    }

    if (!D.CommandList || !D.Color || !D.Output || !D.CompletionFence.IsValid())
    {
        { FScopeLock Lock(&DiagnosticsMutex); ++FailedFrames; }
        SetError(TEXT("DLSS-NR Evaluate skipped: Color, Output or command list is null."));
        return false;
    }

    const FIntPoint OutputSize = D.OutputRect.Size();
    FFeatureState* Feature = CreateFeature(D);
    if (!Feature)
    {
        FScopeLock Lock(&DiagnosticsMutex);
        ++FailedFrames;
        return false;
    }
    const bool bReset = D.bReset || Feature->bNeedsReset ||
        D.FrameNumber > Feature->LastFrame + 1;
    Feature->LastFrame = D.FrameNumber;
    Feature->CompletionFence = D.CompletionFence;
    {
        FScopeLock Lock(&DiagnosticsMutex);
        FeatureSize = Feature->Size;
        LastTelemetry.bReset = bReset;
        if (bReset && !D.bReset) ++ResetFrames;
    }

    FNGXParameter* P = CoreParameters;
    if (!P)
    {
        { FScopeLock Lock(&DiagnosticsMutex); ++FailedFrames; }
        SetError(TEXT("DLSS-NR parameter block disappeared before Evaluate."));
        return false;
    }

    const auto SetRect = [P](const char* Prefix, const FIntRect& R)
    {
        const FString Base = UTF8_TO_TCHAR(Prefix);
        auto SetU = [P](const FString& Name, uint32 V)
        {
            FTCHARToUTF8 Utf8(*Name);
            P->Set(Utf8.Get(), V);
        };
        SetU(Base + TEXT("SubrectBaseX"), FMath::Max(0, R.Min.X));
        SetU(Base + TEXT("SubrectBaseY"), FMath::Max(0, R.Min.Y));
        SetU(Base + TEXT("SubrectWidth"), FMath::Max(0, R.Width()));
        SetU(Base + TEXT("SubrectHeight"), FMath::Max(0, R.Height()));
    };

    P->Set("DLSSNR.Color", D.Color);
    P->Set("DLSSNR.Output", D.Output);
    if (D.Depth || bParametersHadDepth) P->Set("DLSSNR.Depth", D.Depth);
    if (D.MotionVectors || bParametersHadMotion) P->Set("DLSSNR.MVec", D.MotionVectors);

    P->Set("DLSSNR.Width", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.Height", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.InputWidth", static_cast<unsigned int>(D.ColorRect.Width()));
    P->Set("DLSSNR.InputHeight", static_cast<unsigned int>(D.ColorRect.Height()));
    P->Set("DLSSNR.OutputWidth", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.OutputHeight", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.Output.Width", static_cast<unsigned int>(OutputSize.X));
    P->Set("DLSSNR.Output.Height", static_cast<unsigned int>(OutputSize.Y));
    P->Set("DLSSNR.Scale", 1.0f);
    P->Set("DLSSNR.ScalingRatio", 1.0f);

    SetRect("DLSSNR.Color", D.ColorRect);
    SetRect("DLSSNR.Output", D.OutputRect);
    if (D.Depth || bParametersHadDepth) SetRect("DLSSNR.Depth", D.DepthRect);
    if (D.MotionVectors || bParametersHadMotion) SetRect("DLSSNR.MVec", D.MotionRect);
    bParametersHadDepth = D.Depth != nullptr;
    bParametersHadMotion = D.MotionVectors != nullptr;

    P->Set("DLSSNR.MVecScaleX", D.MotionScaleX);
    P->Set("DLSSNR.MVecScaleY", D.MotionScaleY);
    P->Set("DLSSNR.DepthInverted", D.bDepthInverted ? 1 : 0);
    P->Set("DLSSNR.Enabled", 1);
    P->Set("DLSSNR.Reset", bReset ? 1 : 0);

    // The post-tonemap input is already temporally reconstructed. Applying the original
    // camera jitter here would move an otherwise stationary image every frame.
    P->Set("Jitter.Offset.X", 0.0f);
    P->Set("Jitter.Offset.Y", 0.0f);

    P->Set("DLSSNR.Intensity", D.Intensity);
    P->Set("DLSSNR.LocalToneStrength", D.LocalTone);
    P->Set("DLSSNR.LocalStructureStrength", D.LocalStructure);
    P->Set("DLSSNR.SkinStructureStrength", D.SkinStructure);
    P->Set("DLSSNR.UseAutoMask", D.bAutoMask ? 1 : 0);
    P->Set("DLSSNR.Style", D.Style);
    P->Set("DLSSNR.UICorrection", D.bUICorrection ? 1 : 0);

    const FNGXResult Result = EvaluateFeatureFn(D.CommandList, Feature->Handle, P, nullptr);
    LastNGXResult = Result;
    if (!DLSS5NRNGX::Succeeded(Result))
    {
        Feature->bNeedsReset = true;
        { FScopeLock Lock(&DiagnosticsMutex); ++FailedFrames; }
        SetError(FString::Printf(TEXT("DLSS-NR EvaluateFeature failed: 0x%08X"), Result), Result);
        return false;
    }
    Feature->bNeedsReset = false;

    {
        FScopeLock Lock(&DiagnosticsMutex);
        ++SuccessfulFrames;
    }
    LastError.Reset();
    return true;
}

void FDLSS5NRRuntime::UpdateFrameTelemetry(const FDLSS5NRFrameTelemetry& Telemetry)
{
    FScopeLock Lock(&DiagnosticsMutex);
    LastTelemetry = Telemetry;
}

FDLSS5NRDiagnosticsSnapshot FDLSS5NRRuntime::GetDiagnosticsSnapshot() const
{
    FScopeLock ExecutionLock(&ExecutionMutex);
    FScopeLock Lock(&DiagnosticsMutex);

    FDLSS5NRDiagnosticsSnapshot Out;
    Out.bRuntimeLoaded = DllHandle != nullptr;
    Out.bRuntimeInitialized = bInitialized;
    Out.bCoreFound = CoreModuleHandle != nullptr;
    Out.bCoreParameters = CoreParameters != nullptr;
    Out.bSnippetPopulated = bSnippetPopulated;
    Out.bCallerGateReady = bCallerGatePathReady;
    Out.EvaluateAttempts = EvaluateAttempts;
    Out.SuccessfulFrames = SuccessfulFrames;
    Out.FailedFrames = FailedFrames;
    Out.ResetFrames = ResetFrames;
    Out.bDepthAvailable = LastTelemetry.bDepthAvailable;
    Out.bMotionAvailable = LastTelemetry.bMotionAvailable;
    Out.bDepthRequested = LastTelemetry.bDepthRequested;
    Out.bMotionRequested = LastTelemetry.bMotionRequested;
    Out.bDepthUsedLastFrame = bDepthUsedLastFrame;
    Out.bMotionUsedLastFrame = bMotionUsedLastFrame;
    Out.bResetLastFrame = LastTelemetry.bReset;
    Out.bCameraCutLastFrame = LastTelemetry.bCameraCut;
    Out.FeatureSize = FeatureSize;
    Out.SceneDepthExtent = LastTelemetry.SceneDepthExtent;
    Out.SceneVelocityExtent = LastTelemetry.SceneVelocityExtent;
    Out.ColorRect = LastTelemetry.ColorRect;
    Out.DepthRect = LastTelemetry.DepthRect;
    Out.MotionRect = LastTelemetry.MotionRect;
    Out.OutputRect = LastTelemetry.OutputRect;
    Out.JitterPixels = LastTelemetry.JitterPixels;
    Out.PreExposure = LastTelemetry.PreExposure;
    Out.MotionScaleX = LastTelemetry.MotionScaleX;
    Out.MotionScaleY = LastTelemetry.MotionScaleY;
    Out.LastNGXResult = LastNGXResult;
    Out.LastError = LastError;
    Out.RuntimePath = RuntimePath;
    Out.CallerModulePath = CallerModulePath;
    Out.LastResetReason = LastTelemetry.ResetReason;
    return Out;
}

void FDLSS5NRRuntime::DestroyFeature(FFeatureState& Feature)
{
    if (Feature.Handle && ReleaseFeatureFn)
    {
        const FNGXResult Result = ReleaseFeatureFn(Feature.Handle);
        LastNGXResult = Result;
        if (!DLSS5NRNGX::Succeeded(Result))
        {
            UE_LOG(LogDLSS5, Warning, TEXT("DLSS-NR ReleaseFeature returned 0x%08X"), Result);
        }
        Feature.Handle = nullptr;
    }
}

void FDLSS5NRRuntime::CollectCompletedFeatures(uint64 FrameNumber)
{
    // Retire closed views and unused passes as well as resized/reconfigured features.
    for (auto It = Features.CreateIterator(); It; ++It)
    {
        if (FrameNumber > It.Value().LastFrame + 120)
        {
            RetiredFeatures.Add(MoveTemp(It.Value()));
            It.RemoveCurrent();
        }
    }
    for (int32 Index = RetiredFeatures.Num() - 1; Index >= 0; --Index)
    {
        FFeatureState& Feature = RetiredFeatures[Index];
        if (Feature.CompletionFence.IsValid() &&
            Feature.CompletionFence->NumPendingWriteCommands.GetValue() == 0 &&
            Feature.CompletionFence->Poll())
        {
            DestroyFeature(Feature);
            RetiredFeatures.RemoveAtSwap(Index);
        }
    }
}

void FDLSS5NRRuntime::ReleaseAllFeaturesAfterGPUIdle()
{
    for (auto& Entry : Features) DestroyFeature(Entry.Value);
    for (FFeatureState& Feature : RetiredFeatures) DestroyFeature(Feature);
    Features.Empty();
    RetiredFeatures.Empty();
    FeatureSize = FIntPoint::ZeroValue;
}

void FDLSS5NRRuntime::Unload()
{
    // ShutdownModule drains the GPU before reaching this point. Load failure has no features.
    check(Features.IsEmpty() && RetiredFeatures.IsEmpty());

    if (bInitialized && Shutdown1 && InitializedDevice)
    {
        Shutdown1(InitializedDevice);
    }
    bInitialized = false;
    bSnippetPopulated = false;
    InitializedDevice = nullptr;

    // GetCapabilityParameters() returns an owned block in the public NGX API.
    // Destroy only blocks acquired through that path, and only while the core is still loaded.
    if (CoreParameters && bOwnCoreParameters && CoreDestroyParameters && CoreModuleHandle)
    {
        CoreDestroyParameters(CoreParameters);
    }
    CoreParameters = nullptr;
    bOwnCoreParameters = false;
    CoreModuleHandle = nullptr;
    CoreGetCapabilityParameters = nullptr;
    CoreGetParameters = nullptr;
    CoreDestroyParameters = nullptr;

    if (DllHandle)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }

    InitExt = nullptr;
    Shutdown1 = nullptr;
    PopulateParametersFn = nullptr;
    CreateFeatureFn = nullptr;
    EvaluateFeatureFn = nullptr;
    ReleaseFeatureFn = nullptr;
    GetAPIVersion = nullptr;
    GetApplicationId = nullptr;

    {
        FScopeLock Lock(&DiagnosticsMutex);
        LastTelemetry = FDLSS5NRFrameTelemetry();
        EvaluateAttempts = 0;
        FailedFrames = 0;
        ResetFrames = 0;
        bDepthUsedLastFrame = false;
        bMotionUsedLastFrame = false;
        SuccessfulFrames = 0;
    }
}
