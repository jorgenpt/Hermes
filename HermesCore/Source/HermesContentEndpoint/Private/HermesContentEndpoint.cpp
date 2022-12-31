// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "HermesContentEndpoint.h"

#include "HermesContentEndpointEditorExtension.h"

#include <AssetRegistry/AssetRegistryModule.h>
#include <ContentBrowserModule.h>
#include <CoreMinimal.h>
#include <CoreUObject.h>
#include <HermesServer.h>
#include <IContentBrowserSingleton.h>
#include <MainFrame.h>
#include <Subsystems/AssetEditorSubsystem.h>

#define LOCTEXT_NAMESPACE "Editor.HermesContentEndpoint"

DEFINE_LOG_CATEGORY_STATIC(LogHermesContentEndpoint, Log, All);

const FName NAME_EndpointId(TEXT("content"));

struct FPendingRequest
{
	FString Path;
	FHermesQueryParamsMap Query;
};

struct FHermesContentEndpointModule : IModuleInterface
{
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;

	void OnAssetRegistryFilesLoaded();
	void OnRequest(const FString& Path, const FHermesQueryParamsMap& QueryParams);

	TArray<FPendingRequest> PendingRequests;
	FDelegateHandle AssetRegistryLoadedDelegateHandle;
	FHermesContentEndpointEditorExtension EditorExtension;
};

IMPLEMENT_MODULE(FHermesContentEndpointModule, HermesContentEndpoint);

void FHermesContentEndpointModule::StartupModule()
{
	// Register a "post-loading" callback if the asset registry is currently loading
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogHermesContentEndpoint, Verbose,
		       TEXT("Asset registry is currently loading, setting up callback for when it finishes"));
		AssetRegistryLoadedDelegateHandle = AssetRegistry.OnFilesLoaded().AddRaw(
			this, &FHermesContentEndpointModule::OnAssetRegistryFilesLoaded);
	}

	IHermesServerModule& Hermes = FModuleManager::LoadModuleChecked<IHermesServerModule>("HermesServer");
	Hermes.Register(NAME_EndpointId, FHermesOnRequest::CreateRaw(this, &FHermesContentEndpointModule::OnRequest));

	EditorExtension.InstallContentBrowserExtension();
	EditorExtension.InstallAssetEditorExtension();
}

void FHermesContentEndpointModule::ShutdownModule()
{
	EditorExtension.UninstallAssetEditorExtension();
	EditorExtension.UninstallContentBrowserExtension();

	if (auto Hermes = FModuleManager::GetModulePtr<IHermesServerModule>("HermesServer"))
	{
		Hermes->Unregister(NAME_EndpointId);
	}

	if (AssetRegistryLoadedDelegateHandle.IsValid())
	{
		if (IAssetRegistry* AssetRegistry = IAssetRegistry::Get())
		{
			AssetRegistry->OnFilesLoaded().Remove(AssetRegistryLoadedDelegateHandle);
		}

		AssetRegistryLoadedDelegateHandle.Reset();
	}
}

void FHermesContentEndpointModule::OnAssetRegistryFilesLoaded()
{
	UE_LOG(LogHermesContentEndpoint, Verbose, TEXT("Finished loading asset registry, processing %d pending requests"),
	       PendingRequests.Num());

	AssetRegistryLoadedDelegateHandle.Reset();

	// Process any requests that came in while we were loading
	TArray<FPendingRequest> Requests(MoveTemp(PendingRequests));
	for (const FPendingRequest& Request : Requests)
	{
		OnRequest(Request.Path, Request.Query);
	}
}

void FHermesContentEndpointModule::OnRequest(const FString& Path, const FHermesQueryParamsMap& QueryParams)
{
	// If the asset registry is still loading assets, put this in the queue for when it's done
	IAssetRegistry& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	if (AssetRegistry.IsLoadingAssets())
	{
		UE_LOG(LogHermesContentEndpoint, Verbose,
		       TEXT("Received request for %s while loading asset registry, putting in queue"), *Path);
		FPendingRequest& Request = PendingRequests.AddDefaulted_GetRef();
		Request.Path = Path;
		Request.Query = QueryParams;
		return;
	}

	TArray<FAssetData> AssetData;
	AssetRegistry.GetAssetsByPackageName(*Path, AssetData);
	if (AssetData.Num() > 0)
	{
		// Since this is a valid asset, either open it or edit it
		const bool bShouldEdit = QueryParams.Contains("edit");
		if (bShouldEdit)
		{
			UE_LOG(LogHermesContentEndpoint, Verbose, TEXT("Opening %s for editing"), *Path);
			UAssetEditorSubsystem* AssetEditorSubsystem = GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();
			AssetEditorSubsystem->OpenEditorForAsset(AssetData[0].GetAsset());
		}
		else
		{
			UE_LOG(LogHermesContentEndpoint, Verbose, TEXT("Focusing %s in content browser"), *Path);

			IContentBrowserSingleton& ContentBrowser = FModuleManager::LoadModuleChecked<FContentBrowserModule>(
				"ContentBrowser").Get();

			const bool bAllowLockedBrowsers = false;
			const bool bFocusContentBrowser = true;
			ContentBrowser.SyncBrowserToAssets(AssetData, bAllowLockedBrowsers, bFocusContentBrowser);
		}
	}
	else
	{
		UE_LOG(LogHermesContentEndpoint, Error, TEXT("Couldn't find any assets for %s"), *Path);
	}

	IMainFrameModule& MainFrameModule = IMainFrameModule::Get();
	TSharedPtr<SWindow> ParentWindow = MainFrameModule.GetParentWindow();
	if (ParentWindow.IsValid())
	{
		// Bring the main frame Slate window into focus
		ParentWindow->ShowWindow();
		// Use this hacky API to bring the OS-level window forward
		ParentWindow->GetNativeWindow()->HACK_ForceToFront();
	}
}

#undef LOCTEXT_NAMESPACE
