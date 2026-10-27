# DLSS5ForUE5 v0.6.1

## Highlights

- **Updated NVIDIA Neural Rendering runtime:** v0.6.1 Editor packages use the separately licensed NVIDIA NGX Neural Rendering runtime reporting version **310.9.16.0**.
- **Movie Render Queue (MRQ):** Maintainer-confirmed Neural Rendering output in **UE 5.6 and UE 5.8**, alongside working Editor viewport rendering. This is Editor/MRQ support, not packaged-game native Neural Rendering support.
- **Improved motion-vector guidance:** Uses display-pixel motion-vector units for the NR path and aligns depth/motion guides to the Neural Rendering color resolution to reduce temporal misalignment.
- **Highlight preservation:** Optional restoration of bright, tonemapped highlights over the NR result to help retain emissive and specular intensity.
- **Better Editor setting behavior:** Improved synchronization between launch CVars and the Editor control window without unexpectedly overwriting existing runtime settings.
- **Rebuilt for UE 5.5, 5.6, 5.7, and 5.8:** Refreshed Editor Win64 binaries and corrected the About-panel build date to **2026-10-27**.

## Maintainer verification

- **UE 5.6 and UE 5.8:** Maintainer tested the final date-corrected build in both the Editor viewport and Movie Render Queue and confirmed working output.
- **UE 5.5 and UE 5.7:** Matching Editor modules were rebuilt successfully. Fresh runtime/MRQ acceptance for these exact final builds is not represented as completed.
- **Packaged Development/Shipping:** Native NVIDIA Neural Rendering inference is **not supported** in packaged games. Do not interpret successful editor/MRQ output as packaged runtime support.

## Requirements and installation

- Windows 64-bit, D3D12, compatible NVIDIA RTX GPU and driver.
- The official NVIDIA DLSS Unreal Engine plugin, installed and enabled for your Unreal Engine version.
- Download the **UE 5.5, 5.6, 5.7, or 5.8 Editor Win64 ZIP** matching your engine from [GitHub Releases](https://github.com/praveenkumarappu/DLSS5ForUE5/releases).
- Extract the single top-level `DLSS5ForUE5/` folder into `YourProject/Plugins/`, launch Unreal Editor, then open **Tools > DLSS 5 for UE5**.

The Neural Rendering model/runtime is a separate NVIDIA component; this is an **unofficial community integration** and is not endorsed by NVIDIA or Epic Games. See [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

## Contribution acknowledgement

Thanks to [@scragnog](https://github.com/scragnog) for the existing GPU-safe NR lifetime/history improvements merged via [PR #2](https://github.com/praveenkumarappu/DLSS5ForUE5/pull/2), and the guidance in still-open [PR #5](https://github.com/praveenkumarappu/DLSS5ForUE5/pull/5) that informed adopted motion-guide, highlight, and Editor-CVar changes. **PR #5 is not merged in full.** Its proposed custom-stencil control mask, distortion-field input, and further experimental controls are not part of the claimed v0.6.1 feature set. See [CONTRIBUTORS.md](CONTRIBUTORS.md).

## Limitations

Neural Rendering remains experimental. Multiple sequential passes cost additional GPU time; low-light scenes, camera/object motion, and NVIDIA Streamline + MRQ combinations can be sensitive to content and SDK version. For documented limitations and workarounds see [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md).
