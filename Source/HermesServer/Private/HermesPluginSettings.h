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
	UPROPERTY(Config, EditAnywhere, meta = (
		DisplayName = "URL Protocol",
		ToolTip = "The protocol to use for our URLs, defaults to hue4://",
		ConfigRestartRequired = true))
	FString UrlProtocol = TEXT("hue4");

	UPROPERTY(Config, EditAnywhere, meta = (
		DisplayName = "URL Hostname",
		ToolTip = "The hostname used for our URLs, defaults to the project name if left empty",
		ConfigRestartRequired = true))
	FString UrlHostname;

public:
	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Plugins"));
	}
};
