// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#pragma once
#include "HermesServer.h"

#include <Containers/UnrealString.h>
#include <TickableEditorObject.h>

struct FRegisteredEndpoint
{
	FName Name;
	FHermesOnRequest Delegate;

	bool operator==(const FName& Endpoint) const
	{
		return Name == Endpoint;
	}
};

class FGenericHermesServer : public IHermesServerModule, public FTickableEditorObject
{
protected: // Implementation of IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

protected: // Implementation of FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const final override;
	virtual bool IsTickable() const final override;
	virtual ETickableTickType GetTickableTickType() const final override;

protected: // Implementation of IHermesServerModule
	virtual void Register(FName Endpoint, FHermesOnRequest Delegate) final override;
	virtual void Unregister(FName Endpoint) final override;
	virtual FString GetUri(FName Endpoint, const FString& Path) final override;

private: // State
	bool bFullyInitialized = false;
	TArray<FRegisteredEndpoint> Endpoints;
	TOptional<FString> PreviouslyRegisteredScheme;
	FDelegateHandle OnModularFeatureRegisteredHandle;
	FDelegateHandle OnModularFeatureUnregisteredHandle;

protected: // Interface for platform implementations
	/** Register ourselves for the given scheme with the OS handler. */
	virtual bool RegisterScheme(const TCHAR* Scheme, bool bDebug) = 0;
	/** Remove a previous registration for the given scheme. */
	virtual void UnregisterScheme(const TCHAR* Scheme) = 0;

protected: // API for platform implementations
	/** Dispatch the given path to the correct endpoint handler */
	void HandlePath(const FString& FullPath) const;

private: // Implementation details
	/**
	 * If it's different from our previously registered scheme, configure this one as our current one. Unregisters the
	 * previous scheme, if one has been registered.
	 */
	void UpdateScheme(const FString& Scheme, bool bDebug);
	/**
	 * Pick the best scheme, preferring scheme providers first, then the scheme provider from our last run (if any),
	 * then the scheme configured in the settings.
	 */
	void RefreshRegisteredScheme();
};
