#include "DLSS5NRSettings.h"

#include "DLSS5NRRuntime.h"
#include "HAL/IConsoleManager.h"

#define LOCTEXT_NAMESPACE "DLSS5NRSettings"

namespace
{
    void SetIntCVar(const TCHAR* Name, int32 Value)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            CVar->Set(Value, ECVF_SetByProjectSetting);
        }
    }

    void SetFloatCVar(const TCHAR* Name, float Value)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            CVar->Set(Value, ECVF_SetByProjectSetting);
        }
    }

    FString RectToString(const FIntRect& Rect)
    {
        return FString::Printf(TEXT("(%d,%d) %dx%d"), Rect.Min.X, Rect.Min.Y, Rect.Width(), Rect.Height());
    }
}

UDLSS5NRSettings::UDLSS5NRSettings()
{
    DiagnosticRuntime = TEXT("Waiting for renderer...");
    DiagnosticNGX = TEXT("Waiting for renderer...");
    DiagnosticGuides = TEXT("Waiting for renderer...");
    DiagnosticSubrects = TEXT("Waiting for renderer...");
    DiagnosticTemporal = TEXT("Waiting for renderer...");
    DiagnosticCounters = TEXT("0 attempts / 0 successful / 0 failed");
    DiagnosticLastResult = TEXT("No evaluation yet");
}

void UDLSS5NRSettings::ApplyToCVars() const
{
    SetIntCVar(TEXT("r.DLSS5.Enable"), bEnableNeuralRendering ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.Style"), static_cast<int32>(Style));
    SetIntCVar(TEXT("r.DLSS5.Preset"), FMath::Clamp(Preset, 0, 3));
    SetIntCVar(TEXT("r.DLSS5.PassCount"), FMath::Clamp(PassCount, 1, 4));
    SetFloatCVar(TEXT("r.DLSS5.Intensity"), Intensity);
    SetFloatCVar(TEXT("r.DLSS5.LocalTone"), LocalTone);
    SetFloatCVar(TEXT("r.DLSS5.LocalStructure"), LocalStructure);
    SetFloatCVar(TEXT("r.DLSS5.SkinStructure"), SkinStructure);
    SetIntCVar(TEXT("r.DLSS5.AutoMask"), bAutoMask ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.UICorrection"), bUICorrection ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.EnableSceneGuides"), bEnableSceneGuides ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.FeedSceneGuides"), bFeedSceneGuides ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.UseDepth"), bUseDepth ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.UseMotionVectors"), bUseMotionVectors ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.AutoResetHistory"), bAutoResetHistory ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.DepthInverted"), bDepthInverted ? 1 : 0);
    SetFloatCVar(TEXT("r.DLSS5.MVecScaleX"), MotionScaleXMultiplier);
    SetFloatCVar(TEXT("r.DLSS5.MVecScaleY"), MotionScaleYMultiplier);
    SetFloatCVar(TEXT("r.DLSS5.ExposureResetRatio"), ExposureResetRatio);
    SetIntCVar(TEXT("r.DLSS5.Debug"), bVerboseDiagnostics ? 1 : 0);
}

#if WITH_EDITOR
void UDLSS5NRSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    ApplyToCVars();
    SaveConfig();
}
#endif

void UDLSS5NRSettings::RefreshDiagnostics()
{
    const FDLSS5NRDiagnosticsSnapshot D = FDLSS5NRRuntime::Get().GetDiagnosticsSnapshot();

    DiagnosticRuntime = FString::Printf(
        TEXT("Runtime %s | Core %s | Core params %s | Caller gate %s"),
        D.bRuntimeLoaded ? TEXT("READY") : TEXT("NOT LOADED"),
        D.bCoreFound ? TEXT("READY") : TEXT("WAITING"),
        D.bCoreParameters ? TEXT("READY") : TEXT("WAITING"),
        D.bCallerGateReady ? TEXT("READY") : TEXT("FAILED"));

    DiagnosticNGX = FString::Printf(
        TEXT("Initialized %s | Snippet %s | Feature %dx%d | Passes %d"),
        D.bRuntimeInitialized ? TEXT("YES") : TEXT("NO"),
        D.bSnippetPopulated ? TEXT("YES") : TEXT("NO"),
        D.FeatureSize.X, D.FeatureSize.Y,
        FMath::Clamp(PassCount, 1, 4));

    DiagnosticGuides = FString::Printf(
        TEXT("Depth: requested=%s available=%s fed=%s (%dx%d) | Motion: requested=%s available=%s fed=%s (%dx%d) | scale=(%.6f, %.6f)"),
        D.bDepthRequested ? TEXT("YES") : TEXT("NO"),
        D.bDepthAvailable ? TEXT("YES") : TEXT("NO"),
        D.bDepthUsedLastFrame ? TEXT("YES") : TEXT("NO"),
        D.SceneDepthExtent.X, D.SceneDepthExtent.Y,
        D.bMotionRequested ? TEXT("YES") : TEXT("NO"),
        D.bMotionAvailable ? TEXT("YES") : TEXT("NO"),
        D.bMotionUsedLastFrame ? TEXT("YES") : TEXT("NO"),
        D.SceneVelocityExtent.X, D.SceneVelocityExtent.Y,
        D.MotionScaleX, D.MotionScaleY);

    DiagnosticSubrects = FString::Printf(
        TEXT("Color %s | Depth %s | MVec %s | Output %s"),
        *RectToString(D.ColorRect), *RectToString(D.DepthRect), *RectToString(D.MotionRect), *RectToString(D.OutputRect));

    DiagnosticTemporal = FString::Printf(
        TEXT("Jitter=(%.4f, %.4f) px | PreExposure=%.4f | Reset=%s | CameraCut=%s | %s"),
        D.JitterPixels.X, D.JitterPixels.Y, D.PreExposure,
        D.bResetLastFrame ? TEXT("YES") : TEXT("NO"),
        D.bCameraCutLastFrame ? TEXT("YES") : TEXT("NO"),
        D.LastResetReason.IsEmpty() ? TEXT("history continuous") : *D.LastResetReason);

    DiagnosticCounters = FString::Printf(
        TEXT("%llu attempts | %llu successful | %llu failed | %llu resets"),
        D.EvaluateAttempts, D.SuccessfulFrames, D.FailedFrames, D.ResetFrames);

    DiagnosticLastResult = FString::Printf(TEXT("0x%08X%s%s"), D.LastNGXResult,
        D.LastError.IsEmpty() ? TEXT("") : TEXT(" | "),
        D.LastError.IsEmpty() ? TEXT("") : *D.LastError);
}

#undef LOCTEXT_NAMESPACE
