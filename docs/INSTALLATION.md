# Installation

## Precompiled release installation

1. Install NVIDIA's official DLSS plugin for the same Unreal Engine version used by your project.
2. Download the matching DLSS5ForUE5 release ZIP:
   - UE 5.5 → `DLSS5ForUE5 [v0.5.6] -UE5.5.zip`
   - UE 5.6 → `DLSS5ForUE5 [v0.5.6] -UE5.6.zip`
   - UE 5.7 → `DLSS5ForUE5 [v0.5.6] -UE5.7.zip`
   - UE 5.8 → `DLSS5ForUE5 [v0.5.6] -UE5.8.zip`
3. Extract the `DLSS5ForUE5` folder to `YourProject/Plugins/`.
4. Launch the project and enable NVIDIA DLSS and DLSS5ForUE5 if prompted.
5. Restart Unreal Engine.
6. Open **Tools → DLSS 5 for UE5**.

The precompiled ZIP contains binaries for the selected engine version. Visual Studio is not required for normal installation.

## Source build

If you clone the repository instead of using a release ZIP, place `Plugins/DLSS5ForUE5` inside your project and regenerate project files. Build `Development Editor / Win64` with Visual Studio 2022.

The same source tree supports UE 5.5–5.8 through engine-version compatibility guards.

## Required NVIDIA plugin

DLSS5ForUE5 uses the NGX core initialized by NVIDIA's official Unreal Engine DLSS plugin. Installing/enabling the NVIDIA DLSS plugin is required.

Do not rename the runtime module `DLSS5ForUE5_nvngx`. The name intentionally contains the `nvngx` caller-path token required by the tested Neural Rendering runtime.
