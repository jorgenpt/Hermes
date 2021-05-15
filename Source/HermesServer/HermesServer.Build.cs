// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.

using UnrealBuildTool;

public class HermesServer : ModuleRules
{
	public HermesServer(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateIncludePaths.Add("HermesServer/Private");

		PublicDependencyModuleNames.Add("Core");
		PrivateDependencyModuleNames.AddRange(
			new[]
			{
				"CoreUObject",
				"HermesURLHandler",
				"HTTP",
				"Projects",
				"UnrealEd",
			}
		);
	}
}
