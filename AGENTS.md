# Codex Code Review guidance for DLSS5ForUE5

Review this unofficial Unreal Engine plugin for correctness and severe regressions. Report actionable defects with file/line references and a reproducible failure mode. Prioritize rendering correctness, runtime safety, licensing, and build compatibility over formatting.

## Release baseline

- The current stable baseline is **v0.6.0** (`Version: 600`, `VersionName: 0.6.0`). Keep source, installer output, current installation instructions, and release documentation aligned with it.
- Do not import the separate v0.6.1 validation build or present its results as direct v0.6.0 validation. Historical v0.5.6 release notes remain historical.
- Stable baseline describes the maintained editor release; it does not establish packaged native NR support or independently verified runtime stability on every engine version.

## Critical review rules

- Target Unreal Engine 5.5, 5.6, 5.7, and 5.8. Flag breaking API changes and missing UE version guards. A compile on one engine version is not proof of support for others.
- Treat Neural Rendering as an experimental, editor-validated feature. Packaged Development and Shipping native NR are unsupported for this baseline. Separate v0.6.1 validation reported `NVSDK_NGX_D3D12_Init_Ext` returning `0xBAD00002`. Never describe fallback frames, successful DLL loads, or compilation as proof of native inference.
- For any claim that NR works, require evidence of NGX feature creation, increasing successful native evaluation counters, valid GPU output, and the exact tested engine, configuration, GPU, and driver.
- Audit render-thread and RHI lifetime, GPU-fence retirement, feature release, D3D12 resource states, texture extent/view-rect mappings, depth, motion vectors, temporal jitter/history reset and per-view/pass isolation.
- Verify safe initialization failure, runtime toggles, resize, camera cuts, Editor shutdown, and optional third-party plugin absence. Never hide actual NGX errors behind a PASS status.
- Do not add proprietary NVIDIA runtime binaries or unsupported redistributables to the Git repository. The original code is MIT; NVIDIA and Unreal Engine components have separate licenses.
- Treat packaged Neural Rendering as explicitly unsupported in documentation until a separately validated, authorized packaged runtime path is established. Do not use caller spoofing, binary patching, or fabricated success as a workaround.
- Verify PRs do not accidentally include generated `Binaries`, `Intermediate`, `Saved`, diagnostics, personal paths, credentials, or multi-hundred-MB experiment artifacts.
- Do not merge, tag or publish source releases automatically as part of review. Keep code review separate from the later per-engine ZIP release process.

## Review output

Focus on P0/P1 correctness and safety problems that materially affect users or invalidate tests. For each finding, explain the exact code location, when it fails, and a minimal fix or test. Distinguish proven failures from hypotheses. If no such issues are found, explicitly state the scope and any tests that were not run.