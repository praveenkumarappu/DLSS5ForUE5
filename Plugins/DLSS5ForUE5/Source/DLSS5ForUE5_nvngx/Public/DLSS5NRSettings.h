#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DLSS5NRSettings.generated.h"

UENUM()
enum class EDLSS5NRStyle : uint8
{
    Natural UMETA(DisplayName = "Natural"),
    Cinematic UMETA(DisplayName = "Cinematic")
};

/**
 * Artist-facing settings model for DLSS 5 for UE5.
 *
 * The render thread still consumes console variables internally because they are cheap and
 * thread-safe. The dedicated editor window edits this config object and synchronizes the render-thread CVars.
 */
UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "DLSS 5 for UE5"))
class DLSS5FORUE5_NVNGX_API UDLSS5NRSettings : public UObject
{
    GENERATED_BODY()

public:
    UDLSS5NRSettings();

#if WITH_EDITOR
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

    void ApplyToCVars() const;
    void RefreshDiagnostics();

    // ---------------- General ----------------
    UPROPERTY(EditAnywhere, Config, Category = "General", meta = (DisplayName = "Enable Neural Rendering"))
    bool bEnableNeuralRendering = false;

    UPROPERTY(EditAnywhere, Config, Category = "General")
    EDLSS5NRStyle Style = EDLSS5NRStyle::Natural;

    UPROPERTY(EditAnywhere, Config, Category = "General", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3", DisplayName = "NR Preset"))
    int32 Preset = 1;

    // Edited by the dedicated Slate slider in the DLSS 5 window rather than the generic DetailsView.
    UPROPERTY(Config)
    int32 PassCount = 1;

    // ---------------- Neural Controls ----------------
    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0"))
    float Intensity = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "Local Tone"))
    float LocalTone = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "Local Structure"))
    float LocalStructure = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "2.0", UIMin = "0.0", UIMax = "2.0", DisplayName = "Skin Structure"))
    float SkinStructure = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (DisplayName = "Automatic Mask"))
    bool bAutoMask = true;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (DisplayName = "UI Correction"))
    bool bUICorrection = false;

    // ---------------- Scene Guides ----------------
    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (DisplayName = "Enable 3D Scene Guides (Experimental)", ToolTip = "Master safety switch for depth and motion-vector guides. Off keeps the proven v0.3 color-only path. This is forced OFF on every editor startup for crash-safe recovery. Enable only after the editor is fully running."))
    bool bEnableSceneGuides = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Feed Scene Guides Into NR (Experimental)", ToolTip = "Safe staging switch. Off generates/diagnoses guide textures without handing them to NGX. Turn this on only after guide generation is stable."))
    bool bFeedSceneGuides = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Use DLSS Input Depth (UE Scene Depth)", ToolTip = "Uses Unreal Engine scene depth, which is the host depth source that DLSS/Streamline consumes. DLSS does not generate a separate depth buffer. Falls back to color-only if unavailable."))
    bool bUseDepth = true;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Use DLSS-Compatible Motion Vectors", ToolTip = "Builds a dense pixel-space motion-vector guide from UE depth + GBuffer velocity. Forced OFF on every editor startup until depth-only guide generation/feed is validated."))
    bool bUseMotionVectors = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (DisplayName = "Automatic History Reset", ToolTip = "Resets NR temporal history on camera cuts, forced camera resets, first frame, viewport-size changes, and major exposure discontinuities."))
    bool bAutoResetHistory = true;

    // ---------------- Advanced ----------------
    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (DisplayName = "Depth Is Reversed-Z", ToolTip = "UE D3D12 uses reversed-Z by default."))
    bool bDepthInverted = true;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "-4.0", ClampMax = "4.0", UIMin = "-2.0", UIMax = "2.0", DisplayName = "Motion Scale X Multiplier"))
    float MotionScaleXMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "-4.0", ClampMax = "4.0", UIMin = "-2.0", UIMax = "2.0", DisplayName = "Motion Scale Y Multiplier"))
    float MotionScaleYMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "1.1", ClampMax = "8.0", UIMin = "1.1", UIMax = "4.0", DisplayName = "Exposure Reset Ratio", ToolTip = "If pre-exposure changes by more than this ratio between frames, reset NR history."))
    float ExposureResetRatio = 2.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced")
    bool bVerboseDiagnostics = true;

    // ---------------- Live Diagnostics ----------------
    // Intentionally not UPROPERTY fields. The dedicated editor window renders these
    // as a separate live text panel so diagnostics can update without rebuilding the
    // DetailsView and interrupting sliders, combo boxes, or mouse capture.
    FString DiagnosticRuntime;
    FString DiagnosticNGX;
    FString DiagnosticGuides;
    FString DiagnosticSubrects;
    FString DiagnosticTemporal;
    FString DiagnosticCounters;
    FString DiagnosticLastResult;
};
