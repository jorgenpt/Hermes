// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "HermesPluginSettings.h"

#include "Hermes.h"

#include <Misc/App.h>

UHermesPluginSettings::UHermesPluginSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const TOptional<FString> ProjectScheme = Hermes::SanitizeScheme(FApp::GetProjectName());
	DefaultUriScheme = ProjectScheme.Get("hunreal");
}
