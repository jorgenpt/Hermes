// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
using UnrealBuildTool;

public class HermesServer : ModuleRules
{
	public HermesServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateIncludePaths.Add("HermesServer/Private");

		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"DeveloperSettings",
				"HermesURLHandler",
				"HTTP",
				"Projects",
				"UnrealEd",
			}
		);
	}
}
