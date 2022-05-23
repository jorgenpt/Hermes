// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <ContentBrowserModule.h>
#include <CoreMinimal.h>

class FExtender;
class FMenuBuilder;
class FSlateStyleSet;
class FUICommandList;
struct FAssetData;
struct FToolMenuSection;

struct FHermesContentEndpointEditorExtension
{
	void InstallContentBrowserExtension();
	void UninstallContentBrowserExtension();

	void InstallAssetEditorExtension();
	void UninstallAssetEditorExtension();

private:
	static void CopyEndpointURLsToClipboard(TArray<FName> Packages, const TCHAR* OptionalSuffix = nullptr);

	static TSharedRef<FExtender> OnExtendContentBrowserAssetSelectionMenu(const TArray<FAssetData>& SelectedAssets);
	static void OnExtendContentBrowserCommands(TSharedRef<FUICommandList> CommandList,
	                                           FOnContentBrowserGetSelection GetSelectionDelegate);

	static void CreateAssetContextMenu(FToolMenuSection& InSection);
	static TSharedRef<FExtender> OnExtendAssetEditor(const TSharedRef<FUICommandList> CommandList,
	                                                 const TArray<UObject*> ContextSensitiveObjects);

private:
	FDelegateHandle ContentBrowserAssetExtenderDelegateHandle;
	FDelegateHandle ContentBrowserCommandExtenderDelegateHandle;

	FDelegateHandle AssetEditorExtenderDelegateHandle;

	TSharedPtr<FSlateStyleSet> SlateStyle;
};
