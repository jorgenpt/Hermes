// Copyright (c) Jørgen Tjernø <jorgen@tjer.no>. All rights reserved.
#include "GenericHermesServer.h"

#include <Interfaces/IPluginManager.h>

#include <Windows/AllowWindowsPlatformTypes.h>

#include "accctrl.h"
#include "aclapi.h"

struct FWindowsHermesServerModule : FGenericHermesServer
{
private: // Implementation of IModuleInterface
	virtual void ShutdownModule() override final;

private: // Implementation of FTickableEditorObject
	virtual void Tick(float DeltaTime) override final;

private: // Implementation of FGenericHermesServer
	virtual bool RegisterScheme(const TCHAR* Scheme, bool bDebug) override final;
	virtual void UnregisterScheme(const TCHAR* Scheme) override final;

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

/**
 * Helper struct to manage the lifecycle of some opaque heap-allocated Windows types
 */
struct FHermesSecurityDescriptorBuilder
{
	~FHermesSecurityDescriptorBuilder()
	{
		if (ACL)
			LocalFree(ACL);
		if (SecurityDescriptor)
			LocalFree(SecurityDescriptor);
	}

	PSECURITY_DESCRIPTOR CreateDescriptor(PSID Sid)
	{
		check(!SecurityDescriptor);
		check(!ACL);

		// Create an ACE (Access Control Entry) to allow the given SID to read & write to this object
		EXPLICIT_ACCESS ExplicitAccess;
		::ZeroMemory(&ExplicitAccess, sizeof(ExplicitAccess));
		ExplicitAccess.grfAccessPermissions = GENERIC_READ | GENERIC_WRITE;
		ExplicitAccess.grfAccessMode = SET_ACCESS;
		ExplicitAccess.grfInheritance = NO_INHERITANCE;
		ExplicitAccess.Trustee.TrusteeForm = TRUSTEE_IS_SID;
		ExplicitAccess.Trustee.TrusteeType = TRUSTEE_IS_USER;
		ExplicitAccess.Trustee.ptstrName = (LPTSTR)Sid;

		// Create a new ACL that contains the new ACEs.
		DWORD dwRes = SetEntriesInAcl(1, &ExplicitAccess, NULL, &ACL);
		if (ERROR_SUCCESS != dwRes)
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not set ACL entries for Mailslot security descriptor: %u"), GetLastError());
			return nullptr;
		}

		// Create & initialize a security descriptor.
		SecurityDescriptor = (PSECURITY_DESCRIPTOR)LocalAlloc(LPTR, SECURITY_DESCRIPTOR_MIN_LENGTH);
		if (NULL == SecurityDescriptor)
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not allocate memory for Mailslot security descriptor: %u"), GetLastError());
			return nullptr;
		}

		if (!InitializeSecurityDescriptor(SecurityDescriptor, SECURITY_DESCRIPTOR_REVISION))
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not initialize Mailslot security descriptor: %u"), GetLastError());
			return nullptr;
		}

		// Add the ACL to the security descriptor.
		if (!SetSecurityDescriptorDacl(SecurityDescriptor,
									   /* bDaclPresent = */ TRUE,
									   /* pDacl = */ ACL,
									   /* bDaclDefaulted = */ FALSE))
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not set Mailslot security descriptor DACL: %u"), GetLastError());
			return nullptr;
		}

		return SecurityDescriptor;
	}

private:
	PACL ACL = NULL;
	PSECURITY_DESCRIPTOR SecurityDescriptor = NULL;
};

/**
 * Helper struct to manage the lifecycle of an opaque buffer used to represent a system SID
 */
struct FHermesSID
{
	void CopyFrom(PSID FromSID)
	{
		DWORD Length = GetLengthSid(FromSID);
		Bytes.SetNumZeroed(Length);
		bSuccess = CopySid(Length, GetSID(), FromSID);
		if (!bSuccess)
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not get duplicate SID for Mailslot ACL: %u"), ::GetLastError());
		}
	}

	bool IsValid() const
	{
		return bSuccess;
	}

	PSID GetSID()
	{
		return reinterpret_cast<PSID>(Bytes.GetData());
	}

private:
	bool bSuccess = false;
	TArray<uint8> Bytes;
};

/**
 * Helper function to retrieve the SID of the current user -- this will get the actual user, even if running elevated (i.e. "Run As Administrator")
 */
FHermesSID HermesPrivate_GetCurrentUserSID()
{
	FHermesSID UserSID;
	HANDLE TokenHandle;
	if (::OpenProcessToken(::GetCurrentProcess(), TOKEN_QUERY, &TokenHandle))
	{
		DWORD UserTokenSize;

		::GetTokenInformation(TokenHandle, TokenUser, NULL, 0, &UserTokenSize);

		if (::GetLastError() == ERROR_INSUFFICIENT_BUFFER)
		{
			TArray<BYTE> UserTokenBytes;
			UserTokenBytes.SetNumZeroed(UserTokenSize);
			PTOKEN_USER UserToken = reinterpret_cast<PTOKEN_USER>(UserTokenBytes.GetData());

			if (::GetTokenInformation(TokenHandle, TokenUser, UserToken, UserTokenSize, &UserTokenSize))
			{
				UserSID.CopyFrom(UserToken->User.Sid);
			}
			else
			{
				UE_LOG(LogHermesServer, Error, TEXT("Could not get current user token for Mailslot ACL: %u"), ::GetLastError());
			}
		}
		else
		{
			UE_LOG(LogHermesServer, Error, TEXT("Could not get buffer size for a token for Mailslot ACL: %u"), ::GetLastError());
		}

		::CloseHandle(TokenHandle);
	}
	else
	{
		UE_LOG(LogHermesServer, Error, TEXT("Could not query the process token for Mailslot ACL: %u"), ::GetLastError());
	}

	return UserSID;
}

bool FWindowsHermesServerModule::RegisterScheme(const TCHAR* Scheme, bool bDebug)
{
	checkf(ServerHandle == INVALID_HANDLE_VALUE,
	       TEXT("Called RegisterScheme(\"%s\"), but mailslot already initialized for %s://"), Scheme, *ServerScheme);

	SECURITY_ATTRIBUTES SecurityAttributes = {};
	SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
	SecurityAttributes.bInheritHandle = FALSE;

	/**
	 * Create a custom security descriptor for our mailslot that gives the current user read & write privileges to it.
	 * This is needed because the default ACL for a mailslot when the process is running elevated (i.e. "Run as Administrator")
	 * does not allow for writing by the current user, which means that any attempts by the non-elevated hermes_urls.exe to
	 * open the Mailslot for writing will fail. Without this, when the editor is running as administrator, URLs will always open
	 * a new editor.
	 */
	FHermesSecurityDescriptorBuilder DescriptorBuilder;
	FHermesSID CurrentUserSID = HermesPrivate_GetCurrentUserSID();
	if (CurrentUserSID.IsValid())
	{
		if (PSECURITY_DESCRIPTOR SecurityDescriptor = DescriptorBuilder.CreateDescriptor(CurrentUserSID.GetSID()))
		{
			SecurityAttributes.lpSecurityDescriptor = SecurityDescriptor;
		}
	}

	const FString MailslotName = FString::Printf(TEXT("\\\\.\\mailslot\\bitSpatter\\Hermes\\%s"), Scheme);
	UE_LOG(LogHermesServer, Verbose, TEXT("Attempting to create Mailslot %s"), *MailslotName);
	ServerHandle = CreateMailslot(*MailslotName, MAX_MESSAGE_SIZE, 0, &SecurityAttributes);
	if (ServerHandle == INVALID_HANDLE_VALUE)
	{
		TCHAR ErrorMsg[1024];
		FPlatformMisc::GetSystemErrorMessage(ErrorMsg, UE_ARRAY_COUNT(ErrorMsg), 0);
		UE_LOG(LogHermesServer, Error, TEXT("Unable to create Mailslot with name %s: %s"), *MailslotName, ErrorMsg);
		return false;
	}

	const FString EditorPath = FPaths::ConvertRelativePathToFull(
		FPlatformProcess::GetModulesDirectory() / FPlatformProcess::ExecutableName(false));
	const TCHAR* RegisterArgument = bDebug ? TEXT("--debug register --register-with-debugging") : TEXT("register");
	const FString OpenCommand = FString::Printf(
		TEXT("\"%s\" \"%s\" -HermesPath=\"%%1\""), *EditorPath, *FPaths::GetProjectFilePath());

	const FString HermesHandlerExe = GetHermesHandlerExe();
	const FString Arguments = FString::Printf(TEXT("%s -- %s %s"), RegisterArgument, Scheme, *OpenCommand);

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

#include <Windows/HideWindowsPlatformTypes.h>
