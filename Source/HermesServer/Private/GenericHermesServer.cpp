// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include <PlatformHttp.h>

DEFINE_LOG_CATEGORY(LogHermesServer);
DECLARE_STATS_GROUP(TEXT("HermesServer"), STATGROUP_HermesServer, STATCAT_Advanced);

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

	FRegisteredEndpoint& RegisteredEndpoint = Endpoints[Endpoints.Emplace()];
	RegisteredEndpoint.Name = Endpoint;
	RegisteredEndpoint.Delegate = Delegate;
}

void FGenericHermesServer::Unregister(FName Endpoint)
{
	const int32 NumRemoved = Endpoints.RemoveAll([Endpoint](const FRegisteredEndpoint& RegisteredEndpoint)
	{
		return RegisteredEndpoint == Endpoint;
	});
	ensureAlwaysMsgf(NumRemoved > 0,
	                 TEXT(
		                 "Unregistering endpoint %s which hasn't been registered, is this being unintentionally called twice (or are you forgetting to unregister)?"
	                 ), *Endpoint.ToString());
}

FString FGenericHermesServer::GetUrl(FName Endpoint, const FString& Path)
{
	return FString::Printf(TEXT("%s://%s/%s/%s"), GetProtocol(), GetHostname(), *Endpoint.ToString(), *Path);
}

void FGenericHermesServer::Tick(float DeltaTime)
{
	if (!bHasCheckedLaunchPath)
	{
		FString LaunchPath;
		if (FParse::Value(FCommandLine::Get(), TEXT("-HermesPath="), LaunchPath))
		{
			HandlePath(LaunchPath);
		}
	}
	bHasCheckedLaunchPath = true;
}

const TCHAR* FGenericHermesServer::GetProtocol() const
{
	// TODO: Configurable in INI
	return TEXT("hermes");
}

const TCHAR* FGenericHermesServer::GetHostname() const
{
	// TODO: Configurable -- INI, or project-specific override
	return FApp::GetProjectName();
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

	// TODO: Should we forward URL fragments too?

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
		return;
	}

	Endpoint->Delegate.Execute(Path, QueryParameters);
}
