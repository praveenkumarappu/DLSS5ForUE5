# DLSS5ForUE5 v0.6.0

## Highlights

- Independent Neural Rendering feature and history per view and sequential pass
- GPU-safe deferred NGX feature release using completion fences
- Improved temporal-history reset and recovery behavior
- Preview, capture, and unsupported views excluded from Neural Rendering history
- Improved scene-guide resource handling
- Corrected motion-vector handling
- Improved post-temporal jitter handling
- Safer runtime shutdown with GPU drain
- Added Neural Rendering lifetime and history regression testing
- Unreal Engine 5.5-5.8 compatibility
- Unreal Engine 5.6 Renderer/Internal compatibility fix
- Improved stability when resizing viewports, changing presets, and using multiple views

## Contributor credit

Thanks to **[@scragnog](https://github.com/scragnog)** for identifying the GPU feature-lifetime / temporal-history problem in [Issue #1](https://github.com/praveenkumarappu/DLSS5ForUE5/issues/1) and contributing the core fix in [PR #2](https://github.com/praveenkumarappu/DLSS5ForUE5/pull/2).

## Validation note

Maintainer runtime testing has confirmed the NR lifetime update with 3D scene guides enabled on UE 5.5 and UE 5.6 without observed flicker or D3D12/GPU crashes in those tests. The v0.6.0 UE 5.7 and UE 5.8 builds should still receive the same runtime regression checks before their stability is considered independently verified.

## Stable baseline and packaged support

The maintained stable baseline is v0.6.0. Existing maintainer editor validation covers the NR lifetime update on UE 5.5 and UE 5.6; equivalent v0.6.0 runtime validation on UE 5.7 and UE 5.8 remains pending. No new compilation, GPU, or inference tests were performed for this documentation alignment.

Packaged Development and Shipping native NR are unsupported for this baseline. Separate v0.6.1 validation reported NGX Init_Ext failure `0xBAD00002` with zero successful native evaluations; this must not be presented as direct v0.6.0 validation. Packaging success, DLL loading, mock tests, and fallback frames do not establish native neural inference.
