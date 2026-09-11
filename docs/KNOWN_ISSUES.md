# Known Issues

## Movie Render Queue / NVIDIA Streamline PIE assertion

With some NVIDIA Streamline plugin builds, starting Movie Render Queue with DLSS/DLAA can assert inside NVIDIA's `StreamlineRHI::OnBeginPIE()` due to an invalid supported/unsupported PIE state combination.

The observed call stack is inside NVIDIA Streamline rather than DLSS5ForUE5. If encountered, test MRQ with optional Streamline features such as Frame Generation / Reflex disabled while keeping NVIDIA DLSS and DLSS Movie Pipeline support enabled.

## Motion source can report 1×1

In some render-hook states Unreal's native velocity source is a 1×1 dummy texture. DLSS5ForUE5 still generates a dense motion guide and reconstructs camera/background motion from Scene Depth, but per-object motion remains experimental.

A useful validation test is a stationary camera with a moving object, comparing Motion ON vs OFF.

## Multi-pass cost

Pass Count 2–4 sequentially evaluates Neural Rendering multiple times. GPU cost rises substantially and temporal/artistic behavior may become stronger. More passes are not necessarily higher quality.

## D3D12 only

The current implementation is intended for Windows D3D12. Other RHIs are not supported by this public preview.
