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

GitHub automatically provides the repository source archives for the release tag.

## SHA-256

```text
UE5.5  d74bc6e92eb3d5efd75c65f73393f7f98d23d6539b042c28d92bc994a78a9614
UE5.6  c91a2926f224a7d678de812998ead62525e30f11ade9d4edff7530ee2dc73dcb
UE5.7  14c90af00ae0a404e615ac84576989b60d0b56d04bbf021d9367fd6a21d61138
UE5.8  ed71cd3bdf87eacc663527334cc03c9d1fe9fb0714938079a93b445012e6cbe8
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
