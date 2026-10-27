#include "DLSS5ForUE5Module.h"

#include "DLSS5NRRuntime.h"
#include "DLSS5NRSettings.h"
#include "DLSS5NRCVars.h"
#include "DLSS5NRViewExtension.h"
#include "DLSS5NRRecovery.h"
#include "HAL/IConsoleManager.h"

#include "Interfaces/IPluginManager.h"
#include "Misc/CoreDelegates.h"
#include "Misc/Paths.h"
#include "Runtime/Launch/Resources/Version.h"
#include "RenderingThread.h"
#include "SceneViewExtension.h"
#include "ShaderCore.h"
#include "RHICommandList.h"

DEFINE_LOG_CATEGORY(LogDLSS5);

FString DLSS5GetVersion()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DLSS5ForUE5"));
    return Plugin ? Plugin->GetDescriptor().VersionName : TEXT("unknown");
}


namespace
{
    FString RectString(const FIntRect& Rect)
    {
        return FString::Printf(TEXT("(%d,%d) %dx%d"), Rect.Min.X, Rect.Min.Y, Rect.Width(), Rect.Height());
    }

    void PrintStatus()
    {
        const FDLSS5NRDiagnosticsSnapshot D = FDLSS5NRRuntime::Get().GetDiagnosticsSnapshot();
        const TSharedPtr<IPlugin> NvidiaDLSS = IPluginManager::Get().FindPlugin(TEXT("DLSS"));
        const bool bDLSSInstalled = NvidiaDLSS.IsValid();
        const bool bDLSSEnabled = bDLSSInstalled && NvidiaDLSS->IsEnabled();

        UE_LOG(LogDLSS5, Display, TEXT("================ DLSS5 %s Status ================"), *DLSS5GetVersion());
        UE_LOG(LogDLSS5, Display, TEXT("NVIDIA DLSS plugin found/enabled: %s / %s"),
            bDLSSInstalled ? TEXT("YES") : TEXT("NO"), bDLSSEnabled ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogDLSS5, Display, TEXT("NR runtime loaded/initialized: %s / %s"),
            D.bRuntimeLoaded ? TEXT("YES") : TEXT("NO"), D.bRuntimeInitialized ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogDLSS5, Display, TEXT("NGX core / params / snippet / legacy filename hint (diagnostic only): %s / %s / %s / %s"),
            D.bCoreFound ? TEXT("YES") : TEXT("NO"),
            D.bCoreParameters ? TEXT("YES") : TEXT("NO"),
            D.bSnippetPopulated ? TEXT("YES") : TEXT("NO"),
            D.bCallerGateReady ? TEXT("YES") : TEXT("NO"));
        UE_LOG(LogDLSS5, Display, TEXT("Attempts / successful / failed / resets: %llu / %llu / %llu / %llu"),
            D.EvaluateAttempts, D.SuccessfulFrames, D.FailedFrames, D.ResetFrames);
        UE_LOG(LogDLSS5, Display, TEXT("Feature size: %dx%d | Last NGX result: 0x%08X"),
            D.FeatureSize.X, D.FeatureSize.Y, D.LastNGXResult);
        for (const TCHAR* Name : {TEXT("r.DLSS5.Enable"), TEXT("r.DLSS5.PassCount"), TEXT("r.DLSS5.Preset"),
            TEXT("r.DLSS5.Style"), TEXT("r.DLSS5.Intensity"), TEXT("r.DLSS5.EnableSceneGuides"),
            TEXT("r.DLSS5.FeedSceneGuides"), TEXT("r.DLSS5.UseMotionVectors")})
        {
            if (const IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name))
                UE_LOG(LogDLSS5, Display, TEXT("Active %s=%s"), Name, *V->GetString());
        }
        UE_LOG(LogDLSS5, Display, TEXT("Depth (UE scene depth / DLSS host input) requested/available/used: %s / %s / %s | extent %dx%d"),
            D.bDepthRequested ? TEXT("YES") : TEXT("NO"),
            D.bDepthAvailable ? TEXT("YES") : TEXT("NO"),
            D.bDepthUsedLastFrame ? TEXT("YES") : TEXT("NO"),
            D.SceneDepthExtent.X, D.SceneDepthExtent.Y);
        UE_LOG(LogDLSS5, Display, TEXT("Motion requested/available/used: %s / %s / %s | source extent %dx%d | scale %.8f, %.8f"),
            D.bMotionRequested ? TEXT("YES") : TEXT("NO"),
            D.bMotionAvailable ? TEXT("YES") : TEXT("NO"),
            D.bMotionUsedLastFrame ? TEXT("YES") : TEXT("NO"),
            D.SceneVelocityExtent.X, D.SceneVelocityExtent.Y,
            D.MotionScaleX, D.MotionScaleY);
        UE_LOG(LogDLSS5, Display, TEXT("Rects | Color %s | Depth %s | MVec %s | Output %s"),
            *RectString(D.ColorRect), *RectString(D.DepthRect), *RectString(D.MotionRect), *RectString(D.OutputRect));
        UE_LOG(LogDLSS5, Display, TEXT("Temporal | Jitter %.4f, %.4f px | PreExposure %.5f | Reset %s | CameraCut %s"),
            D.JitterPixels.X, D.JitterPixels.Y, D.PreExposure,
            D.bResetLastFrame ? TEXT("YES") : TEXT("NO"), D.bCameraCutLastFrame ? TEXT("YES") : TEXT("NO"));
        if (!D.LastResetReason.IsEmpty())
        {
            UE_LOG(LogDLSS5, Display, TEXT("Last reset reason: %s"), *D.LastResetReason);
        }
        UE_LOG(LogDLSS5, Display, TEXT("Runtime: %s"), *D.RuntimePath);
        UE_LOG(LogDLSS5, Display, TEXT("Caller module: %s"), *D.CallerModulePath);
        if (!D.LastError.IsEmpty())
        {
            UE_LOG(LogDLSS5, Display, TEXT("Last error: %s"), *D.LastError);
        }
        UE_LOG(LogDLSS5, Display, TEXT("======================================================"));
    }

    FAutoConsoleCommand GDLSS5NRStatusCommand(
        TEXT("DLSS5.Status"),
        TEXT("Print DLSS5 runtime/guide diagnostics and active CVars."),
        FConsoleCommandDelegate::CreateStatic(&PrintStatus));

    void EnterSafeModeFromConsole()
    {
        if (UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>())
        {
            Settings->bEnableNeuralRendering = false;
            Settings->bEnableSceneGuides = false;
            Settings->bFeedSceneGuides = false;
            Settings->bUseMotionVectors = false;
            Settings->ApplyToCVars();
            Settings->RefreshDiagnostics();
            UE_LOG(LogDLSS5, Display, TEXT("DLSS5 Safe Mode enabled."));
        }
    }

    FAutoConsoleCommand GDLSS5SafeModeCommand(
        TEXT("DLSS5.SafeMode"),
        TEXT("Disable Neural Rendering and experimental scene-guide execution, for this process (does not write configuration)."),
        FConsoleCommandDelegate::CreateStatic(&EnterSafeModeFromConsole));
}

void FDLSS5ForUE5Module::StartupModule()
{
    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("DLSS5ForUE5"));
    if (!Plugin.IsValid())
    {
        UE_LOG(LogDLSS5, Error, TEXT("Could not resolve DLSS5ForUE5 plugin directory."));
        return;
    }

    // Global shaders must be mapped before engine shader compilation begins. This is the
    // reason the v0.4 runtime module loads at PostConfigInit rather than PostEngineInit.
    const FString ShaderDir = FPaths::Combine(Plugin->GetBaseDir(), TEXT("Shaders"));
    AddShaderSourceDirectoryMapping(TEXT("/DLSS5NR"), ShaderDir);
    UE_LOG(LogDLSS5, Log, TEXT("Mapped DLSS5ForUE5 shader directory: %s"), *ShaderDir);

#if !PLATFORM_WINDOWS
    UE_LOG(LogDLSS5, Warning, TEXT("DLSS5ForUE5 is currently Win64/D3D12 only."));
    return;
#else
    // Loading the snippet DLL is safe here. The NVIDIA driver NGX core is intentionally
    // acquired lazily on the first render evaluation, after UE/NVIDIA have initialized it.
    if (!IsRunningCommandlet() && !FDLSS5NRRuntime::Get().Load())
    {
        UE_LOG(LogDLSS5, Error, TEXT("DLSS-NR runtime load failed. Renderer will remain pass-through."));
    }

#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
    // UE 5.8 deprecated direct access to OnPostEngineInit. Use the accessor on 5.8+
    // while keeping the older API for UE 5.5-5.7 compatibility.
    PostEngineInitHandle = FCoreDelegates::GetOnPostEngineInit().AddRaw(this, &FDLSS5ForUE5Module::HandlePostEngineInit);
#else
    PostEngineInitHandle = FCoreDelegates::OnPostEngineInit.AddRaw(this, &FDLSS5ForUE5Module::HandlePostEngineInit);
#endif
    UE_LOG(LogDLSS5, Display, TEXT("DLSS 5 for UE5 %s bootstrap loaded."), *DLSS5GetVersion());
#endif
}

void FDLSS5ForUE5Module::HandlePostEngineInit()
{
#if PLATFORM_WINDOWS
    // Cooking needs shader registration, but must not initialize a neural runtime or
    // change game-session recovery state in the cooker process.
    if (IsRunningCommandlet()) return;
    UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>();
    if (Settings)
    {
        const bool bRecover = DLSS5NRRecovery::Begin(RecoveryMarker);
        if (bRecover)
        {
            for (const TCHAR* Name : { TEXT("r.DLSS5.Enable"), TEXT("r.DLSS5.EnableSceneGuides"),
                TEXT("r.DLSS5.FeedSceneGuides"), TEXT("r.DLSS5.UseMotionVectors") })
                if (IConsoleVariable* V = IConsoleManager::Get().FindConsoleVariable(Name)) V->Set(0, ECVF_SetByConsole);
            UE_LOG(LogDLSS5, Warning, TEXT("NR recovery: a recent NR-active session ended uncleanly. NR/guides are disabled for this process; use r.DLSS5.Enable to re-enable."));
        }
        Settings->LoadFromCVars();
        DLSS5NRRecovery::Update(RecoveryMarker);
        Settings->RefreshDiagnostics();

        UE_LOG(LogDLSS5, Display, TEXT("Launch CVars respected. NR enabled: %d; recovery: %s"), Settings->bEnableNeuralRendering ? 1 : 0, bRecover ? TEXT("YES") : TEXT("NO"));
    }

    ViewExtension = FSceneViewExtensions::NewExtension<FDLSS5NRViewExtension>();

    DiagnosticsTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FDLSS5ForUE5Module::TickDiagnostics), 0.5f);

    const TSharedPtr<IPlugin> NvidiaDLSS = IPluginManager::Get().FindPlugin(TEXT("DLSS"));
    UE_LOG(LogDLSS5, Display, TEXT("DLSS 5 for UE5 %s renderer ready. NVIDIA DLSS plugin: %s. UI is editor-only; runtime control: r.DLSS5.Enable."), *DLSS5GetVersion(),
        (NvidiaDLSS.IsValid() && NvidiaDLSS->IsEnabled()) ? TEXT("enabled") : TEXT("not enabled / NGX core may be unavailable"));
#endif
}

bool FDLSS5ForUE5Module::TickDiagnostics(float DeltaTime)
{
    DLSS5NRRecovery::Update(RecoveryMarker);
    if (UDLSS5NRSettings* Settings = GetMutableDefault<UDLSS5NRSettings>())
    {
        Settings->RefreshDiagnostics();
    }
    return true;
}

void FDLSS5ForUE5Module::ShutdownModule()
{
    if (PostEngineInitHandle.IsValid())
    {
#if ENGINE_MAJOR_VERSION > 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 8)
        FCoreDelegates::GetOnPostEngineInit().Remove(PostEngineInitHandle);
#else
        FCoreDelegates::OnPostEngineInit.Remove(PostEngineInitHandle);
#endif
        PostEngineInitHandle.Reset();
    }

    if (DiagnosticsTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(DiagnosticsTickerHandle);
        DiagnosticsTickerHandle.Reset();
    }

    ViewExtension.Reset();

    // Flushing the CPU render thread alone does not finish GPU work.
    if (GDynamicRHI)
    {
        ENQUEUE_RENDER_COMMAND(DLSS5NRDrainGPU)([](FRHICommandListImmediate& RHICmdList)
        {
            RHICmdList.SubmitAndBlockUntilGPUIdle();
            FDLSS5NRRuntime::Get().ReleaseAllFeaturesAfterGPUIdle();
        });
    }
    FlushRenderingCommands();
    FDLSS5NRRuntime::Get().Unload();
    DLSS5NRRecovery::End(RecoveryMarker);
}

IMPLEMENT_MODULE(FDLSS5ForUE5Module, DLSS5ForUE5_nvngx)
