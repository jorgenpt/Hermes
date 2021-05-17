// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "HermesContentEndpointEditorExtension.h"

#include "HermesContentEndpoint.h"

#include <HAL/PlatformApplicationMisc.h>
#include <HermesServer.h>
#include <ToolMenus.h>
#include <Toolkits/AssetEditorToolkitMenuContext.h>

#define LOCTEXT_NAMESPACE "Editor.HermesContentEndpointEditorExtension"

struct FHermesContentEndpointEditorCommands : public TCommands<FHermesContentEndpointEditorCommands>
{
	FHermesContentEndpointEditorCommands();
	virtual void RegisterCommands() override;

	// Copies an URL to show the given asset in the content browser
	TSharedPtr<FUICommandInfo> CopyRevealURL;

	// Copies an URL to edit the given asset
	TSharedPtr<FUICommandInfo> CopyEditURL;
};

FHermesContentEndpointEditorCommands::FHermesContentEndpointEditorCommands()
	: TCommands<FHermesContentEndpointEditorCommands>(
		"HermesContentEndpointEditorExtensions",
		NSLOCTEXT("Contexts", "Commands", "Hermes Content Endpoint Editor Extensions"),
		NAME_None,
		FEditorStyle::GetStyleSetName()
	)
{
}

void FHermesContentEndpointEditorCommands::RegisterCommands()
{
	UI_COMMAND(CopyRevealURL, "Copy URL", "Copy an URL that'll reveal this asset in the content browser.",
	           EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::C));
	UI_COMMAND(CopyEditURL, "Copy URL", "Copy an URL that'll open this asset for editing.",
	           EUserInterfaceActionType::Button, FInputChord(EModifierKey::Alt | EModifierKey::Shift, EKeys::E));
}

void FHermesContentEndpointEditorExtension::CopyEndpointURLsToClipboard(TArray<FName> Packages,
                                                                     const TCHAR* OptionalSuffix)
{
	if (Packages.Num() == 0)
	{
		return;
	}

	IHermesServerModule& Hermes = FModuleManager::LoadModuleChecked<IHermesServerModule>("HermesServer");

	// Get the URL for each package and concatenate them with newlines in between
	FString ClipboardText;
	for (const FName& Package : Packages)
	{
		ClipboardText += Hermes.GetUri(NAME_EndpointId, *Package.ToString());
		if (OptionalSuffix)
		{
			ClipboardText += OptionalSuffix;
		}
		ClipboardText += LINE_TERMINATOR;
	}

	// Strip the final newline
	const int32 LineTerminatorLen = FCString::Strlen(LINE_TERMINATOR);
	checkf(ClipboardText.EndsWith(LINE_TERMINATOR), TEXT("There should always be at least one line terminator!"));
	ClipboardText.RemoveAt(ClipboardText.Len() - LineTerminatorLen, LineTerminatorLen, false);

	FPlatformApplicationMisc::ClipboardCopy(*ClipboardText);
}

void FHermesContentEndpointEditorExtension::InstallContentBrowserExtension()
{
	FHermesContentEndpointEditorCommands::Register();

	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(
		TEXT("ContentBrowser"));

	// Set up a callback to register our CopyRevealURL command when needed
	TArray<FContentBrowserCommandExtender>& ContentBrowserCommandExtenders = ContentBrowserModule.
		GetAllContentBrowserCommandExtenders();
	ContentBrowserCommandExtenders.Add(FContentBrowserCommandExtender::CreateStatic(&OnExtendContentBrowserCommands));
	ContentBrowserCommandExtenderDelegateHandle = ContentBrowserCommandExtenders.Last().GetHandle();

	// Set up a callback for whenever the context menu is generated to add our CopyRevealURL command
	TArray<FContentBrowserMenuExtender_SelectedAssets>& ContentBrowserAssetContextMenuExtenders = ContentBrowserModule.
		GetAllAssetViewContextMenuExtenders();
	ContentBrowserAssetContextMenuExtenders.Add(
		FContentBrowserMenuExtender_SelectedAssets::CreateStatic(&OnExtendContentBrowserAssetSelectionMenu));
	ContentBrowserAssetExtenderDelegateHandle = ContentBrowserAssetContextMenuExtenders.Last().GetHandle();
}

void FHermesContentEndpointEditorExtension::UninstallContentBrowserExtension()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(
		TEXT("ContentBrowser"));

	TArray<FContentBrowserMenuExtender_SelectedAssets>& ContentBrowserAssetContextMenuExtenders = ContentBrowserModule.
		GetAllAssetViewContextMenuExtenders();
	ContentBrowserAssetContextMenuExtenders.RemoveAll([this](const FContentBrowserMenuExtender_SelectedAssets& Delegate)
	{
		return Delegate.GetHandle() == ContentBrowserAssetExtenderDelegateHandle;
	});
	ContentBrowserAssetExtenderDelegateHandle.Reset();

	TArray<FContentBrowserCommandExtender>& ContentBrowserCommandExtenders = ContentBrowserModule.
		GetAllContentBrowserCommandExtenders();
	ContentBrowserCommandExtenders.RemoveAll([this](const FContentBrowserCommandExtender& Delegate)
	{
		return Delegate.GetHandle() == ContentBrowserCommandExtenderDelegateHandle;
	});
	ContentBrowserCommandExtenderDelegateHandle.Reset();
}

void FHermesContentEndpointEditorExtension::OnExtendContentBrowserCommands(TSharedRef<FUICommandList> CommandList,
                                                                           FOnContentBrowserGetSelection
                                                                           GetSelectionDelegate)
{
	CommandList->MapAction(FHermesContentEndpointEditorCommands::Get().CopyRevealURL,
	                       FExecuteAction::CreateLambda([GetSelectionDelegate]
	                       {
		                       if (GetSelectionDelegate.IsBound())
		                       {
			                       TArray<FAssetData> SelectedAssets;
			                       TArray<FString> SelectedPaths;
			                       GetSelectionDelegate.Execute(SelectedAssets, SelectedPaths);

			                       TArray<FName> Packages;
			                       for (const auto& Asset : SelectedAssets)
			                       {
				                       Packages.AddUnique(Asset.PackageName);
			                       }
			                       CopyEndpointURLsToClipboard(Packages);
		                       }
	                       })
	);
}

TSharedRef<FExtender> FHermesContentEndpointEditorExtension::OnExtendContentBrowserAssetSelectionMenu(
	const TArray<FAssetData>& SelectedAssets)
{
	TSharedRef<FExtender> Extender(new FExtender());

	// Add our option after "Copy File Path" in the context menu
	Extender->AddMenuExtension(
		"CopyFilePath",
		EExtensionHook::After,
		nullptr,
		FMenuExtensionDelegate::CreateLambda([](FMenuBuilder& MenuBuilder)
		{
			MenuBuilder.AddMenuEntry(FHermesContentEndpointEditorCommands::Get().CopyRevealURL);
		})
	);

	return Extender;
}

void FHermesContentEndpointEditorExtension::InstallAssetEditorExtension()
{
	// This whole setup is adapted from Engine/Plugins/Editor/AssetManagerEditor/Source/AssetManagerEditor/Private/AssetManagerEditorModule.cpp

	TArray<FAssetEditorExtender>& AssetEditorMenuExtenderDelegates =
		FAssetEditorToolkit::GetSharedMenuExtensibilityManager()->GetExtenderDelegates();
	AssetEditorMenuExtenderDelegates.Add(FAssetEditorExtender::CreateStatic(&OnExtendAssetEditor));
	AssetEditorExtenderDelegateHandle = AssetEditorMenuExtenderDelegates.Last().GetHandle();

	// Locate the "Asset" menu in the tool menus
	UToolMenu* Menu = UToolMenus::Get()->ExtendMenu("MainFrame.MainMenu.Asset");
	// Find the "asset editor actions" section (which is the only one)
	FToolMenuSection& Section = Menu->FindOrAddSection("AssetEditorActions");
	// Add a new entry that adds the "copy to clipboard" option
	FToolMenuEntry& Entry = Section.AddDynamicEntry("ContentEndpointCommands",
	                                                FNewToolMenuSectionDelegate::CreateStatic(&CreateAssetContextMenu));
	// Position it after the "Find in Content Browser" option on that menu
	Entry.InsertPosition = FToolMenuInsert("FindInContentBrowser", EToolMenuInsertType::After);
}

void FHermesContentEndpointEditorExtension::UninstallAssetEditorExtension()
{
	TArray<FAssetEditorExtender>& AssetEditorMenuExtenderDelegates =
		FAssetEditorToolkit::GetSharedMenuExtensibilityManager()->GetExtenderDelegates();
	AssetEditorMenuExtenderDelegates.RemoveAll([this](const FAssetEditorExtender& Delegate)
	{
		return Delegate.GetHandle() == AssetEditorExtenderDelegateHandle;
	});
	AssetEditorExtenderDelegateHandle.Reset();
}

TSharedRef<FExtender> FHermesContentEndpointEditorExtension::OnExtendAssetEditor(
	const TSharedRef<FUICommandList> CommandList,
	const TArray<UObject*> ContextSensitiveObjects)
{
	TArray<FName> PackageNames;
	for (UObject* EditedAsset : ContextSensitiveObjects)
	{
		if (EditedAsset && EditedAsset->IsAsset() && !EditedAsset->IsPendingKill())
		{
			PackageNames.AddUnique(EditedAsset->GetOutermost()->GetFName());
		}
	}

	TSharedRef<FExtender> Extender(new FExtender());

	if (PackageNames.Num() > 0)
	{
		// Quote from FAssetManagerEditorModule::OnExtendAssetEditor:
		// - "It's safe to modify the CommandList here because this is run as the editor UI is created and the payloads are safe"
		CommandList->MapAction(
			FHermesContentEndpointEditorCommands::Get().CopyEditURL,
			FExecuteAction::CreateStatic(CopyEndpointURLsToClipboard, PackageNames, TEXT("?edit"))
		);
	}

	return Extender;
}


void FHermesContentEndpointEditorExtension::CreateAssetContextMenu(FToolMenuSection& InSection)
{
	// Check that we are actually editing a real asset before providing the Copy Edit URL option.
	UAssetEditorToolkitMenuContext* MenuContext = InSection.FindContext<UAssetEditorToolkitMenuContext>();
	if (MenuContext && MenuContext->Toolkit.IsValid() && MenuContext->Toolkit.Pin()->IsActuallyAnAsset())
	{
		for (const UObject* EditedAsset : *MenuContext->Toolkit.Pin()->GetObjectsCurrentlyBeingEdited())
		{
			if (EditedAsset && EditedAsset->IsAsset() && !EditedAsset->IsPendingKill())
			{
				InSection.AddMenuEntry(FHermesContentEndpointEditorCommands::Get().CopyEditURL);
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE
