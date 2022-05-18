// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <CoreMinimal.h>
#include <Features/IModularFeatures.h>
#include <HermesUriSchemeProvider.h>
#include <Hermes.h>
#include <Misc/App.h>
#include <Modules/ModuleInterface.h>

#include "HermesBranchSupportPluginSettings.h"
#include "Modules/ModuleManager.h"

struct FHermesBranchSupport : IHermesUriSchemeProvider, IModuleInterface
{
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;
	virtual TOptional<FString> GetPreferredScheme() override final;
};

IMPLEMENT_MODULE(FHermesBranchSupport, HermesBranchSupport);

void FHermesBranchSupport::StartupModule()
{
	IModularFeatures::Get().RegisterModularFeature(GetModularFeatureName(), this);
}

void FHermesBranchSupport::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(GetModularFeatureName(), this);
}

TOptional<FString> FHermesBranchSupport::GetPreferredScheme()
{
	return GetDefault<UHermesBranchSupportPluginSettings>()->GetScheme();
}
