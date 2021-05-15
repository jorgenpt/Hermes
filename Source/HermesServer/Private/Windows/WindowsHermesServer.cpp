// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include <Interfaces/IPluginManager.h>

#include <Windows/AllowWindowsPlatformTypes.h>

struct FWindowsHermesServerModule : FGenericHermesServer
{
	virtual void StartupModule() override final;
	virtual void ShutdownModule() override final;
	virtual void Tick(float DeltaTime) override final;

	HANDLE ServerHandle;
	FProcHandle RegistrationHandle;
};

IMPLEMENT_MODULE(FWindowsHermesServerModule, HermesServer)

// Max message size is around the maximum path size (32k), plus 256 bytes for protocol, host, and query string.
static constexpr int32 MAX_MESSAGE_SIZE = 32 * 1024 + 256;

void FWindowsHermesServerModule::StartupModule()
{
	const FString MailslotName = FString::Printf(TEXT("\\\\.\\mailslot\\bitSpatter\\Hermes\\%s\\%s"), GetProtocol(), GetHostname());
	UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to create Mailslot %s"), *MailslotName);
	ServerHandle = CreateMailslot(*MailslotName, MAX_MESSAGE_SIZE, 0, nullptr);
	if (ServerHandle != INVALID_HANDLE_VALUE)
	{
		const TSharedPtr<IPlugin> HermesCorePlugin = IPluginManager::Get().FindPlugin("HermesCore");
		checkf(HermesCorePlugin != nullptr, TEXT("Unable to look up ourselves!"));
		const FString EditorPath = FPaths::ConvertRelativePathToFull(FPlatformProcess::GetModulesDirectory() / FPlatformProcess::ExecutableName(false));
		const FString OpenCommand = FString::Printf(TEXT("\"%s\" \"%s\" -HermesPath=\"%%1\""), *EditorPath, *FPaths::GetProjectFilePath());
		const FString RegistrationArguments = FString::Printf(TEXT("register-hostname -- %s %s %s"), GetProtocol(), GetHostname(), *OpenCommand);
		const FString HermesURLsExe = HermesCorePlugin->GetBaseDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory() / TEXT("hermes_urls.exe");

		UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to register %s://%s using %s %s"), GetProtocol(), GetHostname(), *HermesURLsExe, *RegistrationArguments);
		RegistrationHandle = FPlatformProcess::CreateProc(*HermesURLsExe, *RegistrationArguments, true, false, false, nullptr, 0, nullptr, nullptr, nullptr);
		if (!RegistrationHandle.IsValid())
		{
			UE_LOG(LogHermesServer, Error, TEXT("Unable to register %s://%s using %s %s"), GetProtocol(), GetHostname(), *HermesURLsExe, *RegistrationArguments);
		}
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

	if (RegistrationHandle.IsValid())
	{
		if (!FPlatformProcess::IsProcRunning(RegistrationHandle))
		{
			int32 ReturnCode = INDEX_NONE;
			if (FPlatformProcess::GetProcReturnCode(RegistrationHandle, &ReturnCode))
			{
				if (ReturnCode != 0)
				{
					UE_LOG(LogHermesServer, Error, TEXT("URL Registration failed with status code %i"), ReturnCode);
				}
				else
				{
					UE_LOG(LogHermesServer, Verbose, TEXT("URL Registration completed successfully"));
				}
			}
			else
			{
				UE_LOG(LogHermesServer, Error, TEXT("Unable to poll return code for completed registration"));
			}

			FPlatformProcess::CloseProc(RegistrationHandle);
			RegistrationHandle.Reset();
		}
	}

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
		TStringConversion<FUTF8ToTCHAR_Convert> Conversion((FUTF8ToTCHAR_Convert::FromType*)Data.GetData(), BytesRead);
		const FString StrData(Conversion.Length(), Conversion.Get());
		HandlePath(StrData);
	}
	else
	{
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), 0);
		UE_LOG(LogHermesServer, Error, TEXT("Unable to read message of %i bytes from mailslot: %s"), PendingMessageSize, ErrorMsg);
	}
}

#include <Windows/HideWindowsPlatformTypes.h>
