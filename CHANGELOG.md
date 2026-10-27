# Changelog

## v0.6.1 - Neural Rendering Runtime and Editor/MRQ Improvements (release candidate)

- Updated the bundled third-party Neural Rendering runtime used by the Editor Win64 validation packages to **310.9.16.0**.
- **UE 5.6 and UE 5.8:** maintainer-confirmed working Neural Rendering in the Editor viewport and Movie Render Queue with the rebuilt 2026-10-27 binaries.
- **UE 5.5 and UE 5.7:** compiled for Editor Win64; fresh post-rebuild viewport/MRQ validation has not been claimed.
- Improved the pixel-space motion-guide convention and alignment of depth and motion guides to Neural Rendering color resolution.
- Added optional highlight preservation for bright tonemapped highlights, helping to retain emissive/specular intensity.
- Improved synchronization of Editor controls with launch CVars.
- Corrected the build-date string displayed by the Editor UI to **2026-10-27**.
- Credits: work from [@scragnog](https://github.com/scragnog)'s still-open [PR #5](https://github.com/praveenkumarappu/DLSS5ForUE5/pull/5) informed the implemented motion-guide, highlight-preservation and CVar changes. **PR #5 has not been merged in full.** Its custom-stencil mask, distortion field and other unvalidated experimental features are not advertised.
- Native Neural Rendering in packaged Development/Shipping games remains unsupported; existing low-light/Streamline limitations are not represented as fixed.

**Release synchronization status:** The compiled v0.6.1 source archive must be matched and committed to `main` before tagging or publishing. This changelog entry alone does not update the plugin implementation. See [v0.6.1 release notes](RELEASE_NOTES_v0.6.1.md).

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