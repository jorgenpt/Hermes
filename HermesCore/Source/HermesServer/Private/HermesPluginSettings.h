// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include "HermesPluginSettings.generated.h"

UCLASS(Config=Editor, DefaultConfig, meta = (DisplayName = "Hermes URLs"))
class UHermesPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Hermes", meta = (
		DisplayName = "Default URI Scheme",
		ToolTip =
		"The scheme to use for our URIs if there is no UriSchemeProvider -- should be unique to each project / branch",
		ConfigRestartRequired = true))
	FString DefaultUriScheme;

	UPROPERTY(Config, EditAnywhere, Category = "Hermes", AdvancedDisplay, meta = (
		DisplayName = "Enable Debug Logging",
		ToolTip = "Log debug messages about URL handling to hermes.log next to hermes_urls.exe",
		ConfigRestartRequired = true))
	bool bDebug = false;

public:
	UHermesPluginSettings(const FObjectInitializer& ObjectInitializer);

	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Plugins"));
	}
};
