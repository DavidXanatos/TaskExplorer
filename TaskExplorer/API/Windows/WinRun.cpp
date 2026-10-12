/*
 * Task Explorer -
 *   starting a process with options, moved out of CRunDialog.
 *
 * Which token to use, whether to start suspended and whether to inject a DLL are
 * all decisions about the machine the process starts on, so they belong here -
 * the dialog now only collects them.
 */

#include "stdafx.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"
#include "ProcessHacker/RunAs.h"
#include "WinHelpers/InjectDll/injdll.h"

#include <wtsapi32.h>
#include <userenv.h>
#include <tlhelp32.h>
#pragma comment(lib, "wtsapi32.lib")
#pragma comment(lib, "userenv.lib")

QStringList CWindowsAPI::GetRunHistory() const
{
	return GetRunMruList();
}

CSystemAPI::SRunAsChoices CWindowsAPI::GetRunAsChoices() const
{
	SRunAsChoices Choices;

	//
	// Platform terms, not prose - left untranslated as they always have been.
	//
	Choices.LogonTypes.append(qMakePair(QString("Batch"), (quint32)LOGON32_LOGON_BATCH));
	Choices.LogonTypes.append(qMakePair(QString("Interactive"), (quint32)LOGON32_LOGON_INTERACTIVE));
	Choices.LogonTypes.append(qMakePair(QString("Network"), (quint32)LOGON32_LOGON_NETWORK));
	Choices.LogonTypes.append(qMakePair(QString("New credentials"), (quint32)LOGON32_LOGON_NEW_CREDENTIALS));
	Choices.LogonTypes.append(qMakePair(QString("Service"), (quint32)LOGON32_LOGON_SERVICE));

	Choices.Accounts = GetLogonAccounts();
	Choices.Sessions = GetLogonSessions();
	Choices.Desktops = GetDesktops();
	Choices.CurrentSessionId = GetCurrentSessionId();
	Choices.CurrentDesktop = GetCurrentDesktop();

	return Choices;
}

bool CWindowsAPI::IsServiceAccount(const QString& UserName) const
{
	PPH_STRING username = CastQString(UserName);
	const bool bService = !!::IsServiceAccount(username);
	if (username)
		PhDereferenceObject(username);
	return bService;
}


//
// ---- starting something in another session ----
//
// See the note in WindowsAPI.h. Used by the two callers that need it: the run
// dialog when this process is a service, and the window agents, which are
// nothing but a program started in somebody else's session.
//

static HANDLE UnfilterSessionToken(HANDLE Token)
{
	//
	// A logged-on administrator holds two tokens: the one everything runs with,
	// which has the group deny-only, and the linked one it elevates to. Both
	// routes below give the first.
	//
	// A token that is not a limited one has no linked token worth having -
	// asking gives the filtered one back, which is the wrong direction.
	//
	TOKEN_ELEVATION_TYPE Type = TokenElevationTypeDefault;
	DWORD Size = 0;
	if (!GetTokenInformation(Token, TokenElevationType, &Type, sizeof(Type), &Size))
		return Token;

	if (Type != TokenElevationTypeLimited)
		return Token;

	TOKEN_LINKED_TOKEN Linked = { 0 };
	if (!GetTokenInformation(Token, TokenLinkedToken, &Linked, sizeof(Linked), &Size))
		return Token;

	HANDLE Primary = NULL;
	if (!DuplicateTokenEx(Linked.LinkedToken, TOKEN_ALL_ACCESS, NULL,
						  SecurityImpersonation, TokenPrimary, &Primary))
	{
		CloseHandle(Linked.LinkedToken);
		return Token;
	}

	CloseHandle(Linked.LinkedToken);
	CloseHandle(Token);
	return Primary;
}

static HANDLE OpenSessionToken(quint32 SessionId, bool bLinkedToken)
{
	HANDLE Token = NULL;
	if (WTSQueryUserToken((ULONG)SessionId, &Token))
		return bLinkedToken ? UnfilterSessionToken(Token) : Token;

	//
	// The borrowed route. Any process in the session will do, so the first one
	// that opens is taken, and it is opened for query only - a token is all
	// that is wanted from it.
	//
	// Enumerated through the snapshot API rather than through theSystem's
	// process list: this can be called from inside a refresh, before that
	// round has filled the list in, and a fallback that depended on it would
	// fail for a reason having nothing to do with sessions or privileges.
	//
	HANDLE Snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (Snapshot == INVALID_HANDLE_VALUE)
		return NULL;

	PROCESSENTRY32W Entry = { sizeof(Entry) };
	for (BOOL Ok = Process32FirstW(Snapshot, &Entry); Ok; Ok = Process32NextW(Snapshot, &Entry))
	{
		DWORD Session = 0;
		if (!ProcessIdToSessionId(Entry.th32ProcessID, &Session) || Session != (DWORD)SessionId)
			continue;

		HANDLE Process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Entry.th32ProcessID);
		if (!Process)
			continue;

		HANDLE Source = NULL;
		if (OpenProcessToken(Process, TOKEN_DUPLICATE | TOKEN_QUERY, &Source))
		{
			HANDLE Primary = NULL;
			if (DuplicateTokenEx(Source, TOKEN_ALL_ACCESS, NULL, SecurityImpersonation, TokenPrimary, &Primary))
			{
				CloseHandle(Source);
				CloseHandle(Process);
				CloseHandle(Snapshot);
				return bLinkedToken ? UnfilterSessionToken(Primary) : Primary;
			}
			CloseHandle(Source);
		}
		CloseHandle(Process);
	}

	CloseHandle(Snapshot);
	return NULL;
}

quint32 GetInteractiveSessionId()
{
	const DWORD Session = WTSGetActiveConsoleSessionId();
	return Session == 0xFFFFFFFF ? (quint32)-1 : (quint32)Session;
}

bool IsInInteractiveSession()
{
	DWORD Session = 0;
	if (!ProcessIdToSessionId(GetCurrentProcessId(), &Session))
		return true;		// unknown: behave as before rather than reroute blindly
	return Session != 0;
}

STATUS StartProcessInSession(quint32 SessionId, const QString& CommandLine,
							 bool bLinkedToken, quint64* pProcessId)
{
	HANDLE Token = OpenSessionToken(SessionId, bLinkedToken);
	if (!Token)
		return ERR(TE_Generic, QVariantList()
			<< CastPhString(PhGetStatusMessage(0, GetLastError())), (long)GetLastError());

	//
	// The session is set explicitly as well as coming from the token, which
	// already carries it. Saying it costs nothing and makes the intent readable
	// where the failure would otherwise be baffling.
	//
	DWORD Session = (DWORD)SessionId;
	SetTokenInformation(Token, TokenSessionId, &Session, sizeof(Session));

	//
	// The interactive desktop by name. Without it the process lands on whatever
	// the token's default says, which for a service token is the service's own -
	// the very desktop this whole exercise is about not being stuck on.
	//
	STARTUPINFOW StartupInfo = { sizeof(StartupInfo) };
	StartupInfo.lpDesktop = (LPWSTR)L"WinSta0\\Default";

	PROCESS_INFORMATION ProcessInfo = { 0 };

	LPVOID Environment = NULL;
	CreateEnvironmentBlock(&Environment, Token, FALSE);

	std::wstring Command = CommandLine.toStdWString();

	//
	// CreateProcessAsUser first, which wants SeAssignPrimaryTokenPrivilege -
	// LocalSystem has it. An elevated administrator does not, and falls back to
	// CreateProcessWithTokenW, which wants SeImpersonatePrivilege instead and
	// that one an administrator does have. Between the two, both cases work
	// without either being given anything it did not already hold.
	//
	BOOL Started = CreateProcessAsUserW(Token, NULL, (LPWSTR)Command.c_str(), NULL, NULL, FALSE,
		CREATE_UNICODE_ENVIRONMENT, Environment, NULL, &StartupInfo, &ProcessInfo);

	if (!Started)
	{
		Started = CreateProcessWithTokenW(Token, 0, NULL, (LPWSTR)Command.c_str(),
			CREATE_UNICODE_ENVIRONMENT, Environment, NULL, &StartupInfo, &ProcessInfo);
	}

	const DWORD Error = GetLastError();

	if (Environment)
		DestroyEnvironmentBlock(Environment);
	CloseHandle(Token);

	if (!Started)
		return ERR(TE_Generic, QVariantList()
			<< CastPhString(PhGetStatusMessage(0, Error)), (long)Error);

	if (pProcessId)
		*pProcessId = (quint64)ProcessInfo.dwProcessId;

	CloseHandle(ProcessInfo.hThread);
	CloseHandle(ProcessInfo.hProcess);
	return OK;
}

STATUS CWindowsAPI::RunProgram(const SRunOptions& Options)
{
	//
	// A service has no session anybody is sitting at.
	//
	// Started the ordinary way, the program would run in session 0 - correctly,
	// successfully and completely invisibly, which is the worst of the three.
	// So when this process is not somewhere a program would be seen, the
	// program is started in the session that is.
	//
	// Not through the window agents, which would be a needless hop and would
	// make running a program depend on a setting that is off by default: an
	// agent is itself nothing but a program started this way.
	//
	// The DLL injection and suspended-start paths below need a handle to the
	// new process, which this route does not hand back, so those keep the
	// ordinary path and fail honestly where they cannot work.
	//
	DWORD OwnSession = 0;
	ProcessIdToSessionId(GetCurrentProcessId(), &OwnSession);

	//
	// Zero means "wherever this makes sense": our own session when we are in
	// one, and the session somebody is sitting at when we are a service.
	//
	const quint32 Target = Options.SessionId ? Options.SessionId
		: (IsInInteractiveSession() ? (quint32)OwnSession : GetInteractiveSessionId());

	//
	// Rerouted only when the target is somewhere else. A viewer starting a
	// program in its own session wants the ordinary path - it already has the
	// right token, and going the long way round would lose the DLL injection
	// and suspended-start options for nothing.
	//
	if (Target != (quint32)-1 && Target != (quint32)OwnSession
	 && Options.InjectDll.isEmpty() && !Options.Suspended)
	{
		const STATUS Status = StartProcessInSession(Target, Options.Program, Options.Elevated);
		if (Status.IsError())
			return Status;

		const std::wstring Started = Options.Program.toStdWString();
		PhpAddRunMRUListEntry(PH_STRINGREF { Started.length() * sizeof(wchar_t), (wchar_t*)Started.c_str() });
		return OK;
	}

	std::wstring filePath = Options.Program.toStdWString();
	std::wstring dllPath = Options.InjectDll.toStdWString();
	const bool bInject = !Options.InjectDll.isEmpty();

	NTSTATUS status;

	HANDLE tokenHandle = NULL;
	HANDLE newTokenHandle = NULL;
	HANDLE threadHandle = NULL;
	HANDLE processHandle = NULL;
#ifdef _WIN64
	BOOLEAN isWow64 = FALSE;
#endif

	if (!Options.Elevated && const_cast<CWindowsAPI*>(this)->RootAvaiable() && !bInject && !Options.Suspended)
	{
		if (NT_SUCCESS(status = RunAsLimitedUser((wchar_t*)filePath.c_str())))
			goto done;
	}

	if (!NT_SUCCESS(status = PhOpenProcessToken(
		NtCurrentProcess(),
		TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_GROUPS | TOKEN_ADJUST_DEFAULT | READ_CONTROL | WRITE_DAC,
		&tokenHandle
	)))
		goto cleanup;

	if (!Options.Elevated && const_cast<CWindowsAPI*>(this)->RootAvaiable())
	{
		// Note: the process will still look admin but wont have the privilegs
		if (!NT_SUCCESS(status = PhFilterTokenForLimitedUser(tokenHandle, &newTokenHandle)))
			goto cleanup;
	}

	/*
	 * PH_CREATE_PROCESS_INHERIT_HANDLES Inheritable handles will be duplicated to the process from the parent process.
	 * PH_CREATE_PROCESS_SUSPENDED The initial thread will be created suspended.
	 * PH_CREATE_PROCESS_BREAKAWAY_FROM_JOB The process will not be assigned to the job object associated with the parent process.
	 * PH_CREATE_PROCESS_NEW_CONSOLE The process will have its own console, instead of inheriting the console of the parent process.
	 */
    if (!NT_SUCCESS(status = PhCreateProcessWin32(
        NULL,
        (wchar_t*)filePath.c_str(),
        NULL,
        NULL,
        (bInject || Options.Suspended) ? PH_CREATE_PROCESS_SUSPENDED : 0,
        newTokenHandle ? newTokenHandle : tokenHandle,
        &processHandle,
        &threadHandle
        )))
			goto cleanup;


	if (bInject)
	{
		BYTE* pebAddress;

#ifdef _WIN64
		PVOID peb32;
		if (!NT_SUCCESS(status = PhGetProcessPeb32(processHandle, &peb32)))
			goto cleanup;
		isWow64 = !!peb32; // PhGetProcessIsWow64

		if (isWow64)
			pebAddress = (BYTE*)peb32;
		else
#endif
		{
			PROCESS_BASIC_INFORMATION basicInfo;
			if (!NT_SUCCESS(status = PhGetProcessBasicInformation(processHandle, &basicInfo)))
				goto cleanup;

			pebAddress = (BYTE*)basicInfo.PebBaseAddress;
		}

		unsigned long long LoaderThreadsOffset = 0;
#ifdef _WIN64
		if (!isWow64)
		{
			const int ProcessParameters_64 = 32; // FIELD_OFFSET(PEB, ProcessParameters); // 64 bit
			const int LoaderThreads_64 = 1036; // FIELD_OFFSET(RTL_USER_PROCESS_PARAMETERS, LoaderThreads); // 64 bit

			unsigned long long ProcessParameters;
			if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, pebAddress + ProcessParameters_64, &ProcessParameters, sizeof(ProcessParameters), NULL)))
				goto cleanup;

			LoaderThreadsOffset = ProcessParameters + LoaderThreads_64;
		}
		else
#endif
		{
			const int ProcessParameters_32 = 16; // FIELD_OFFSET(PEB, ProcessParameters); // 32 bit
			const int LoaderThreads_32 = 672; // FIELD_OFFSET(RTL_USER_PROCESS_PARAMETERS, LoaderThreads); // 32 bit

			unsigned long ProcessParameters;
			if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, pebAddress + ProcessParameters_32, &ProcessParameters, sizeof(ProcessParameters), NULL)))
				goto cleanup;

			LoaderThreadsOffset = ProcessParameters + LoaderThreads_32;
		}

		ULONG LoaderThreads;
		if (!NT_SUCCESS(status = NtReadVirtualMemory(processHandle, (PVOID)LoaderThreadsOffset, &LoaderThreads, sizeof(LoaderThreads), NULL)))
			goto cleanup;
		LoaderThreads = 1; 
		if (!NT_SUCCESS(status = NtWriteVirtualMemory(processHandle, (PVOID)LoaderThreadsOffset, &LoaderThreads, sizeof(LoaderThreads), NULL)))
			goto cleanup;

#ifdef _WIN64
		if (!isWow64)
			status = inject_x64(processHandle, threadHandle, dllPath.c_str()) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
		else
#endif
			status = inject_x86(processHandle, threadHandle, dllPath.c_str()) ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;

		if (!NT_SUCCESS(status))
			status = STATUS_DLL_NOT_FOUND;
		else if (!Options.Suspended)
			status = NtResumeThread(threadHandle, NULL);
	}

cleanup:
	if (!NT_SUCCESS(status) && processHandle && processHandle != INVALID_HANDLE_VALUE)
		NtTerminateProcess(processHandle, -1);

	if(newTokenHandle)
		NtClose(newTokenHandle);
	if(tokenHandle)
		NtClose(tokenHandle);
	if(processHandle)
		NtClose(processHandle);
	if(threadHandle)
		NtClose(threadHandle);

done:
	if (!NT_SUCCESS(status))
		return ERR(TE_Generic, QVariantList() << CastPhString(PhGetStatusMessage(status, 0)), status);

	PhpAddRunMRUListEntry(PH_STRINGREF { filePath.length() * sizeof(wchar_t), (wchar_t*)filePath.c_str() });
	return OK;
}

STATUS CWindowsAPI::RunProgramAs(const SRunAsOptions& Options)
{
	NTSTATUS status;
	BOOLEAN useLinkedToken = FALSE;
	BOOLEAN createSuspended = FALSE;
	ULONG logonType = ULONG_MAX;
	ULONG sessionId = ULONG_MAX;
	PPH_STRING program = NULL;
	PPH_STRING username = NULL;
	PPH_STRING password = NULL;
	PPH_STRING desktopName = NULL;
	HANDLE ProcessId = (HANDLE)Options.ParentPid;
	ULONG currentSessionId = ULONG_MAX;

	STATUS Result = OK;

	program = CastQString(Options.Program);
	username = CastQString(Options.UserName);
	useLinkedToken = Options.UseLinkedToken;
	createSuspended = Options.Suspended;

	if (PhIsNullOrEmptyString(program))
	{
		Result = ERR(TE_NoProgramGiven);
		goto CleanupExit;
	}

	logonType = Options.LogonType;
	sessionId = Options.SessionId;
	desktopName = CastQString(Options.Desktop);

	// Fix up the user name if it doesn't have a domain.
	if (PhFindCharInString(username, 0, '\\') == -1)
	{
		PSID sid;
		PPH_STRING newUserName;

		if (NT_SUCCESS(PhLookupName(&username->sr, &sid, NULL, NULL)))
		{
			if (newUserName = PhGetSidFullName(sid, TRUE, NULL))
				PhSwapReference(&username, newUserName);

			PhFree(sid);
		}
	}

	if (!Options.Password.isEmpty())
		password = CastQString(Options.Password);

	//if (IsCurrentUserAccount(username))
	//{
	//    status = PhCreateProcessWin32(
	//        NULL,
	//        program->Buffer,
	//        NULL,
	//        NULL,
	//        0,
	//        NULL,
	//        NULL,
	//        NULL
	//        );
	//}

	PhGetProcessSessionId(NtCurrentProcess(), &currentSessionId);

	if (g_KsiClientProtectionLevel == 0 && logonType == LOGON32_LOGON_INTERACTIVE && !ProcessId && sessionId == currentSessionId && !useLinkedToken)
	{
		// We are eligible to load the user profile.
		// This must be done here, not in the service, because
		// we need to be in the target session.
		//
		// Skipped when this process is protected (g_KsiClientProtectionLevel != 0):
		// the profile-loading path below runs CreateProcessWithLogonW, which is
		// serviced by seclogon, and seclogon opens the *calling* process. The
		// driver strips that access from a protected client, so the call fails
		// with access denied. Falling through to the else branch hands the job to
		// the SYSTEM helper service, which does LogonUser + CreateProcessAsUser
		// itself and never touches seclogon (it just does not mount the hive).

		PH_CREATE_PROCESS_AS_USER_INFO createInfo;
		PPH_STRING domainPart = NULL;
		PPH_STRING userPart = NULL;
		HANDLE newProcessHandle;

		PhpSplitUserName(username->Buffer, &domainPart, &userPart);

		memset(&createInfo, 0, sizeof(PH_CREATE_PROCESS_AS_USER_INFO));
		createInfo.CommandLine = PhGetString(program);
		createInfo.UserName = PhGetString(userPart);
		createInfo.DomainName = PhGetString(domainPart);
		createInfo.Password = PhGetStringOrEmpty(password);

		// Whenever we can, try not to set the desktop name; it breaks a lot of things.
		if (!PhIsNullOrEmptyString(desktopName) && !PhEqualString2(desktopName, L"WinSta0\\Default", TRUE))
			createInfo.DesktopName = PhGetString(desktopName);

		//PhSetDesktopWinStaAccess();

		status = PhCreateProcessAsUser(
			&createInfo,
			PH_CREATE_PROCESS_WITH_PROFILE | (createSuspended ? PH_CREATE_PROCESS_SUSPENDED : 0),
			NULL,
			NULL,
			&newProcessHandle,
			NULL);

		if (NT_SUCCESS(status))
		{
			PROCESS_BASIC_INFORMATION basicInfo;
			PSID userSid, logonSid;

			if (PhRunAsGetLogonSid(newProcessHandle, &userSid, &logonSid))
			{
				status = PhRunAsUpdateDesktop(userSid);

				if (!NT_SUCCESS(status))
					goto CleanupExit;

				status = PhRunAsUpdateWindowStation(userSid, logonSid);

				if (!NT_SUCCESS(status))
					goto CleanupExit;
			}

			if (!createSuspended)
			{
				if (NT_SUCCESS(PhGetProcessBasicInformation(newProcessHandle, &basicInfo)))
				{
					AllowSetForegroundWindow(HandleToUlong(basicInfo.UniqueProcessId));
				}

				PhConsoleSetForeground(newProcessHandle, TRUE);

				NtResumeProcess(newProcessHandle);
			}

			NtClose(newProcessHandle);
		}

		if (domainPart) PhDereferenceObject(domainPart);
		if (userPart) PhDereferenceObject(userPart);
	}
	else
	{
		if (ProcessId)
		{
			status = PhRunAsExecutionAlias(program);

			if (!NT_SUCCESS(status))
			{
				status = PhRunAsExecuteParentCommand(
					PhMainWndHandle,
					PhGetString(program),
					ProcessId,
					createSuspended
				);
			}
		}
		else
		{
			status = PhExecuteRunAsCommand3(
				PhMainWndHandle,
				PhGetString(program),
				PhGetString(username),
				PhGetStringOrEmpty(password),
				logonType,
				ProcessId,
				sessionId,
				PhGetString(desktopName),
				useLinkedToken,
				createSuspended
			);
		}
	}

	if (!NT_SUCCESS(status))
	{
		// a cancelled credential prompt is not an error worth reporting
		if (status != STATUS_CANCELLED)
			Result = ERR(TE_Generic, QVariantList() << CastPhString(PhGetStatusMessage(status, 0)), status);
	}
	else if (status != STATUS_TIMEOUT)
		PhpAddRunMRUListEntry(program->sr);

CleanupExit:
	if (program)
		PhDereferenceObject(program);
	if (username)
		PhDereferenceObject(username);
	if (password)
	{
		RtlSecureZeroMemory(password->Buffer, password->Length);
		PhDereferenceObject(password);
	}
	if (desktopName)
		PhDereferenceObject(desktopName);

	return Result;
}

