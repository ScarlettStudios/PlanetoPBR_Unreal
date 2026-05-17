// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class planetoPBR_unreal : ModuleRules
{
	public planetoPBR_unreal(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine"
		});

		PrivateDependencyModuleNames.AddRange(new string[] {  });
	}
}
