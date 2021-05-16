// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
using UnrealBuildTool;

public class HermesContentEndpoint : ModuleRules
{
	public HermesContentEndpoint(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"ApplicationCore",
				"AssetRegistry",
				"ContentBrowser",
				"Core",
				"CoreUObject",
				"EditorStyle",
				"Engine",
				"HermesServer",
				"InputCore",
				"MainFrame",
				"ToolMenus",
				"UnrealEd",
			}
		);

		// Temporarily list a few dependencies that we need to make ContentBrowser build.
		// Required until https://github.com/EpicGames/UnrealEngine/pull/8036 is merged.
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"SlateCore",
				"Slate",
			}
		);
	}
}
