#include "DLSS5NRSettings.h"

#include "DLSS5NRRuntime.h"
#include "Engine/Texture.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectGlobals.h"

#define LOCTEXT_NAMESPACE "DLSS5NRSettings"

namespace
{
    void SetIntCVar(const TCHAR* Name, int32 Value)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            // Console priority so window edits win over ini and earlier console sets.
            CVar->Set(Value, ECVF_SetByConsole);
        }
    }

    void SetFloatCVar(const TCHAR* Name, float Value)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            CVar->Set(Value, ECVF_SetByConsole);
        }
    }

    void SetStringCVar(const TCHAR* Name, const FString& Value)
    {
        if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
        {
            CVar->Set(*Value, ECVF_SetByConsole);
        }
    }

    FString GetStringCVar(const TCHAR* Name)
    {
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
        return CVar ? CVar->GetString() : FString();
    }

    int32 GetIntCVar(const TCHAR* Name, int32 Default)
    {
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
        return CVar ? CVar->GetInt() : Default;
    }

    float GetFloatCVar(const TCHAR* Name, float Default)
    {
        const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name);
        return CVar ? CVar->GetFloat() : Default;
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
    SetIntCVar(TEXT("r.DLSS5.ControlMaskStencil"), FMath::Clamp(ControlMaskStencil, -1, 255));
    SetIntCVar(TEXT("r.DLSS5.ControlMaskInvert"), bControlMaskInvert ? 1 : 0);
    SetStringCVar(TEXT("r.DLSS5.DistortionFieldTexture"), DistortionFieldTexture.IsNull() ? FString() : DistortionFieldTexture.ToString());
    SetFloatCVar(TEXT("r.DLSS5.ScalingRatio"), FMath::Clamp(ScalingRatio, 0.25f, 1.0f));
    SetIntCVar(TEXT("r.DLSS5.ScalingRatioMode"), FMath::Clamp(ScalingRatioMode, 0, 1));
    SetIntCVar(TEXT("r.DLSS5.AutoResetHistory"), bAutoResetHistory ? 1 : 0);
    SetIntCVar(TEXT("r.DLSS5.DepthInverted"), bDepthInverted ? 1 : 0);
    SetFloatCVar(TEXT("r.DLSS5.MVecScaleX"), MotionScaleXMultiplier);
    SetFloatCVar(TEXT("r.DLSS5.MVecScaleY"), MotionScaleYMultiplier);
    SetFloatCVar(TEXT("r.DLSS5.ExposureResetRatio"), ExposureResetRatio);
    SetFloatCVar(TEXT("r.DLSS5.HighlightPreserveStart"), HighlightPreserveStart);
    SetFloatCVar(TEXT("r.DLSS5.HighlightPreserveEnd"), HighlightPreserveEnd);
    SetIntCVar(TEXT("r.DLSS5.Debug"), bVerboseDiagnostics ? 1 : 0);
}

void UDLSS5NRSettings::LoadFromCVars()
{
    bEnableNeuralRendering = GetIntCVar(TEXT("r.DLSS5.Enable"), 0) != 0;
    Style = static_cast<EDLSS5NRStyle>(FMath::Clamp(GetIntCVar(TEXT("r.DLSS5.Style"), 0), 0, 2));
    Preset = FMath::Clamp(GetIntCVar(TEXT("r.DLSS5.Preset"), 1), 0, 3);
    PassCount = FMath::Clamp(GetIntCVar(TEXT("r.DLSS5.PassCount"), 1), 1, 4);
    Intensity = GetFloatCVar(TEXT("r.DLSS5.Intensity"), 1.0f);
    LocalTone = GetFloatCVar(TEXT("r.DLSS5.LocalTone"), 1.0f);
    LocalStructure = GetFloatCVar(TEXT("r.DLSS5.LocalStructure"), 1.0f);
    SkinStructure = GetFloatCVar(TEXT("r.DLSS5.SkinStructure"), 1.0f);
    bAutoMask = GetIntCVar(TEXT("r.DLSS5.AutoMask"), 1) != 0;
    bUICorrection = GetIntCVar(TEXT("r.DLSS5.UICorrection"), 0) != 0;
    bEnableSceneGuides = GetIntCVar(TEXT("r.DLSS5.EnableSceneGuides"), 0) != 0;
    bFeedSceneGuides = GetIntCVar(TEXT("r.DLSS5.FeedSceneGuides"), 0) != 0;
    bUseDepth = GetIntCVar(TEXT("r.DLSS5.UseDepth"), 1) != 0;
    bUseMotionVectors = GetIntCVar(TEXT("r.DLSS5.UseMotionVectors"), 0) != 0;
    ControlMaskStencil = FMath::Clamp(GetIntCVar(TEXT("r.DLSS5.ControlMaskStencil"), 0), -1, 255);
    bControlMaskInvert = GetIntCVar(TEXT("r.DLSS5.ControlMaskInvert"), 0) != 0;
    const FString FieldPath = GetStringCVar(TEXT("r.DLSS5.DistortionFieldTexture"));
    DistortionFieldTexture = FieldPath.IsEmpty() ? TSoftObjectPtr<UTexture>() : TSoftObjectPtr<UTexture>(FSoftObjectPath(FieldPath));
    if (!DistortionFieldTexture.IsNull())
    {
        DistortionFieldTexture.LoadSynchronous();
    }
    ScalingRatio = FMath::Clamp(GetFloatCVar(TEXT("r.DLSS5.ScalingRatio"), 1.0f), 0.25f, 1.0f);
    ScalingRatioMode = FMath::Clamp(GetIntCVar(TEXT("r.DLSS5.ScalingRatioMode"), 0), 0, 1);
    bAutoResetHistory = GetIntCVar(TEXT("r.DLSS5.AutoResetHistory"), 1) != 0;
    bDepthInverted = GetIntCVar(TEXT("r.DLSS5.DepthInverted"), 1) != 0;
    MotionScaleXMultiplier = GetFloatCVar(TEXT("r.DLSS5.MVecScaleX"), 1.0f);
    MotionScaleYMultiplier = GetFloatCVar(TEXT("r.DLSS5.MVecScaleY"), 1.0f);
    ExposureResetRatio = GetFloatCVar(TEXT("r.DLSS5.ExposureResetRatio"), 2.0f);
    HighlightPreserveStart = GetFloatCVar(TEXT("r.DLSS5.HighlightPreserveStart"), 0.7f);
    HighlightPreserveEnd = GetFloatCVar(TEXT("r.DLSS5.HighlightPreserveEnd"), 0.9f);
    bVerboseDiagnostics = GetIntCVar(TEXT("r.DLSS5.Debug"), 1) != 0;
}

#if WITH_EDITOR
void UDLSS5NRSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
    Super::PostEditChangeProperty(PropertyChangedEvent);
    ApplyToCVars();
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
