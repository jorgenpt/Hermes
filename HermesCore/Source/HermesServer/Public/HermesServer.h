// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleInterface.h>

DECLARE_LOG_CATEGORY_EXTERN(LogHermesServer, Log, All);

typedef TMap<FString, FString> FHermesQueryParamsMap;
DECLARE_DELEGATE_TwoParams(FHermesOnRequest, const FString& /* Path */, const FHermesQueryParamsMap& /* QueryParams */);

struct IHermesServerModule : IModuleInterface
{
	/**
	 * Register a handler for a specific endpoint. The handler will receive a callback for any path underneath the
	 * specified endpoint id.
	 *
	 * @param Endpoint an identifier for your endpoint, must be unique
	 * @param Delegate the callback that is invoked when there's an URI opened
	 * @see Unregister
	 */
	virtual void Register(FName Endpoint, FHermesOnRequest Delegate) = 0;

	/**
	* Unregister a handler for a specific endpoint. Will ensure if the endpoint hasn't been unregistered
	*
	* @param Endpoint the identifier for your endpoint that was previously passed to Register
	* @see Register
	*/
	virtual void Unregister(FName Endpoint) = 0;

	/**
	 * Generate URI that'll be passed to the given endpoint.
	 *
	 * @param Endpoint the identifier for a specific endpoint, usually your own
	 * @param Path a path that is passed to the endpoint, can be empty
	 */
	virtual FString GetUri(FName Endpoint, const FString& Path = TEXT("")) = 0;
};
