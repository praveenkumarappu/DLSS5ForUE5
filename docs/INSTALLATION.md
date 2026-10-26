# Installation - v0.6.0 stable baseline

## Precompiled release installation

1. Install NVIDIA's official DLSS plugin for the same Unreal Engine version used by your project.
2. From the [GitHub Releases page](https://github.com/praveenkumarappu/DLSS5ForUE5/releases), download the plugin ZIP that matches your Unreal Engine version. Use the exact filenames shown on the selected release.
3. Extract the `DLSS5ForUE5` folder to `YourProject/Plugins/`.
4. Launch the project and enable NVIDIA DLSS and DLSS5ForUE5 if prompted.
5. Restart Unreal Engine.
6. Open **Tools > DLSS 5 for UE5**.

The precompiled ZIP contains binaries for the selected engine version. Visual Studio is not required for normal installation.

## Source build

If you clone the repository instead of using a release ZIP, place `Plugins/DLSS5ForUE5` inside your project and regenerate project files. Build `Development Editor / Win64` with Visual Studio 2022.

The same source tree supports UE 5.5-5.8 through engine-version compatibility guards.

## Required NVIDIA plugin

DLSS5ForUE5 uses the NGX core initialized by NVIDIA's official Unreal Engine DLSS plugin. Installing/enabling the NVIDIA DLSS plugin is required.

Keep the plugin's module names and any separately supplied NVIDIA runtime files unchanged. Renaming binaries is not a supported initialization or troubleshooting method.

## Supported usage

Neural Rendering is experimental and intended for Unreal Editor use. Native Neural Rendering in packaged Development or Shipping games is not currently supported. `DLSS5.Open` is an Editor-only console command.
