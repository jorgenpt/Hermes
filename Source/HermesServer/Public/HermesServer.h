// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include "CoreMinimal.h"

DECLARE_LOG_CATEGORY_EXTERN(LogHermesServer, Log, All);

typedef TMap<FString, FString> FHermesQueryParamsMap;
DECLARE_DELEGATE_TwoParams(FHermesOnRequest, const FString& /* Path */, const FHermesQueryParamsMap& /* QueryParams */);

struct IHermesServerModule
{
	virtual ~IHermesServerModule() = default;

	virtual void Register(FName Endpoint, FHermesOnRequest Delegate) = 0;
	virtual void Unregister(FName Endpoint) = 0;
	virtual FString GetUrl(FName Endpoint, const FString& Path = TEXT("")) = 0;
};
