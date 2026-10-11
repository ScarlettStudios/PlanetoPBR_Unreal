using UnrealBuildTool;

public class PlaneToPBRMCP : ModuleRules
{
	public PlaneToPBRMCP(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core", "CoreUObject", "Engine", "ToolsetRegistry"
		});
		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"PlaneToPBR", "PlaneToPBREditor", "UnrealEd", "Json", "JsonUtilities",
			"ModelContextProtocol", "ModelContextProtocolEngine", "ModelContextProtocolEditor"
		});
	}
}