# NR lifetime and temporal-history validation

The runtime keeps a feature per view and sequential pass. Replaced features stay
alive until a fence after their last evaluation has completed, including any
pending RHI fence write. Module shutdown submits outstanding work and waits for
the GPU before releasing features.

The 120-frame inactivity threshold only selects features for retirement. It does
not establish GPU completion. Native NGX calls and their shared CPU parameter
block are serialized during RHI translation.

## Automated regression

Build a Win64 Development Editor host with this plugin enabled, then run:

```powershell
& "$EngineRoot\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $TestProject `
    -NullRHI -Unattended -NoSplash -NoSound -NoP4 `
    '-ExecCmds=Automation RunTests DLSS5.Runtime' `
    '-TestExit=Automation Test Queue Empty' `
    "-ReportExportPath=$ReportDirectory"
```

`DLSS5.Runtime.HistoryAndGPURetirement` exercises the real runtime using mock NGX
entry points and controllable GPU fences. It needs no NVIDIA runtime or GPU work.
It checks independent view/pass histories, first-use and recovery resets, zero
post-reconstruction jitter, stale depth-pointer clearing, deferred release while
GPU work or an RHI fence write remains pending, and exactly-once shutdown release.

This test does not validate real GPU ordering or visible image quality.

## Native rendering checks

Use a disposable D3D12/SM6 project with the official NVIDIA DLSS integration and a
legally obtained NR runtime installed as described in the main README.

1. Start with Pass Count 1. Check a stationary camera with Scene Guides off, then
   enable Scene Guides and DLSS-Compatible Motion Vectors. Inspect flicker and
   ghosting both stationary and while moving the camera or an object.
2. Toggle presets and guide availability repeatedly. Resize the main viewport
   and browse asset thumbnails. The main view should retain its own history;
   previews should not create NR features.
3. Test multiple perspective editor/PIE views and then Pass Counts 2 through 4.
   Each view/pass must have independent history and reset on a camera cut.
4. Disable NR for several frames and re-enable it. Close views, return after a
   pause, and exit the editor. Check for GPU faults and NGX errors.
5. Repeat Editor-side validation on the other supported engine versions. Packaged Development/Shipping native NR remains unsupported; packaged launch or graceful fallback is not an inference acceptance test.

Steps above describe the review checklist, not a claim that every case was run.

## Current baseline and packaged support

The maintained source baseline is **v0.6.1**. The maintainer confirmed working Neural Rendering in the UE 5.6 and UE 5.8 Editor viewport and Movie Render Queue using the final compiled builds. UE 5.5 and UE 5.7 Editor packages compiled successfully; equivalent runtime/MRQ acceptance for those final builds is not asserted.

The deterministic CPU/mock regression tests validate feature-state behavior and do not independently prove GPU correctness. Native packaged Development/Shipping Neural Rendering remains unsupported. Prior packaged experiments returned `NVSDK_NGX_D3D12_Init_Ext` failure `0xBAD00002` with zero successful native NR evaluations. A successful launch, fallback frame or DLL load is not native inference.

## Optional initialization diagnostics

`-DLSS5InitDiagnostics` can write JSONL diagnostic reports under the project's Saved directory (or a path supplied by `DLSS5InitDiagFile=`). These reports can contain executable paths, project paths, loaded module filenames, diagnostic addresses and GPU/NGX state. **Review and redact identifying paths or sensitive information before sharing them publicly.** This diagnostic mode is opt-in.
