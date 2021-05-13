
using System;
using System.IO;
using UnrealBuildTool;

public class HermesURLHandler : ModuleRules
{
    public HermesURLHandler(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;

		RuntimeDependencies.Add(Path.Combine("$(BinaryOutputDir)", "hermes-urls.exe"), Path.Combine(ModuleDirectory, "target", "release", "hermes-urls.exe"));
	}
}
