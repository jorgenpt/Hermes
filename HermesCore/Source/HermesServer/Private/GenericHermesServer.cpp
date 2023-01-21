// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include "HermesPluginSettings.h"
#include "HermesUriSchemeProvider.h"

#include <Features/IModularFeatures.h>
#include <Misc/CommandLine.h>
#include <Misc/ConfigCacheIni.h>
#include <PlatformHttp.h>

DEFINE_LOG_CATEGORY(LogHermesServer);
DECLARE_STATS_GROUP(TEXT("HermesServer"), STATGROUP_HermesServer, STATCAT_Advanced);

void FGenericHermesServer::StartupModule()
{
	RefreshRegisteredScheme();

	auto OnModularFeaturesChanged = [&](const FName& Type, class IModularFeature*)
	{
		if (Type == IHermesUriSchemeProvider::GetModularFeatureName())
		{
			RefreshRegisteredScheme();
		}
	};

	IModularFeatures& Features = IModularFeatures::Get();
	OnModularFeatureRegisteredHandle = Features.OnModularFeatureRegistered().AddLambda(OnModularFeaturesChanged);
	OnModularFeatureUnregisteredHandle = Features.OnModularFeatureUnregistered().AddLambda(OnModularFeaturesChanged);
}

void FGenericHermesServer::ShutdownModule()
{
	IModularFeatures& Features = IModularFeatures::Get();
	Features.OnModularFeatureRegistered().Remove(OnModularFeatureRegisteredHandle);
	Features.OnModularFeatureUnregistered().Remove(OnModularFeatureUnregisteredHandle);
}

void FGenericHermesServer::Tick(float DeltaTime)
{
	if (!bFullyInitialized)
	{
		bFullyInitialized = true;

		// Any modular features should've been registered by now, so refresh the scheme and ignore the saved LastScheme
		RefreshRegisteredScheme();

		FString LaunchPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-HermesPath="), LaunchPath))
		{
			UE_LOG(LogHermesServer, Verbose, TEXT("Handling command line path %s"), *LaunchPath);
			HandlePath(LaunchPath);
		}
	}
}

TStatId FGenericHermesServer::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(FGenericHermesServer_Tick, STATGROUP_HermesServer);
}

bool FGenericHermesServer::IsTickable() const
{
	return true;
}

ETickableTickType FGenericHermesServer::GetTickableTickType() const
{
	return ETickableTickType::Always;
}

void FGenericHermesServer::Register(FName Endpoint, FHermesOnRequest Delegate)
{
	UE_LOG(LogHermesServer, Verbose, TEXT("Registering handler for endpoint %s"), *Endpoint.ToString());
	if (!ensureAlwaysMsgf(!Endpoints.Contains(Endpoint),
	                      TEXT(
		                      "Registering duplicate delegate for endpoint %s, is this being unintentionally called twice (or are you forgetting to unregister)?"
	                      ), *Endpoint.ToString()))
	{
		Endpoints.RemoveAll([Endpoint](const FRegisteredEndpoint& RegisteredEndpoint)
		{
			return RegisteredEndpoint == Endpoint;
		});
	}

	FRegisteredEndpoint& RegisteredEndpoint = Endpoints.Emplace_GetRef();
	RegisteredEndpoint.Name = Endpoint;
	RegisteredEndpoint.Delegate = Delegate;
}

void FGenericHermesServer::Unregister(FName Endpoint)
{
	UE_LOG(LogHermesServer, Verbose, TEXT("Unregistering handler for endpoint %s"), *Endpoint.ToString());
	const int32 NumRemoved = Endpoints.RemoveAll([Endpoint](const FRegisteredEndpoint& RegisteredEndpoint)
	{
		return RegisteredEndpoint == Endpoint;
	});
	ensureAlwaysMsgf(NumRemoved > 0,
	                 TEXT(
		                 "Unregistering endpoint %s which hasn't been registered, is this being unintentionally called twice (or are you forgetting to unregister)?"
	                 ), *Endpoint.ToString());
}

FString FGenericHermesServer::GetUri(FName Endpoint, const FString& Path)
{
	if (PreviouslyRegisteredScheme.IsSet())
	{
		FString ModifiedPath(Path);
		ModifiedPath.RemoveFromStart(TEXT("/"));
		return FString::Printf(TEXT("%s://%s/%s"), *PreviouslyRegisteredScheme.GetValue(), *Endpoint.ToString(),
		                       *ModifiedPath);
	}

	return FString();
}

void FGenericHermesServer::HandlePath(const FString& FullPath) const
{
	UE_LOG(LogHermesServer, Display, TEXT("Dispatching path '%s'"), *FullPath);

	// Identify the endpoint -- the first path component -- which decides where we route this path.
	const TCHAR* EndpointNameBeg = *FullPath;
	if (*EndpointNameBeg == TEXT('/'))
	{
		EndpointNameBeg++;
	}
	const TCHAR* EndpointNameEnd = FCString::Strchr(EndpointNameBeg, TEXT('/'));
	if (EndpointNameEnd == nullptr)
	{
		// If there's no specific path underneath the endpoint, we'll just pass an empty path to the handler
		EndpointNameEnd = EndpointNameBeg + FCString::Strlen(EndpointNameBeg);
	}
	const FString EndpointName(EndpointNameEnd - EndpointNameBeg, EndpointNameBeg);

	// The rest of it is the endpoint-specific subpath, unless there's a query string (?foo=bar)
	const TCHAR* PathBeg = EndpointNameEnd;
	const TCHAR* PathEnd = FCString::Strchr(PathBeg, TEXT('?'));
	if (PathEnd == nullptr)
	{
		// If there are no query parameters, use the rest of the string as the path
		PathEnd = PathBeg + FCString::Strlen(PathBeg);
	}
	const FString Path = FPlatformHttp::UrlDecode(FString(PathEnd - PathBeg, PathBeg));

	// Extract the query parameters into a TMap, to make it easier for various endpoints to use them
	TMap<FString, FString> QueryParameters;
	if (*PathEnd == TEXT('?'))
	{
		const FString QueryString(PathEnd + 1);
		TArray<FString> QueryParameterComponents;
		QueryString.ParseIntoArray(QueryParameterComponents, TEXT("&"), true);
		for (const FString& QueryParameter : QueryParameterComponents)
		{
			int32 EndOfKey = INDEX_NONE;
			// Support both foo=bar and just foo, the latter will just be an empty string in the map
			if (!QueryParameter.FindChar(TEXT('='), EndOfKey))
			{
				QueryParameters.Emplace(FPlatformHttp::UrlDecode(QueryParameter).ToLower(), TEXT(""));
			}
			else
			{
				QueryParameters.Emplace(FPlatformHttp::UrlDecode(QueryParameter.Left(EndOfKey)).ToLower(),
				                        FPlatformHttp::UrlDecode(QueryParameter.Mid(EndOfKey + 1)));
			}
		}
	}

	UE_LOG(LogHermesServer, Verbose, TEXT("Parsed path:\n  - Endpoint '%s'\n  - Subpath '%s'\n  - %i parameter(s):"),
	       *EndpointName, *Path, QueryParameters.Num());
	for (const auto& Pair : QueryParameters)
	{
		UE_LOG(LogHermesServer, Verbose, TEXT("    - '%s' = '%s'"), *Pair.Key, *Pair.Value);
	}

	const FRegisteredEndpoint* Endpoint = Endpoints.FindByKey(FName(*EndpointName));
	if (Endpoint == nullptr)
	{
		// TODO: If I implement blueprint handlers, we probably want to defer dispatch here if we haven't discovered
		// all the blueprints yet.
		UE_LOG(LogHermesServer, Error,
		       TEXT("There is no handler registered for the endpoint '%s' in path '%s'"), *EndpointName, *FullPath);
	}
	else
	{
		Endpoint->Delegate.Execute(Path, QueryParameters);
	}
}

void FGenericHermesServer::RefreshRegisteredScheme()
{
	// Don't update the schema if we're shutting down.
	if (!GIsRunning)
	{
		return;
	}

	const FName FeatureName(IHermesUriSchemeProvider::GetModularFeatureName());
	IModularFeatures& Features = IModularFeatures::Get();
	TArray<IHermesUriSchemeProvider*> Providers = Features.GetModularFeatureImplementations<
		IHermesUriSchemeProvider>(FeatureName);

	FString LastScheme;
	GConfig->GetString(
		TEXT("/Script/HermesServer.HermesPluginSettings"), TEXT("LastScheme"), LastScheme,
		GEditorPerProjectIni);

	// First prefer providers, in order of registration
	TOptional<FString> PickedScheme;
	for (IHermesUriSchemeProvider* Provider : Providers)
	{
		TOptional<FString> Scheme = Provider->GetPreferredScheme();
		if (Scheme.IsSet())
		{
			PickedScheme = Scheme;
			break;
		}
	}

	// We only use the LastScheme if we're not fully initialized, so that we can do an "early initialization" until all
	// the modular features have a chance to register.
	if (!PickedScheme.IsSet() && !bFullyInitialized && !LastScheme.IsEmpty())
	{
		PickedScheme = LastScheme;
	}

	const auto* Settings = GetDefault<UHermesPluginSettings>();

	// Finally, try using the hard coded setting
	if (!PickedScheme.IsSet())
	{
		FString DefaultScheme = Settings->DefaultUriScheme;
		if (DefaultScheme.IsEmpty())
		{
			DefaultScheme = TEXT("hunreal");
		}

		PickedScheme = DefaultScheme;
	}

	// Unregister the scheme from our last boot if we're not using it any more,
	// and update the 'last scheme' cached value for our next boot.
	if (PickedScheme != LastScheme)
	{
		if (!LastScheme.IsEmpty())
		{
			UnregisterScheme(*LastScheme);
		}

		if (PickedScheme.IsSet())
		{
			GConfig->SetString(
				TEXT("/Script/HermesServer.HermesPluginSettings"), TEXT("LastScheme"), **PickedScheme,
				GEditorPerProjectIni);
		}
		else
		{
			GConfig->RemoveKey(
				TEXT("/Script/HermesServer.HermesPluginSettings"), TEXT("LastScheme"), GEditorPerProjectIni);
		}
	}

	if (PickedScheme.IsSet())
	{
		UpdateScheme(*PickedScheme, Settings->bDebug);
	}
}

void FGenericHermesServer::UpdateScheme(const FString& Scheme, bool bDebug)
{
	if (PreviouslyRegisteredScheme.IsSet())
	{
		if (PreviouslyRegisteredScheme == Scheme)
		{
			return;
		}

		UnregisterScheme(**PreviouslyRegisteredScheme);
		PreviouslyRegisteredScheme.Reset();
	}

	if (RegisterScheme(*Scheme, bDebug))
	{
		PreviouslyRegisteredScheme = Scheme;
	}
}
