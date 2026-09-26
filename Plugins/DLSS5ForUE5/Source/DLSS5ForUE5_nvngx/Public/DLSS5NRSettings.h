#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DLSS5NRSettings.generated.h"

class UTexture;

UENUM()
enum class EDLSS5NRStyle : uint8
{
    // Names are unverifiable: the DLL takes an int and its strings name no styles. Labels describe
    // measured behaviour (2026-09-26, Kirk's apartment): 0 stays closest to the source, 1 is warmer,
    // more saturated and sharpest (NVIDIA's reference CLI default), 2 is muted and softer (filmic).
    Default UMETA(DisplayName = "Style 0 (closest to source)"),
    Natural UMETA(DisplayName = "Style 1 (NVIDIA reference default: warmer, sharper)"),
    Cinematic UMETA(DisplayName = "Style 2 (Cinematic: muted, softer)")
};

/**
 * Artist-facing settings model for DLSS 5 for UE5.
 *
 * The r.DLSS5.* console variables are the source of truth: DefaultEngine.ini sets them at launch,
 * this object mirrors them for the editor window (LoadFromCVars), and edits in the window push
 * back to the CVars (ApplyToCVars). Nothing here is saved to config.
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
    void LoadFromCVars();
    void RefreshDiagnostics();

    // ---------------- General ----------------
    UPROPERTY(EditAnywhere, Config, Category = "General", meta = (DisplayName = "Enable Neural Rendering"))
    bool bEnableNeuralRendering = false;

    UPROPERTY(EditAnywhere, Config, Category = "General", meta = (ToolTip = "Network style. Changing it resets NR temporal history."))
    EDLSS5NRStyle Style = EDLSS5NRStyle::Default;

    UPROPERTY(EditAnywhere, Config, Category = "General", meta = (ClampMin = "0", ClampMax = "3", UIMin = "0", UIMax = "3", DisplayName = "NR Preset", ToolTip = "Selects the network weights (0..3). NVIDIA's reference default is 3. Changing it rebuilds the feature."))
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

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "-1.0", ClampMax = "2.0", UIMin = "-1.0", UIMax = "2.0", DisplayName = "Skin Structure", ToolTip = "-1 leaves the runtime default (NVIDIA reference)."))
    float SkinStructure = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (DisplayName = "Automatic Mask"))
    bool bAutoMask = true;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (DisplayName = "UI Correction"))
    bool bUICorrection = false;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "Highlight Preserve Start", ToolTip = "Display brightness (0..1) where the original tonemapped colour starts blending back over the NR result, so lights and emissive panels keep their brightness. Set at or above End to disable."))
    float HighlightPreserveStart = 0.7f;

    UPROPERTY(EditAnywhere, Config, Category = "Neural Controls", meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0", DisplayName = "Highlight Preserve End", ToolTip = "Display brightness (0..1) from which the original tonemapped colour fully replaces the NR result."))
    float HighlightPreserveEnd = 0.9f;

    // ---------------- Scene Guides ----------------
    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (DisplayName = "Enable 3D Scene Guides (Experimental)", ToolTip = "Master safety switch for depth and motion-vector guides. Off is the colour-only path, which cannot be temporally stable under camera motion. Use DLSS5.SafeMode to switch everything off after a crash."))
    bool bEnableSceneGuides = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Feed Scene Guides Into NR (Experimental)", ToolTip = "Safe staging switch. Off generates/diagnoses guide textures without handing them to NGX. Turn this on only after guide generation is stable."))
    bool bFeedSceneGuides = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Use DLSS Input Depth (UE Scene Depth)", ToolTip = "Uses Unreal Engine scene depth, which is the host depth source that DLSS/Streamline consumes. DLSS does not generate a separate depth buffer. Falls back to color-only if unavailable."))
    bool bUseDepth = true;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Use DLSS-Compatible Motion Vectors", ToolTip = "Builds a dense display-pixel motion-vector guide from UE depth + GBuffer velocity. Required for stable results while the camera moves."))
    bool bUseMotionVectors = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", ClampMin = "-1", ClampMax = "255", UIMin = "-1", UIMax = "255", DisplayName = "Control Mask Stencil (Experimental)", ToolTip = "Feed a DLSSNR.ControlMask guide from UE custom depth stencil. 0 = off, -1 = any non-zero stencil, 1..255 = that stencil value. Tag actors with Render CustomDepth Pass. Mask semantics are undocumented by NVIDIA; test with Invert."))
    int32 ControlMaskStencil = 0;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Control Mask Invert"))
    bool bControlMaskInvert = false;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (EditCondition = "bEnableSceneGuides", DisplayName = "Distortion Field Texture (Experimental)", ToolTip = "RGBA16F texture or render target fed as DLSSNR.BidirectionalDistortionField for lens-warp post effects. RG = distorted-to-undistorted displacement, BA = the inverse, signed, in motion-vector units (display pixels). Same layout as Streamline's kBufferTypeBidirectionalDistortionField."))
    TSoftObjectPtr<UTexture> DistortionFieldTexture;

    UPROPERTY(EditAnywhere, Config, Category = "Scene Guides", meta = (DisplayName = "Automatic History Reset", ToolTip = "Resets NR temporal history on camera cuts, forced camera resets, first frame, viewport-size changes, and major exposure discontinuities."))
    bool bAutoResetHistory = true;

    // ---------------- Advanced ----------------
    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (DisplayName = "Depth Is Reversed-Z", ToolTip = "UE D3D12 uses reversed-Z by default."))
    bool bDepthInverted = true;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "-4.0", ClampMax = "4.0", UIMin = "-2.0", UIMax = "2.0", DisplayName = "Motion Scale X Multiplier", ToolTip = "Debug multiplier. Vectors are already display pixels; 1.0 is the NGX convention."))
    float MotionScaleXMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "-4.0", ClampMax = "4.0", UIMin = "-2.0", UIMax = "2.0", DisplayName = "Motion Scale Y Multiplier", ToolTip = "Debug multiplier. Vectors are already display pixels; 1.0 is the NGX convention."))
    float MotionScaleYMultiplier = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "1.1", ClampMax = "8.0", UIMin = "1.1", UIMax = "4.0", DisplayName = "Exposure Reset Ratio", ToolTip = "If pre-exposure changes by more than this ratio between frames, reset NR history."))
    float ExposureResetRatio = 2.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "0.25", ClampMax = "1.0", UIMin = "0.25", UIMax = "1.0", DisplayName = "Scaling Ratio (Experimental)", ToolTip = "DLSSNR.ScalingRatio. Below 1 the network runs at a lower resolution. Forces pass count 1 while active."))
    float ScalingRatio = 1.0f;

    UPROPERTY(EditAnywhere, Config, Category = "Advanced", meta = (ClampMin = "0", ClampMax = "1", UIMin = "0", UIMax = "1", DisplayName = "Scaling Ratio Mode", ToolTip = "0 = downsample the NR colour input and guides to the ratio (DLSS-style, NR upscales to the output). 1 = full-resolution input, ratio hint only."))
    int32 ScalingRatioMode = 0;

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
