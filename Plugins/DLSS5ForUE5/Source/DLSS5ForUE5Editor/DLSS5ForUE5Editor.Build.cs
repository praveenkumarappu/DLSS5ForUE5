using UnrealBuildTool;

public class DLSS5ForUE5Editor : ModuleRules
{
    public DLSS5ForUE5Editor(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DLSS5ForUE5_nvngx"
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "UnrealEd",
            "Slate",
            "SlateCore",
            "ToolMenus",
            "PropertyEditor",
            "LevelEditor"
        });
    }
}
