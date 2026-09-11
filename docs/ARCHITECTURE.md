# Architecture

DLSS5ForUE5 is split into two Unreal modules:

- `DLSS5ForUE5_nvngx` - runtime renderer integration
- `DLSS5ForUE5Editor` - dedicated editor UI

## Render flow

At a high level:

```text
UE scene color
   + UE SceneDepth
   + optional UE velocity / camera reprojection
   + jitter / pre-exposure / reset state
          ↓
RDG-managed guide textures
          ↓
NGX feature 18 evaluation
          ↓
optional sequential NR passes
          ↓
UE output
```

The plugin uses Unreal RDG/RHI to own resource lifetime and transitions, then obtains native D3D12 resources inside an external compute pass for NGX evaluation. Manual D3D12 barrier flushing is intentionally avoided.

## NGX bootstrap

The current implementation reuses the NGX core already initialized by NVIDIA's Unreal DLSS plugin. The experimental Neural Rendering runtime is loaded separately.

## Depth

DLSS does not generate an independent superior scene-depth buffer. The host application supplies depth to DLSS. DLSS5ForUE5 therefore resolves UE SceneDepth into a compact R32F guide.

## Motion

The dense motion guide combines available object velocity with camera/background reprojection from depth and current/previous transforms. Object velocity coverage remains experimental.

## Caller gate

The tested runtime checks that the caller module path contains `nvngx.dll`. The runtime Unreal module name intentionally ends in `_nvngx` so its generated `.dll` satisfies that path check.
