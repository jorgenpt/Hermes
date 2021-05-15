// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include <Windows/AllowWindowsPlatformTypes.h>

struct FWindowsHermesServerModule : FGenericHermesServer, IModuleInterface
{
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;
	virtual void Tick(float DeltaTime) override final;

	HANDLE ServerHandle;
};

IMPLEMENT_MODULE(FWindowsHermesServerModule, HermesServer)

// Max message size is around the maximum path size (32k), plus 256 bytes for protocol, host, and query string.
static constexpr int32 MAX_MESSAGE_SIZE = 32 * 1024 + 256;

void FWindowsHermesServerModule::StartupModule()
{
	const FString MailslotName = FString::Printf(TEXT("\\\\.\\mailslot\\bitSpatter\\Hermes\\%s\\%s"), GetProtocol(), GetHostname());
	ServerHandle = CreateMailslot(*MailslotName, MAX_MESSAGE_SIZE, 0, nullptr);
	if (ServerHandle != INVALID_HANDLE_VALUE)
	{
		// TODO: Invoke the handler to get registration done
	}
	else
	{
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), 0);
		UE_LOG(LogHermesServer, Error, TEXT("Unable to create Mailslot with name %s: %s"), *MailslotName, ErrorMsg);
	}
}

void FWindowsHermesServerModule::ShutdownModule()
{
	if (ServerHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ServerHandle);
		ServerHandle = INVALID_HANDLE_VALUE;
	}
}

void FWindowsHermesServerModule::Tick(float DeltaTime)
{
	FGenericHermesServer::Tick(DeltaTime);

	// Immediate timeout (0ms)
	DWORD ReadTimeout = 0;
	// No maximum message size
	const LPDWORD MaximumMessageSizePtr = nullptr;
	// We don't care about how many are left, we only process one each tick
	const LPDWORD NumMessagesRemainingPtr = nullptr;
	DWORD PendingMessageSize = 0;
	BOOL Success = GetMailslotInfo(
		ServerHandle,
		MaximumMessageSizePtr,
		&PendingMessageSize,
		NumMessagesRemainingPtr,
		&ReadTimeout
	);
	if (!Success || PendingMessageSize == MAILSLOT_NO_MESSAGE)
	{
		return;
	}

	TArray<UTF8CHAR> Data;
	Data.SetNumUninitialized(PendingMessageSize);

	DWORD BytesRead = 0;
	Success = ReadFile(ServerHandle, Data.GetData(), PendingMessageSize, &BytesRead, nullptr);
	if (Success)
	{
		const FString StrData(TStringConversion<FUTF8ToTCHAR_Convert>((FUTF8ToTCHAR_Convert::FromType*)Data.GetData(), Data.Num()).Get());
		HandlePath(StrData);
	}
}

#include <Windows/HideWindowsPlatformTypes.h>
