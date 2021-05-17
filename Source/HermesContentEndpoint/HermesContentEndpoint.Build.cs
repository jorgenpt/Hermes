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
				"Slate",
				"SlateCore",
				"ToolMenus",
				"UnrealEd",
			}
		);
	}
}
