# DLSS5ForUE5 v0.5.6 - Initial Public Preview

First public preview of DLSS5ForUE5, an experimental DLSS 5 Neural Rendering integration for Unreal Engine.

## Supported Unreal Engine versions

- Unreal Engine 5.5
- Unreal Engine 5.6
- Unreal Engine 5.7
- Unreal Engine 5.8

All four versions were build-tested and runtime-tested with Scene Depth, Motion Guides, and Neural Rendering enabled.

## Highlights

- Native Unreal Editor integration
- Dedicated **Tools → DLSS 5 for UE5** window
- Natural / Cinematic style controls
- Intensity, Local Tone, Local Structure, Skin Structure
- UE Scene Depth guide
- Dense motion-vector guide
- Jitter, exposure, camera-cut, and temporal history handling
- 1–4 sequential Neural Rendering passes
- Live runtime / NGX / guide / temporal diagnostics
- Safe Mode recovery
- One universal version-aware source tree for UE 5.5–5.8

## Requirements

- Windows 10/11 x64
- DirectX 12
- Compatible NVIDIA RTX GPU and driver
- NVIDIA's official DLSS Unreal Engine plugin installed and enabled
- DLSS5ForUE5 ZIP matching the project's Unreal Engine version

## Release assets

- `DLSS5ForUE5 [v0.5.6] -UE5.5.zip`
- `DLSS5ForUE5 [v0.5.6] -UE5.6.zip`
- `DLSS5ForUE5 [v0.5.6] -UE5.7.zip`
- `DLSS5ForUE5 [v0.5.6] -UE5.8.zip`

Optional manually prepared source backups:

- `DLSS5ForUE5-v0.5.6-Source.zip`
- `DLSS5ForUE5-v0.5.6-Source.tar.gz`

GitHub also automatically provides its own `Source code (zip)` and `Source code (tar.gz)` archives when the `v0.5.6` release tag is created.

## SHA-256

```text
4672dde4b1040a34852315d2e99b9eab1769cf77b5f487e379966d89eb12ef38  DLSS5ForUE5 [v0.5.6] -UE5.5.zip
adda21d600347fc57f9553cedc4992aa9077b86aa4d7226b248b817ff2021abb  DLSS5ForUE5 [v0.5.6] -UE5.6.zip
09641cfa0ea46b621aa4b1d079fd7d5a4560025bb76ad317142209879b72b1a7  DLSS5ForUE5 [v0.5.6] -UE5.7.zip
2a58e90006f80cdf4dad45a335ccc88a49fc89ce01fd41520a311645981a86d1  DLSS5ForUE5 [v0.5.6] -UE5.8.zip
f0ef9131484479ab2fa13287b1113c593829f6f6f12d179a1c9abab9e9ff5c8d  DLSS5ForUE5-v0.5.6-Source.zip
ab09023995a21dfcfd78547e44da2b939c870412ea7580f6d44f27c10e96e816  DLSS5ForUE5-v0.5.6-Source.tar.gz
```

## Installation

Extract the matching release ZIP into `YourProject/Plugins/`, enable NVIDIA DLSS and DLSS5ForUE5, restart Unreal Engine, then open **Tools → DLSS 5 for UE5**.

## Important notes

- This is experimental research/community software.
- The NVIDIA DLSS plugin is required because DLSS5ForUE5 reuses its NGX initialization.
- Multi-pass processing significantly increases GPU cost.
- Motion-vector object velocity remains experimental in render-hook states where UE exposes a `1×1` velocity source.
- Certain NVIDIA Streamline builds can crash Movie Render Queue during PIE startup. See `docs/KNOWN_ISSUES.md`.
- NVIDIA runtime components contained in release packages remain governed by NVIDIA's license terms and are not covered by the DLSS5ForUE5 MIT License.
- Public release ZIPs intentionally exclude PDBs and Unreal `Intermediate` build files to avoid exposing local build paths and unnecessary debug data.
