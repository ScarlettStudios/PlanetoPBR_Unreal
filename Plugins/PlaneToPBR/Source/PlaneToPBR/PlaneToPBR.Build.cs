using UnrealBuildTool;

public class PlaneToPBR : ModuleRules
{
	public PlaneToPBR(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"HTTP",
			"Json",
			"ImageWrapper"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {});
	}
}
