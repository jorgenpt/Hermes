// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include <Interfaces/IPluginManager.h>

#include <Windows/AllowWindowsPlatformTypes.h>

struct FWindowsHermesServerModule : FGenericHermesServer
{
private: // Implementation of IModuleInterface
	virtual void ShutdownModule() override final;

private: // Implementation of FTickableEditorObject
	virtual void Tick(float DeltaTime) override final;

private: // Implementation of FGenericHermesServer
	virtual bool RegisterScheme(const TCHAR* Scheme) override final;
	virtual void UnregisterScheme(const TCHAR* Scheme) override final;
	virtual void ClipboardCopyURL(const FString& URL) override final;

	FString ServerScheme;
	HANDLE ServerHandle = INVALID_HANDLE_VALUE;
	FProcHandle RegistrationHandle;
};

IMPLEMENT_MODULE(FWindowsHermesServerModule, HermesServer)

// Max message size is around the maximum path size (32k), plus 256 bytes for scheme, host, and query string.
static constexpr int32 MAX_MESSAGE_SIZE = 32 * 1024 + 256;

static FString GetHermesHandlerExe()
{
	const TSharedPtr<IPlugin> HermesCorePlugin = IPluginManager::Get().FindPlugin("HermesCore");
	checkf(HermesCorePlugin != nullptr, TEXT("Unable to look up ourselves!"));
	return HermesCorePlugin->GetBaseDir() / TEXT("Binaries") / FPlatformProcess::GetBinariesSubdirectory() / TEXT(
		"hermes_urls.exe");
}

bool FWindowsHermesServerModule::RegisterScheme(const TCHAR* Scheme)
{
	checkf(ServerHandle == INVALID_HANDLE_VALUE,
	       TEXT("Called RegisterScheme(\"%s\"), but mailslot already initialized for %s://"), Scheme, *ServerScheme);

	const FString MailslotName = FString::Printf(TEXT("\\\\.\\mailslot\\bitSpatter\\Hermes\\%s"), Scheme);
	UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to create Mailslot %s"), *MailslotName);
	ServerHandle = CreateMailslot(*MailslotName, MAX_MESSAGE_SIZE, 0, nullptr);
	if (ServerHandle == INVALID_HANDLE_VALUE)
	{
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), 0);
		UE_LOG(LogHermesServer, Error, TEXT("Unable to create Mailslot with name %s: %s"), *MailslotName, ErrorMsg);
		return false;
	}

	const FString EditorPath = FPaths::ConvertRelativePathToFull(
		FPlatformProcess::GetModulesDirectory() / FPlatformProcess::ExecutableName(false));
	const FString OpenCommand = FString::Printf(
		TEXT("\"%s\" \"%s\" -HermesPath=\"%%1\""), *EditorPath, *FPaths::GetProjectFilePath());

	const FString HermesHandlerExe = GetHermesHandlerExe();
	const FString Arguments = FString::Printf(TEXT("register -- %s %s"), Scheme, *OpenCommand);

	UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to register %s:// using %s %s"), Scheme, *HermesHandlerExe,
	       *Arguments);
	RegistrationHandle = FPlatformProcess::CreateProc(*HermesHandlerExe, *Arguments, true, false, false,
	                                                  nullptr, 0, nullptr, nullptr, nullptr);
	if (!RegistrationHandle.IsValid())
	{
		CloseHandle(ServerHandle);
		ServerHandle = INVALID_HANDLE_VALUE;
		UE_LOG(LogHermesServer, Error, TEXT("Unable to register %s:// using %s %s"), Scheme, *HermesHandlerExe,
		       *Arguments);
		return false;
	}

	ServerScheme = Scheme;
	return true;
}

void FWindowsHermesServerModule::UnregisterScheme(const TCHAR* Scheme)
{
	if (ServerScheme == Scheme && ServerHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ServerHandle);
		ServerScheme = TEXT("");
		ServerHandle = INVALID_HANDLE_VALUE;
	}

	if (RegistrationHandle.IsValid())
	{
		FPlatformProcess::WaitForProc(RegistrationHandle);
	}

	const FString HermesHandlerExe = GetHermesHandlerExe();
	const FString Arguments = FString::Printf(TEXT("unregister -- %s"), Scheme);

	UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to unregister %s:// using %s %s"), Scheme, *HermesHandlerExe,
	       *Arguments);

	int32 ReturnCode = 0;
	FPlatformProcess::ExecProcess(*HermesHandlerExe, *Arguments, &ReturnCode, nullptr, nullptr, nullptr);
	if (ReturnCode != 0)
	{
		UE_LOG(LogHermesServer, Warning, TEXT("Unregistration of %s:// failed with status code %i"), Scheme,
		       ReturnCode);
	}
}

void FWindowsHermesServerModule::ShutdownModule()
{
	if (ServerHandle != INVALID_HANDLE_VALUE)
	{
		CloseHandle(ServerHandle);
		ServerScheme = TEXT("");
		ServerHandle = INVALID_HANDLE_VALUE;
	}

	FGenericHermesServer::ShutdownModule();
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
		UE_LOG(LogHermesServer, Error, TEXT("Unable to read message of %i bytes from mailslot: %s"), PendingMessageSize,
		       ErrorMsg);
	}
}

static const FString SClipboardPrefixTemplate = TEXT("")
TEXT("Version:0.9\r\n") \
TEXT("StartHTML:00000000\r\n") \
TEXT("EndHTML:00000000\r\n") \
TEXT("StartFragment:00000000\r\n") \
TEXT("EndFragment:00000000\r\n") \
TEXT("<!DOCTYPE>\r\n")
TEXT("<HTML><BODY>\r\n")
TEXT("<!--StartFragment -->\r\n");

static const FString SClipboardSuffix = TEXT("")
TEXT("<!--EndFragment-->\r\n")
TEXT("</BODY></HTML>\r\n");

void FWindowsHermesServerModule::ClipboardCopyURL(const FString& URL)
{
	static int CFID = 0;
	if (!CFID)
	{
		CFID = RegisterClipboardFormatA("HTML Format");
	}

	if (OpenClipboard(GetActiveWindow()))
	{
		FString ClipboardTemplate = SClipboardPrefixTemplate + FString::Printf(TEXT("<A HREF=\"%s\">%s</A>\r\n"), *URL, *URL) + SClipboardSuffix;
		const int32 DocTypeIndex = ClipboardTemplate.Find(TEXT("<!DOCTYPE>\n"), ESearchCase::CaseSensitive);
		const int32 FragmentStartIndex = ClipboardTemplate.Find(TEXT("<!--StartFragment -->\n"), ESearchCase::CaseSensitive);
		const int32 FragmentEndIndex = ClipboardTemplate.Find(TEXT("<!--EndFragment -->\n"), ESearchCase::CaseSensitive);

		ClipboardTemplate.ReplaceInline(TEXT("StartHTML:00000000"), *FString::Printf(TEXT("StartHTML:%08u"), DocTypeIndex), ESearchCase::CaseSensitive);
		ClipboardTemplate.ReplaceInline(TEXT("EndHTML:00000000"), *FString::Printf(TEXT("EndHTML:%08u"), ClipboardTemplate.Len()), ESearchCase::CaseSensitive);
		ClipboardTemplate.ReplaceInline(TEXT("StartFragment:00000000"), *FString::Printf(TEXT("StartFragment:%08u"), FragmentStartIndex), ESearchCase::CaseSensitive);
		ClipboardTemplate.ReplaceInline(TEXT("EndFragment:00000000"), *FString::Printf(TEXT("EndFragment:%08u"), FragmentEndIndex), ESearchCase::CaseSensitive);

		verify(EmptyClipboard());
		{
			HGLOBAL GlobalMem = GlobalAlloc(GMEM_MOVEABLE, sizeof(TCHAR) * (URL.Len() + 1));
			check(GlobalMem);
			TCHAR* Data = static_cast<TCHAR*>(GlobalLock(GlobalMem));
			FCString::Strcpy(Data, (URL.Len() + 1), *URL);
			GlobalUnlock(GlobalMem);
			if (SetClipboardData(CF_UNICODETEXT, GlobalMem) == nullptr)
			{
				UE_LOG(LogHermesServer, Fatal, TEXT("SetClipboardData failed with error code %u"), static_cast<uint32>(GetLastError()));
			}
		}
		{
			HGLOBAL GlobalMem = GlobalAlloc(GMEM_MOVEABLE, ClipboardTemplate.Len() + 1);
			check(GlobalMem);
			char* Data = static_cast<char*>(GlobalLock(GlobalMem));
			FCStringAnsi::Strcpy(Data, ClipboardTemplate.Len() + 1, TCHAR_TO_UTF8(*ClipboardTemplate));
			GlobalUnlock(GlobalMem);
			if (SetClipboardData(CFID, GlobalMem) == nullptr)
			{
				UE_LOG(LogHermesServer, Fatal, TEXT("SetClipboardData failed with error code %u"), static_cast<uint32>(GetLastError()));
			}
		}
		verify(CloseClipboard());
	}
}


#include <Windows/HideWindowsPlatformTypes.h>
