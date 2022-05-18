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

public:
	UHermesPluginSettings(const FObjectInitializer& ObjectInitializer);

	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Plugins"));
	}
};
