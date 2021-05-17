// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
using UnrealBuildTool;

public class HermesUriSchemeBranchReplacement : ModuleRules
{
	public HermesUriSchemeBranchReplacement(ReadOnlyTargetRules Target) : base(Target)
	{
		PrivateDependencyModuleNames.AddRange(new[] {"Core", "CoreUObject", "DeveloperSettings"});
		PrivateIncludePathModuleNames.Add("HermesServer");
	}
}
