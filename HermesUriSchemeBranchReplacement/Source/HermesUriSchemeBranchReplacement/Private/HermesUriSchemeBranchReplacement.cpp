// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <Core.h>
#include <Features/IModularFeatures.h>
#include <HermesUriSchemeProvider.h>
#include <Hermes.h>
#include <Misc/App.h>
#include <Modules/ModuleInterface.h>

#include "HermesUriSchemeBranchReplacementPluginSettings.h"
#include "Modules/ModuleManager.h"

struct FHermesUriSchemeBranchReplacement : IHermesUriSchemeProvider, IModuleInterface
{
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;
	virtual TOptional<FString> GetPreferredScheme() override final;
};

IMPLEMENT_MODULE(FHermesUriSchemeBranchReplacement, HermesUriSchemeBranchReplacement);

void FHermesUriSchemeBranchReplacement::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void FHermesUriSchemeBranchReplacement::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

TOptional<FString> FHermesUriSchemeBranchReplacement::GetPreferredScheme()
{
	return GetDefault<UHermesUriSchemeBranchReplacementPluginSettings>()->GetScheme();
}
