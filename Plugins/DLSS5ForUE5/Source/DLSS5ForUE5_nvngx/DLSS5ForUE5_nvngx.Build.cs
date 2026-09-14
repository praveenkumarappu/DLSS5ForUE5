using UnrealBuildTool;
using System.IO;

public class DLSS5ForUE5_nvngx : ModuleRules
{
    public DLSS5ForUE5_nvngx(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        // FViewInfo::ViewRect is the actual scene-buffer rectangle before upscaling.
        PrivateIncludePaths.Add(Path.Combine(EngineDirectory, "Source/Runtime/Renderer/Private"));

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Projects",
            "RenderCore",
            "Renderer",
            "RHI"
        });

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("D3D12RHI");

            // ID3D12DynamicRHI.h includes WindowsD3D12ThirdParty.h, which in turn
            // includes d3dx12.h. Pull in Unreal's bundled DX12 third-party headers
            // explicitly so installed-engine builds (UE 5.6.x) can resolve them.
            AddEngineThirdPartyPrivateStaticDependencies(Target, "DX12");

            PublicDefinitions.Add("DLSS5NR_WITH_D3D12=1");

            string DllPath = Path.Combine(PluginDirectory, "Binaries", "ThirdParty", "Win64", "nvngx_dlssnr.dll");
            // The public repository intentionally does not redistribute NVIDIA's experimental
            // neural-rendering runtime. Build the plugin without it, then let the user provide a
            // legally obtained copy at the documented path. Stage it only when present.
            if (File.Exists(DllPath))
            {
                RuntimeDependencies.Add(DllPath, StagedFileType.NonUFS);
            }
            else
            {
                System.Console.WriteLine("DLSS5ForUE5: nvngx_dlssnr.dll not found. Plugin will build, but Neural Rendering remains unavailable until the runtime is supplied.");
            }
        }
        else
        {
            PublicDefinitions.Add("DLSS5NR_WITH_D3D12=0");
        }
    }
}
