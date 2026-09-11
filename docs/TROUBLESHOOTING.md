# Troubleshooting

## Runtime says not loaded

Check that this exact file exists:

```text
Plugins/DLSS5ForUE5/Binaries/ThirdParty/Win64/nvngx_dlssnr.dll
```

Then run `DLSS5.Status`.

## NGX core not ready

Confirm the official NVIDIA DLSS Unreal Engine plugin is installed and enabled and that the active RHI is D3D12.

## Editor will not compile

Use Visual Studio 2022 with MSVC v143. Unreal's NuGet/AutomationTool restore warnings can appear even when the actual C++ toolchain is available; focus on the first UnrealBuildTool C++ error.

## Shader compile failure

Delete project `DerivedDataCache` only if needed, plus project `Binaries` / `Intermediate`, then rebuild. Include the first shader error when reporting.

## D3D12 E_INVALIDARG

Use `DLSS5.SafeMode`, reopen the editor, then enable in stages: color only → guide generation → depth feed → motion. Capture the exact configuration that first fails.

## Flicker

Scene Guides, depth and temporal tracking can significantly reduce flicker. Validate Depth separately from Motion. Use Pass Count 1 while diagnosing temporal issues.

## UI is difficult to click / sliders lose focus

Use v0.5.3 or newer. The live diagnostics panel no longer force-refreshes the editable DetailsView.

## Report template

Run:

```text
DLSS5.Status
```

and include the output with your UE version, GPU, driver, NVIDIA plugin version and crash log.
