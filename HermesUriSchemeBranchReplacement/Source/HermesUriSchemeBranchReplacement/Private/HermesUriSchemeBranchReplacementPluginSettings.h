// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include "HermesUriSchemeBranchReplacementPluginSettings.generated.h"

USTRUCT()
struct FTokenReplacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere)
	FString Token;

	UPROPERTY(EditAnywhere)
	FString Replacement;
};

UCLASS(Config=Editor, DefaultConfig, meta = (DisplayName = "Hermes URLs - Branch Based Scheme"))
class UHermesUriSchemeBranchReplacementPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, meta = (
		DisplayName = "Branch String Replacements",
		ToolTip =
		"The scheme to use for our URIs if there is no UriSchemeProvider -- should be unique to each project / branch",
		ConfigRestartRequired = true))
	TArray<FTokenReplacement> Replacements;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString BranchName;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString SchemePreview;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString ExampleUri;

public:
	UHermesUriSchemeBranchReplacementPluginSettings(const FObjectInitializer& ObjectInitializer);
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

	virtual FName GetCategoryName() const override
	{
		return FName(TEXT("Plugins"));
	}

	TOptional<FString> GetScheme() const;

private:
	void UpdatePreview();
};
