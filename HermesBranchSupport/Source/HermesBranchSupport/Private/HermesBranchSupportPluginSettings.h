// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <CoreMinimal.h>
#include <Engine/DeveloperSettings.h>
#include "HermesBranchSupportPluginSettings.generated.h"

USTRUCT()
struct FTokenReplacement
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Token Replacement")
	FString Token;

	UPROPERTY(EditAnywhere, Category = "Token Replacement")
	FString Replacement;
};

UCLASS(Config=Editor, DefaultConfig, meta = (DisplayName = "Hermes URLs - Branch Based URLs"))
class UHermesBranchSupportPluginSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config, EditAnywhere, Category = "Replacements", meta = (
		DisplayName = "Branch String Replacements",
		ToolTip =
		"Tokens that we should replace in the branch name returned by Unreal",
		ConfigRestartRequired = true))
	TArray<FTokenReplacement> Replacements;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString BranchName;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString SchemePreview;

	UPROPERTY(Transient, VisibleAnywhere, Category = "Preview")
	FString ExampleUri;

public:
	UHermesBranchSupportPluginSettings(const FObjectInitializer& ObjectInitializer);
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
