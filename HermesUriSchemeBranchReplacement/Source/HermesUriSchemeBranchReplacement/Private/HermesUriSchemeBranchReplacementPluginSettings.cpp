// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "HermesUriSchemeBranchReplacementPluginSettings.h"

#include <Misc/App.h>

#include "Hermes.h"

UHermesUriSchemeBranchReplacementPluginSettings::UHermesUriSchemeBranchReplacementPluginSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	const FString SourceBranchName = FApp::GetBranchName();

	if (Replacements.Num() == 0)
	{
		int32 LastPlusIndex = 0;
		SourceBranchName.FindLastChar(TEXT('+'), LastPlusIndex);
		if (SourceBranchName.StartsWith(TEXT("++")) && LastPlusIndex > 2 && LastPlusIndex < SourceBranchName.Len() - 1)
		{
			FTokenReplacement& InitialReplacement = Replacements.AddDefaulted_GetRef();
			InitialReplacement.Token = SourceBranchName.Left(LastPlusIndex + 1);
			InitialReplacement.Replacement = Hermes::SanitizeScheme(FApp::GetProjectName()).Get(TEXT("hue4")) + TEXT('-');
		}
	}
}

void UHermesUriSchemeBranchReplacementPluginSettings::PostInitProperties()
{
	Super::PostInitProperties();
	BranchName = FApp::GetBranchName();
	UpdatePreview();
}

void UHermesUriSchemeBranchReplacementPluginSettings::PostEditChangeProperty(
	FPropertyChangedEvent& PropertyChangedEvent)
{
	UpdatePreview();
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

TOptional<FString> UHermesUriSchemeBranchReplacementPluginSettings::GetScheme() const
{
	FString Scheme(FApp::GetBranchName());
	for (const auto& Replacement : Replacements)
	{
		Scheme.ReplaceInline(*Replacement.Token, *Replacement.Replacement);
	}
	return Hermes::SanitizeScheme(Scheme);
}

void UHermesUriSchemeBranchReplacementPluginSettings::UpdatePreview()
{
	TOptional<FString> Scheme = GetScheme();
	if (Scheme.IsSet())
	{
		SchemePreview = *Scheme;
		ExampleUri = FString::Printf(TEXT("%s://content/Game/Spells/Fireball?edit"), **Scheme);
	}
	else
	{
		SchemePreview = TEXT("<no valid characters in scheme>");
		ExampleUri = TEXT("N/A");
	}
}
