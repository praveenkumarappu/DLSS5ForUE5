# Changelog

## Unreleased

- Object velocity is decoded with the engine's `DecodeVelocityFromTexture`. UE 5.x gamma-encodes velocity on SM5+, and the previous linear unpack inflated small motion by an order of magnitude, so animated characters never settled while static geometry (camera reprojection) was stable.
- Motion-vector guide is now handed to NGX in display-pixel units with a unit scale, matching NVIDIA's UE DLSS plugin. It was normalised by the guide width, so NR received near-zero motion and history blended unaligned under camera movement.
- Depth and motion guides are resolved at the NR colour resolution instead of the render resolution.
- Added a highlight-preserve blend (`r.DLSS5.HighlightPreserveStart/End`) that keeps tonemapped brightness for lights and emissive surfaces and restores the tonemapper alpha.
- `r.DLSS5.*` console variables are the launch state; the editor window mirrors them and no longer saves to config. Safe boot engages only after a session that did not shut down cleanly (`Saved/DLSS5NR/session.lock`).
- Style accepts 0..2 and is labelled by measured behaviour; Skin Structure accepts -1 (runtime default).
- Added `DLSSNR.ControlMask` from UE custom depth stencil (`r.DLSS5.ControlMaskStencil`, `r.DLSS5.ControlMaskInvert`). Mask 0 leaves the source untouched, 1 applies NR.
- Added `DLSSNR.BidirectionalDistortionField` input from a project texture (`r.DLSS5.DistortionFieldTexture`).
- Added experimental `r.DLSS5.ScalingRatio` / `r.DLSS5.ScalingRatioMode`; the current runtime rejects a downscaled input and shows no effect from the hint alone.
- Feature-creation log line now reports input size, ratio, style, mask and field.

## v0.6.0 - NR Stability Update

- Added independent Neural Rendering feature/history state per view and sequential pass.
- Added GPU-fence based deferred NGX feature retirement so replaced features are not released while GPU work may still reference them.
- Added safer shutdown behavior that drains GPU work before releasing NR features.
- Excluded unsupported preview/capture views from NR history.
- Improved history reset behavior for first use, interrupted rendering, failed evaluations, view/subrect changes, and guide changes.
- Cleared stale scene-guide bindings when switching between guide-enabled and color-only views.
- Corrected motion-vector direction/validity handling for the NGX NR path.
- Uses zero jitter for the post-temporal/post-upscale NR input path.
- Added deterministic NR lifetime/history regression coverage and native validation notes.
- Added UE 5.6 Renderer/Internal include compatibility required by the new `FViewInfo::ViewRect` path while retaining shared UE 5.5–5.8 source compatibility.
- Updated plugin metadata and in-editor About UI to **v0.6.0 Stable**.
- Maintainer validation completed on UE 5.5 and UE 5.6 with 3D guides enabled, with no observed D3D12/GPU crashes or flicker during testing.
- Contributor credit: **[@scragnog](https://github.com/scragnog)** for reporting Issue #1 and implementing the core NR GPU feature-lifetime and temporal-history isolation fix in PR #2.

## v0.5.6 - Initial Public Preview

- Added public release support for Unreal Engine 5.5, 5.6, 5.7, and 5.8.
- Added UE 5.5 `ISceneViewExtension` compatibility overrides.
- Added UE 5.8 `FCoreDelegates::GetOnPostEngineInit()` compatibility handling.
- Added GitHub link to About section.
- Added 1–4 sequential DLSS 5 Neural Rendering passes.
- Added unified Controls, Live Diagnostics, and About layout.
- Added dedicated pass-count slider.
- Added Scene Depth and dense motion-vector guides.
- Added temporal jitter, pre-exposure, camera-cut, and history-reset tracking.
- Added Natural/Cinematic styles and neural controls.
- Added Safe Mode and live diagnostics.
- Renamed the public plugin to `DLSS5ForUE5` and console namespace to `DLSS5` / `r.DLSS5`.