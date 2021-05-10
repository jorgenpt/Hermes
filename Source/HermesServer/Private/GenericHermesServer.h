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
protected: // Implementation of IHermesServerModule
	virtual void Register(FName Endpoint, FHermesOnRequest Delegate) final override;
	virtual void Unregister(FName Endpoint) final override;
	virtual FString GetUrl(FName Endpoint, const FString& Path) final override;

protected: // Implementation of FTickableEditorObject
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const final override;
	virtual bool IsTickable() const final override;
	virtual ETickableTickType GetTickableTickType() const final override;

private: // State
	bool bHasCheckedLaunchURL = false;
	TArray<FRegisteredEndpoint> Endpoints;

protected: // API for platform implementations
	const TCHAR* GetProtocol() const;
	const TCHAR* GetHostName() const;
	void HandleUrl(const FString& Url) const;
};
