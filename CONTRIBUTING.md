# Contributing

Thanks for testing DLSS5ForUE5. Because this plugin touches Unreal's render graph, native D3D12 resources and experimental NVIDIA runtime behavior, reproducible reports matter more than volume.

## Bug reports

Please include:

- DLSS5ForUE5 version
- Unreal Engine exact version
- GPU and driver version
- NVIDIA DLSS / Streamline plugin version
- D3D12 / SM6 confirmation
- Whether Scene Guides, Depth, Motion and multi-pass were enabled
- `DLSS5.Status` output
- Unreal crash call stack / relevant log excerpt
- Minimal reproduction steps

Do not attach NVIDIA proprietary runtime binaries to issues, pull requests, or source commits.

## Pull requests

1. Open an issue first for large rendering changes.
2. Keep the public console namespace under `DLSS5.*` / `r.DLSS5.*`.
3. Preserve safe boot and Safe Mode recovery.
4. Avoid direct D3D12 state/barrier manipulation when UE RDG/RHI can own the transition.
5. Keep NVIDIA proprietary binaries out of commits.
6. Test at Pass Count 1 before validating 2–4.

By submitting a contribution you agree that your contribution may be distributed under the repository's MIT License.
