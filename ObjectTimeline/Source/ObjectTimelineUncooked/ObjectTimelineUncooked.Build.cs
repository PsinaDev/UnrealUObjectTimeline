// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ObjectTimelineUncooked : ModuleRules
{
	public ObjectTimelineUncooked(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"ObjectTimelineRuntime",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"BlueprintGraph",
			"GraphEditor",
			"KismetCompiler",
			"Slate",
			"SlateCore",
			"UnrealEd",
			"InputCore",
			"EditorWidgets",
			"PropertyEditor",
			"ToolWidgets"
		});
	}
}
