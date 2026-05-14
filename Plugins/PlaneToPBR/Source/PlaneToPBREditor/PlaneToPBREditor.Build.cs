using UnrealBuildTool;

public class PlaneToPBREditor : ModuleRules
{
	public PlaneToPBREditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"PlaneToPBR"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"UnrealEd",
			"ToolMenus",
			"Slate",
			"SlateCore",
			"EditorFramework"
		});
	}
}
