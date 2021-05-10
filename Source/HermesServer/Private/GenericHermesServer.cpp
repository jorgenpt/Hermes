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
	return FString::Printf(TEXT("%s://%s/%s/%s"), GetProtocol(), GetHostName(), *Endpoint.ToString(), *Path);
}

void FGenericHermesServer::Tick(float DeltaTime)
{
	if (!bHasCheckedLaunchURL)
	{
		FString LaunchURL;
		if (FParse::Value(FCommandLine::Get(), TEXT("-HermesURL="), LaunchURL))
		{
			HandleUrl(LaunchURL);
		}
	}
	bHasCheckedLaunchURL = true;
}

const TCHAR* FGenericHermesServer::GetProtocol() const
{
	// TODO: Configurable in INI
	return TEXT("hermes");
}

const TCHAR* FGenericHermesServer::GetHostName() const
{
	// TODO: Configurable -- INI, or project-specific override
	return FApp::GetProjectName();
}

void FGenericHermesServer::HandleUrl(const FString& Url) const
{
	UE_LOG(LogHermesServer, Display, TEXT("Dispatching URL '%s'"), *Url);

	// Find the protocol -- everything up to ://
	const TCHAR* ProtocolBeg = *Url;
	const TCHAR* ProtocolEnd = FCString::Strstr(ProtocolBeg, TEXT("://"));
	if (!ensureAlwaysMsgf(ProtocolEnd != nullptr,
	                      TEXT("Received invalid URL '%s', expected '://' after protocol."), *Url))
	{
		UE_LOG(LogHermesServer, Error, TEXT("URL '%s' is malformed. Could not extract the protocol."), *Url);
		return;
	}

	// Make sure it's our local protocol!
	const FString Protocol(ProtocolEnd - ProtocolBeg, ProtocolBeg);
	if (!ensureAlwaysMsgf(Protocol.Equals(GetProtocol(), ESearchCase::IgnoreCase),
	                      TEXT("Received invalid URL '%s', expected protocol '%s'."), *Url, GetProtocol()))
	{
		UE_LOG(LogHermesServer, Error, TEXT("URL '%s' has an unexpected protocol '%s', expected '%s'."), *Url,
		       *Protocol, GetProtocol());
		return;
	}

	// Find the host between the :// and the first /
	const TCHAR* HostBeg = ProtocolEnd + FCString::Strlen(TEXT("://"));
	const TCHAR* HostEnd = FCString::Strchr(HostBeg, TEXT('/'));
	if (!ensureAlwaysMsgf(HostEnd != nullptr, TEXT("Received invalid URL '%s', expected a slash after hostname."),
	                      *Url))
	{
		UE_LOG(LogHermesServer, Error, TEXT("URL '%s' is malformed. Could not extract the host."), *Url);
		return;
	}

	// Make sure it's our local "hostname"!
	const FString Host(HostEnd - HostBeg, HostBeg);
	if (!ensureAlwaysMsgf(Host.Equals(GetHostName(), ESearchCase::IgnoreCase),
	                      TEXT("Received invalid URL '%s', expected host name '%s'."), *Url, GetHostName()))
	{
		UE_LOG(LogHermesServer, Error, TEXT("URL '%s' has an unexpected host '%s', expected '%s'"), *Url, *Host,
		       GetHostName());
		return;
	}

	// Identify the endpoint -- the first path component -- which decides where we route this URL.
	const TCHAR* EndpointNameBeg = HostEnd + 1;
	const TCHAR* EndpointNameEnd = FCString::Strchr(EndpointNameBeg, TEXT('/'));
	if (EndpointNameEnd == nullptr)
	{
		// If there's no specific path underneath the endpoint, we'll just pass an empty path to the handler
		EndpointNameEnd = EndpointNameBeg + FCString::Strlen(EndpointNameBeg);
	}
	const FString EndpointName(EndpointNameEnd - EndpointNameBeg, EndpointNameBeg);

	// The rest of the URL is the path, unless there's a query string (?foo=bar)
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

	UE_LOG(LogHermesServer, Verbose, TEXT("Parsed URL:\n  - Endpoint '%s'\n  - Path '%s'\n  - %i parameter(s):"),
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
		       TEXT("There is no handler registered for the endpoint '%s' in URL '%s'"), *EndpointName, *Url);
		return;
	}

	Endpoint->Delegate.Execute(Path, QueryParameters);
}
