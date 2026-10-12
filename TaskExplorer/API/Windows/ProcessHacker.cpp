/*
 * Task Explorer -
 *   qt wrapper and helper functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2023 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 *
 */

#include "stdafx.h"
#include "../../../MiscHelpers/Common/Settings.h"
#include "../../../MiscHelpers/Common/Common.h"
#include "../../../MiscHelpers/Archive/Archive.h"
#include "ProcessHacker.h"
#include "WinHelper.h"
#include <kphmsgdyn.h>
#include <sistatus.h>
extern "C" {
#include <kphdyndata.h>
}
#include <settings.h>
#include <psapi.h>
#include "../../SVC/TaskService.h"

QString CastPhString(PPH_STRING phString, bool bDeRef)
{
	QString qString;
	if (phString)
	{
		qString = QString::fromWCharArray(phString->Buffer, phString->Length / sizeof(wchar_t));
		if (bDeRef)
			PhDereferenceObject(phString);
	}
	return qString;
}

PPH_STRING CastQString(const QString& qString)
{
	std::wstring wstr = qString.toStdWString();
	UNICODE_STRING ustr;
	RtlInitUnicodeString(&ustr, (wchar_t*)wstr.c_str());
	return PhCreateStringFromUnicodeString(&ustr);
}

/*
BOOLEAN PhInitializeNamespacePolicy(
	VOID
)
{
	HANDLE mutantHandle;
	WCHAR objectName[PH_INT64_STR_LEN_1];
	OBJECT_ATTRIBUTES objectAttributes;
	UNICODE_STRING objectNameUs;
	PH_FORMAT format[2];

	PhInitFormatS(&format[0], L"PhMutant_");
	PhInitFormatU(&format[1], HandleToUlong(NtCurrentProcessId()));

	if (!PhFormatToBuffer(
		format,
		RTL_NUMBER_OF(format),
		objectName,
		sizeof(objectName),
		NULL
	))
	{
		return FALSE;
	}

	RtlInitUnicodeString(&objectNameUs, objectName);
	InitializeObjectAttributes(
		&objectAttributes,
		&objectNameUs,
		OBJ_CASE_INSENSITIVE,
		PhGetNamespaceHandle(),
		NULL
	);

	if (NT_SUCCESS(NtCreateMutant(
		&mutantHandle,
		MUTANT_QUERY_STATE,
		&objectAttributes,
		TRUE
	)))
	{
		return TRUE;
	}

	return FALSE;
}
*/

VOID PhpEnablePrivileges(
	VOID
)
{
	HANDLE tokenHandle;

	if (NT_SUCCESS(PhOpenProcessToken(
		NtCurrentProcess(),
		TOKEN_ADJUST_PRIVILEGES,
		&tokenHandle
	)))
	{
		CHAR privilegesBuffer[FIELD_OFFSET(TOKEN_PRIVILEGES, Privileges) + sizeof(LUID_AND_ATTRIBUTES) * 9];
		PTOKEN_PRIVILEGES privileges;
		ULONG i;

		privileges = (PTOKEN_PRIVILEGES)privilegesBuffer;
		privileges->PrivilegeCount = 9;

		for (i = 0; i < privileges->PrivilegeCount; i++)
		{
			privileges->Privileges[i].Attributes = SE_PRIVILEGE_ENABLED;
			privileges->Privileges[i].Luid.HighPart = 0;
		}

		privileges->Privileges[0].Luid.LowPart = SE_DEBUG_PRIVILEGE;
		privileges->Privileges[1].Luid.LowPart = SE_INC_BASE_PRIORITY_PRIVILEGE;
		privileges->Privileges[2].Luid.LowPart = SE_INC_WORKING_SET_PRIVILEGE;
		privileges->Privileges[3].Luid.LowPart = SE_LOAD_DRIVER_PRIVILEGE;
		privileges->Privileges[4].Luid.LowPart = SE_PROF_SINGLE_PROCESS_PRIVILEGE;
		privileges->Privileges[5].Luid.LowPart = SE_BACKUP_PRIVILEGE;
		privileges->Privileges[6].Luid.LowPart = SE_RESTORE_PRIVILEGE;
		privileges->Privileges[7].Luid.LowPart = SE_SHUTDOWN_PRIVILEGE;
		privileges->Privileges[8].Luid.LowPart = SE_TAKE_OWNERSHIP_PRIVILEGE;

		NtAdjustPrivilegesToken(
			tokenHandle,
			FALSE,
			privileges,
			0,
			NULL,
			NULL
		);

		NtClose(tokenHandle);
	}
}

HMODULE GetThisModuleHandle()
{
    //Returns module handle where this function is running in: EXE or DLL
    HMODULE hModule = NULL;
    ::GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | 
        GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, 
        (LPCTSTR)GetThisModuleHandle, &hModule);

    return hModule;
}

// Hook: NtMapViewOfSection

#include "WinHelpers/FuncHook/HookUtils.h"

typedef NTSTATUS (*P_NtMapViewOfSection)(
	IN  HANDLE SectionHandle,
	IN  HANDLE ProcessHandle,
	IN  OUT PVOID *BaseAddress,
	IN  ULONG_PTR ZeroBits,
	IN  SIZE_T CommitSize,
	IN  OUT PLARGE_INTEGER SectionOffset OPTIONAL,
	IN  OUT PSIZE_T ViewSize,
	IN  ULONG InheritDisposition,
	IN  ULONG AllocationType,
	IN  ULONG Protect);

P_NtMapViewOfSection NtMapViewOfSectionTramp = NULL;

//
// Whether a view NtMapViewOfSection has just reported as mapped is really there.
//
// The question is whether the mapping still *exists*, not whether the memory can
// be read this instant - see MyMapViewOfSection for what this guards against.
// Three things the older test got wrong, each of which denied a legitimate map:
//
//  - A reserved view is legitimate, and is the common case for anything that
//    commits as it goes. RtlCreateQueryDebugBuffer maps its buffer reserved and
//    commits pages on demand - that is what its MaximumCommit argument means -
//    and MEMORY_BASIC_INFORMATION::Protect is documented as meaningful only for
//    MEM_COMMIT, reading back as zero for everything else. Calling that
//    unreadable turned every heap query in this process into ACCESS_DENIED.
//
//  - Copy on write is readable. PAGE_WRITECOPY and PAGE_EXECUTE_WRITECOPY are
//    how image sections are ordinarily mapped, and neither was in the test.
//
//  - The protection word carries modifier bits - PAGE_GUARD, PAGE_NOCACHE,
//    PAGE_WRITECOMBINE - which have to come off before it is compared, and the
//    old bitwise test would also have accepted anything that merely happened to
//    share a bit with one of the four values it listed.
//
bool IsMappingPresent(PVOID Address)
{
	MEMORY_BASIC_INFORMATION mbi;
	if (VirtualQuery(Address, &mbi, sizeof(mbi)) == 0)
		return false;	// no such region at all

	//
	// Free is the one that matters: it is what the address looks like after the
	// driver has unmapped the section behind our back.
	//
	if (mbi.State == MEM_FREE)
		return false;
	if (mbi.State == MEM_RESERVE)
		return true;

	switch (mbi.Protect & ~(PAGE_GUARD | PAGE_NOCACHE | PAGE_WRITECOMBINE))
	{
	case PAGE_READONLY:
	case PAGE_READWRITE:
	case PAGE_WRITECOPY:
	case PAGE_EXECUTE_READ:
	case PAGE_EXECUTE_READWRITE:
	case PAGE_EXECUTE_WRITECOPY:
		return true;
	}
	return false;
}

NTSTATUS NTAPI MyMapViewOfSection(
	IN  HANDLE SectionHandle,
	IN  HANDLE ProcessHandle,
	IN  OUT PVOID* BaseAddress,
	IN  ULONG_PTR ZeroBits,
	IN  SIZE_T CommitSize,
	IN  OUT PLARGE_INTEGER SectionOffset OPTIONAL,
	IN  OUT PSIZE_T ViewSize,
	IN  ULONG InheritDisposition,
	IN  ULONG AllocationType,
	IN  ULONG Protect)
{
	NTSTATUS status = NtMapViewOfSectionTramp(SectionHandle, ProcessHandle, BaseAddress, ZeroBits, CommitSize, SectionOffset, ViewSize, InheritDisposition, AllocationType, Protect);

	//
	// Only for a view mapped into *this* process. VirtualQuery answers about the
	// caller's address space and nothing else, so for any other process it would
	// be reading an unrelated address of our own and judging the map by it.
	//
	if (NT_SUCCESS(status) && !g_MyCrashHandlerExceptionFilter_Engaged
		&& ProcessHandle == NtCurrentProcess())
	{
		if (BaseAddress && *BaseAddress && !IsMappingPresent(*BaseAddress))
		{
			DbgPrint("MyMapViewOfSection: Invalid BaseAddress: %p", *BaseAddress);
			status = STATUS_ACCESS_DENIED;
		}
	}
	return status;
}

int InitPH()
{
	HINSTANCE Instance = GetThisModuleHandle(); // (HINSTANCE)::GetModuleHandle(NULL);
	LONG result;

	//
	// If a dll is not signed like a shell extension for the default windows file open dialog,
	// or alike, we have a problem as our driver when ImageLoadProtection == TRUE will block the loading of the dll
	// and unmap the just loaded section from the driver, so we add a sanity check for NtMapViewOfSection
	// if it returns no error but the memory is not readable return STATUS_ACCESS_DENIED instead.
	//

	HookFunction(GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtMapViewOfSection"), MyMapViewOfSection, (VOID**)&NtMapViewOfSectionTramp);

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	//if (!NT_SUCCESS(PhInitializePhLibEx(L"Task Explorer", ULONG_MAX, Instance, 0, 0)))
	if (!NT_SUCCESS(PhInitializePhLib(L"Task Explorer")))
		return 1;
	//if (!PhInitializeExceptionPolicy())
	//	return 1;
	//if (!PhInitializeNamespacePolicy())
	//	return 1;
	//if (!PhInitializeMitigationPolicy())
	//	return 1;
	//if (!PhInitializeRestartPolicy())
	//    return 1;

	KphInitialize();

	//PhpProcessStartupParameters();
	PhpEnablePrivileges();

	PhSettingsInitialization();
    //PhpInitializeSettings();
	// note: this is needed to open the permissions panel
	PhpAddIntegerSetting(L"EnableSecurityAdvancedDialog", L"1");
	PhpAddStringSetting(L"FileBrowseExecutable", L"%SystemRoot%\\explorer.exe /select,\"%s\"");
	// for permissions diaog on win 7
	PhpAddIntegerSetting(L"EnableThemeSupport", L"0");
	PhpAddIntegerSetting(L"GraphColorMode", L"1");
	PhpAddIntegerSetting(L"TreeListBorderEnable", L"0");

	return 0;
}

//
// Defined in SystemAPI.cpp - see TeProcessBlockingAllowed there.
//
extern bool TeProcessBlockingAllowed();

bool (*g_KernelProcessMonitor)(quint64 ProcessId, quint64 ParentId, const QString& FileName, const QString& CommandLine) = NULL;

void (*g_KernelDebugLogger)(const QString& Output) = NULL;

extern "C" {

static VOID NTAPI KsiCommsCallback(
    _In_ ULONG_PTR ReplyToken,
    _In_ PCKPH_MESSAGE Message
    )
{
	if (Message->Header.MessageId == KphMsgRequiredStateFailure &&
		Message->Kernel.RequiredStateFailure.ClientId.UniqueProcess == NtCurrentProcessId())
	{
		// force the cached value to be updated
		KphLevelEx(FALSE);
	}

	//static QMap<void*,int> killMap;
	//static QMutex killMutex;
	switch (Message->Header.MessageId)
	{
		case KphMsgProcessCreate:
		{
			PKPH_MESSAGE msg = KphCreateMessage(KPH_MESSAGE_MIN_SIZE);

			KphMsgInit(msg, KphMsgProcessCreate);

			if (g_KernelProcessMonitor) 
			{
				quint64 ProcessId = (quint64)Message->Kernel.ProcessCreate.TargetProcessId;
				quint64 ParentId = (quint64)Message->Kernel.ProcessCreate.ParentProcessId;

				QString FileName;
				UNICODE_STRING fileName = { 0 };
				if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldFileName, &fileName))) {
					PPH_STRING oldFileName = PhCreateString(fileName.Buffer);
					PPH_STRING newFileName = PhGetFileName(oldFileName);
					PhDereferenceObject(oldFileName);
					FileName = CastPhString(newFileName);
				}

				QString CommandLine;
				UNICODE_STRING commandLine = { 0 };
				if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldCommandLine, &commandLine)))
					CommandLine = QString::fromWCharArray(commandLine.Buffer, commandLine.Length / sizeof(wchar_t));

				//
				// The verdict is only asked for where this process is the gate. Where
				// it is not, the callback still runs - it is what records the new
				// process with its command line - and whatever it would have refused
				// is not acted on, because nothing is waiting to hear it.
				//
				const bool bAllow = g_KernelProcessMonitor(ProcessId, ParentId, FileName, CommandLine);
				msg->Reply.ProcessCreate.CreationStatus =
					(bAllow || !TeProcessBlockingAllowed()) ? STATUS_SUCCESS : STATUS_ACCESS_DENIED;
			}
			else
				msg->Reply.ProcessCreate.CreationStatus = STATUS_SUCCESS;

			if (TeProcessBlockingAllowed())
				KphCommsReplyMessage(ReplyToken, msg);

			PhDereferenceObject(msg);

			break;
		}
		case KphMsgDebugPrint:
		{
			ANSI_STRING aStr;
			if (NT_SUCCESS(KphMsgDynGetAnsiString(Message, KphMsgFieldOutput, &aStr)))
			{
				if(g_KernelDebugLogger)
					g_KernelDebugLogger(QString::fromLatin1(aStr.Buffer, aStr.Length));
			}
			break;
		}
		/*case KphMsgFilePreCreate:
		{
			PPH_FREE_LIST freelist = KphGetMessageFreeList();

			PKPH_MESSAGE msg = (PKPH_MESSAGE)PhAllocateFromFreeList(freelist);
			KphMsgInit(msg, KphMsgFilePreCreate);
			msg->Reply.File.Pre.Create.Status = STATUS_SUCCESS;

			UNICODE_STRING fileName = { 0 };
			if (NT_SUCCESS(KphMsgDynGetUnicodeString(Message, KphMsgFieldFileName, &fileName))) {
				QString Name = QString::fromWCharArray(fileName.Buffer, fileName.Length / sizeof(WCHAR));
				if (Name.startsWith("\\Device\\ImDisk")) {
					qDebug() << "BAM:" << Name;
					QMutexLocker Lock(&killMutex);
					killMap[Message->Kernel.File.FileObject]++;
				}
			}

			KphCommsReplyMessage(ReplyToken, msg);

			PhFreeToFreeList(freelist, msg);

			break;
		}
		case KphMsgFilePostCreate:
		{
			PPH_FREE_LIST freelist = KphGetMessageFreeList();

			PKPH_MESSAGE msg = (PKPH_MESSAGE)PhAllocateFromFreeList(freelist);
			KphMsgInit(msg, KphMsgFilePostCreate);
			msg->Reply.File.Post.Create.Status = STATUS_SUCCESS;

			{
				QMutexLocker Lock(&killMutex);
				if (killMap[Message->Kernel.File.FileObject])
				{
					msg->Reply.File.Post.Create.Status = STATUS_ACCESS_DENIED;
					killMap[Message->Kernel.File.FileObject]--;
					qDebug() << "BAM: !!!";
				}
			}


			KphCommsReplyMessage(ReplyToken, msg);

			PhFreeToFreeList(freelist, msg);

			break;
		}*/
	}
}

BOOLEAN g_MonitorDebug = FALSE;
BOOLEAN g_MonitorSystem = FALSE;

/**
 * @brief Configures the kernel driver's informer (event notification) system.
 *
 * The driver uses three levels of settings that filter events in sequence:
 *
 * 1. GLOBAL SETTINGS (KphSetInformerSettings):
 *    - System-wide master switch controlling what events the driver generates
 *    - Affects ALL connected clients
 *    - If an event type is disabled here, no client receives it
 *
 * 2. PER-PROCESS SETTINGS (KphSetInformerProcessSettings):
 *    - Filters events ABOUT a specific process (identified by handle)
 *    - Used to prevent feedback loops / "log explosion" where monitoring
 *      our own activity generates more events to monitor
 *    - Example: Mute file events about ourselves to prevent infinite loop:
 *      TaskExplorer logs -> writes file -> driver sees write -> generates event
 *      -> TaskExplorer logs -> writes file -> ... (explosion)
 *
 * 3. PER-CLIENT SETTINGS (KphSetInformerClientSettings):
 *    - Settings specific to THIS client connection
 *    - Multiple clients can connect with different settings
 *    - Controls message timeouts and per-client rate limiting
 *    - Events must pass both global AND client filters to be received
 *
 * Flow: Driver generates event -> Global filter -> Process filter -> Client filter -> Received
 */
VOID PhInformerActivate(
	VOID
)
{
	KPH_INFORMER_SETTINGS settings;
	KPH_INFORMER_CLIENT_SETTINGS client;

	//
	// STEP 1: Configure PER-PROCESS settings for our own process
	//
	// This prevents the "feedback loop" problem where monitoring our own
	// activity would cause an explosion of events. We mute all events
	// ABOUT ourselves, except RequiredStateFailure which is critical
	// security feedback we need to see (e.g., if our process state degrades).
	//
	memset(&settings, 0, sizeof(settings));
	settings.Policy[KPH_INFORMER_INDEX(RequiredStateFailure)] = KPH_RATE_LIMIT_UNLIMITED;
	KphSetInformerProcessSettings(NtCurrentProcess(), &settings);

	//
	// If no monitoring features are enabled, disable all global events
	// and return early - no need to configure client settings.
	//
	if (!g_MonitorDebug && !g_MonitorSystem)
	{
		KphSetInformerSettings(&settings);
		return;
	}

	//
	// STEP 2: Configure GLOBAL settings (affects all clients system-wide)
	//
	// Enable most informer option flags but disable expensive/unnecessary ones:
	// - Stack traces: Performance overhead, not needed for basic monitoring
	// - Reply modes: We don't need to intercept/block operations
	// - Buffer captures: Large data transfers we don't need
	//
	memset(&settings, 0, sizeof(settings));
	//settings.Options.Flags = ULONG_MAX;						// Enable most options
	//settings.Options.EnableStackTraces = FALSE;				// Skip expensive stack capture
	//settings.Options.EnableProcessCreateReply = FALSE;		// Don't need to reply/block process creation
	//settings.Options.FileEnablePreCreateReply = FALSE;		// Don't need to reply to file pre-create
	//settings.Options.FileEnablePostCreateReply = FALSE;		// Don't need to reply to file post-create
	//settings.Options.FileEnablePostFileNames = FALSE;		// Skip post-operation filename resolution
	//settings.Options.FileEnableIoControlBuffers = FALSE;	// Skip IOCTL buffer capture
	//settings.Options.FileEnableFsControlBuffers = FALSE;	// Skip FSCTL buffer capture
	//settings.Options.FileEnableDirControlBuffers = FALSE;	// Skip directory control buffers
	//settings.Options.RegEnablePostObjectNames = FALSE;		// Skip registry post-op object names
	//settings.Options.RegEnablePostValueNames = FALSE;		// Skip registry post-op value names
	//settings.Options.RegEnableValueBuffers = FALSE;			// Skip registry value buffer capture

	//
	// Start with all event types disabled (DENY_ALL), then selectively
	// enable only the ones we need based on monitoring flags.
	//
	for (ULONG i = 0; i < KPH_INFORMER_COUNT; i++)
		settings.Policy[i] = KPH_RATE_LIMIT_DENY_ALL;

	// Enable kernel debug output monitoring if requested
	if (g_MonitorDebug)
		settings.Policy[KPH_INFORMER_INDEX(DebugPrint)] = KPH_RATE_LIMIT_UNLIMITED;
	else
		settings.Policy[KPH_INFORMER_INDEX(DebugPrint)] = KPH_RATE_LIMIT_DENY_ALL;

	// Enable process creation monitoring if requested
	if (g_MonitorSystem)
	{
		settings.Policy[KPH_INFORMER_INDEX(ProcessCreate)] = KPH_RATE_LIMIT_UNLIMITED;

		//
		// Being told, and being asked, are two settings and only one of them is
		// ours to take for granted.
		//
		// The events above are an observation: the driver reports a process
		// creation and carries the image path and command line with it, which
		// is the earliest and most reliable place to read either - by the time
		// a poll notices the process, both can already have been changed.
		//
		// EnableProcessCreateReply is the other thing. With it the driver stops
		// the creation and waits for this process to answer, and takes a refusal
		// as one. That makes every program start on the machine depend on this
		// one being alive and prompt, which is a claim over the computer rather
		// than a view of it - so it is asked for, not assumed. See
		// CSystemAPI::ProcessBlockingAllowed and DRIVER-EXPOSURE.md.
		//
		settings.Options.EnableProcessCreateReply = TeProcessBlockingAllowed() ? TRUE : FALSE;
	}
	else
	{
		settings.Policy[KPH_INFORMER_INDEX(ProcessCreate)] = KPH_RATE_LIMIT_DENY_ALL;
	}

	// Apply the global settings - this affects what the driver generates
	KphSetInformerSettings(&settings);

	//
	// STEP 3: Configure PER-CLIENT settings (specific to this connection)
	//
	// These settings control how messages are delivered to THIS client:
	// - Message timeouts: How long the driver waits for us to process messages
	// - AsyncQueuePolicy: Rate limiting for the async message queue
	// - InformerPolicy: Per-message-type rate limits for this client only
	//
	// Multiple clients can connect with different settings. Even if global
	// settings enable an event, a client can filter it out here.
	//
	memset(&client, 0, sizeof(client));

	// Set timeouts for synchronous message processing (3 seconds each)
	PhTimeoutFromMilliseconds(&client.MessageTimeouts.AsyncTimeout, 3000);
	PhTimeoutFromMilliseconds(&client.MessageTimeouts.DefaultTimeout, 3000);
	PhTimeoutFromMilliseconds(&client.MessageTimeouts.ProcessCreateTimeout, 3000);
	PhTimeoutFromMilliseconds(&client.MessageTimeouts.FilePreCreateTimeout, 3000);
	PhTimeoutFromMilliseconds(&client.MessageTimeouts.FilePostCreateTimeout, 3000);

	// Allow unlimited async queue throughput (no rate limiting on our end)
	//client.AsyncQueuePolicy = KPH_RATE_LIMIT_PER_SEC(1000, 30000);
	client.AsyncQueuePolicy = KPH_RATE_LIMIT_UNLIMITED;

	// Accept all message types that pass the global filter (no additional client-side filtering)
	for (ULONG i = 0; i < KPH_INFORMER_COUNT; i++)
		client.InformerPolicy[i] = KPH_RATE_LIMIT_UNLIMITED;

	// Apply the client-specific settings
	KphSetInformerClientSettings(&client);
}

NTSTATUS KsiReadConfiguration(
	const QString &Path,
	_In_ PCWSTR FileName,
	_Out_ PBYTE* Data,
	_Out_ PULONG Length
)
{
	NTSTATUS status;
	//PPH_STRING fileName;
	HANDLE fileHandle;

	*Data = NULL;
	*Length = 0;

	status = STATUS_NO_SUCH_FILE;

	//fileName = PhGetApplicationDirectoryFileNameZ(FileName, TRUE);
	//if (fileName)
	{
		//if (NT_SUCCESS(status = PhCreateFile(
		if (NT_SUCCESS(status = PhCreateFileWin32(
			&fileHandle,
		//	&fileName->sr,
			(wchar_t*)(Path + "\\" + QString::fromWCharArray(FileName)).utf16(),
			FILE_GENERIC_READ,
			FILE_ATTRIBUTE_NORMAL,
			FILE_SHARE_READ,
			FILE_OPEN,
			FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
		)))
		{
			status = PhGetFileData(fileHandle, (PVOID*)Data, Length);

			NtClose(fileHandle);
		}

		//PhDereferenceObject(fileName);
	}

	return status;
}

/*NTSTATUS KsiValidateDynamicConfiguration(
	_In_ PBYTE DynData,
	_In_ ULONG DynDataLength
)
{
	NTSTATUS status;
	PPH_STRING fileName;
	PVOID versionInfo;
	VS_FIXEDFILEINFO* fileInfo;

	status = STATUS_NO_SUCH_FILE;

	if (fileName = PhGetKernelFileName2())
	{
		if (versionInfo = PhGetFileVersionInfoEx(&fileName->sr))
		{
			if (fileInfo = PhGetFileVersionFixedInfo(versionInfo))
			{
				status = KphDynDataGetConfiguration(
					(PKPH_DYNDATA)DynData,
					DynDataLength,
					HIWORD(fileInfo->dwFileVersionMS),
					LOWORD(fileInfo->dwFileVersionMS),
					HIWORD(fileInfo->dwFileVersionLS),
					LOWORD(fileInfo->dwFileVersionLS),
					NULL
				);
			}

			PhFree(versionInfo);
		}

		PhDereferenceObject(fileName);
	}

	return status;
}*/

NTSTATUS KsiGetDynData(
	const QString &Path,
	_Out_ PBYTE* DynData,
	_Out_ PULONG DynDataLength,
	_Out_ PBYTE* Signature,
	_Out_ PULONG SignatureLength
)
{
	NTSTATUS status;
	PBYTE data = NULL;
	ULONG dataLength;
	PBYTE sig = NULL;
	ULONG sigLength;

	*DynData = NULL;
	*DynDataLength = 0;
	*Signature = NULL;
	*SignatureLength = 0;

	status = KsiReadConfiguration(Path, L"ksidyn.bin", &data, &dataLength);
	if (!NT_SUCCESS(status))
		goto CleanupExit;

	//status = KsiValidateDynamicConfiguration(data, dataLength);
	//if (!NT_SUCCESS(status))
	//	goto CleanupExit;

	status = KsiReadConfiguration(Path, L"ksidyn.sig", &sig, &sigLength);
	if (!NT_SUCCESS(status))
		goto CleanupExit;
	
	if (!sigLength)
	{
		status = STATUS_SI_DYNDATA_INVALID_SIGNATURE;
		goto CleanupExit;
	}

	*DynDataLength = dataLength;
	*DynData = data;
	data = NULL;

	*SignatureLength = sigLength;
	*Signature = sig;
	sig = NULL;

	status = STATUS_SUCCESS;

CleanupExit:
	if (data)
		PhFree(data);
	if (sig)
		PhFree(sig);

	return status;
}

}


bool KphSetDebugLog(bool Enable)
{
	g_MonitorDebug = Enable ? TRUE : FALSE;
	PhInformerActivate();
	return false;
}

bool KphSetSystemMon(bool Enable)
{
	g_MonitorSystem = Enable ? TRUE : FALSE;
	PhInformerActivate();
	return false;
}

bool KphGetSystemMon()
{
	return g_MonitorSystem;
}

PPH_STRING KsiServiceName = NULL;
BOOLEAN KsiEnableLoadNative = FALSE;
BOOLEAN KsiEnableLoadFilter = FALSE;

BOOLEAN KsiEnableUnloadProtection = FALSE;

BOOLEAN g_KphStartupMax = FALSE;
BOOLEAN g_KphStartupHigh = FALSE;

NTSTATUS PhRestartSelf(
	_In_ PPH_STRINGREF AdditionalCommandLine
)
{
#ifndef DEBUG
	static ULONG64 mitigationFlags[] =
	{
		(PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON |
		PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON |
		PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON |
		PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY_IMAGE_LOAD_PREFER_SYSTEM32_ALWAYS_ON |
		PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_ON ),
		(PROCESS_CREATION_MITIGATION_POLICY2_LOADER_INTEGRITY_CONTINUITY_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_STRICT_CONTROL_FLOW_GUARD_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_BLOCK_NON_CET_BINARIES_ALWAYS_ON |
		// PROCESS_CREATION_MITIGATION_POLICY2_XTENDED_CONTROL_FLOW_GUARD_ALWAYS_ON |
		PROCESS_CREATION_MITIGATION_POLICY2_MODULE_TAMPERING_PROTECTION_ALWAYS_ON)
	};
#endif
	NTSTATUS status;
	PPROC_THREAD_ATTRIBUTE_LIST attributeList = NULL;
	PH_STRINGREF commandlineSr;
	PPH_STRING commandline;
	STARTUPINFOEX startupInfo;

	status = PhGetProcessCommandLineStringRef(&commandlineSr);

	if (!NT_SUCCESS(status))
		return status;

	commandline = PhConcatStringRef2(
		&commandlineSr,
		AdditionalCommandLine
	);

	QString ServiceName = CTaskService::RunService();
	if (!ServiceName.isEmpty())
	{
		QVariantMap Parameters;
		Parameters["CommandLine"] = QString::fromWCharArray(PhGetString(commandline));
#ifndef DEBUG
		Parameters["MitigationFlags0"] = mitigationFlags[0];
		Parameters["MitigationFlags1"] = mitigationFlags[1];
#endif

		QVariantMap Request;
		Request["Command"] = "CreateProcessForKsi";
		Request["Parameters"] = Parameters;

		QVariant Response = CTaskService::SendCommand(ServiceName, Request);
		if (Response.isValid())
			PhExitApplication(STATUS_SUCCESS);
	}

#ifndef DEBUG
	status = PhInitializeProcThreadAttributeList(&attributeList, 1);

	if (!NT_SUCCESS(status))
		return status;

	if (WindowsVersion >= WINDOWS_10_22H2)
	{
		status = PhUpdateProcThreadAttribute(
			attributeList,
			PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
			mitigationFlags,
			sizeof(ULONG64) * 2
		);
	}
	else
	{
		status = PhUpdateProcThreadAttribute(
			attributeList,
			PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
			mitigationFlags,
			sizeof(ULONG64) * 1
		);
	}
#endif

	if (!NT_SUCCESS(status))
		return status;

	memset(&startupInfo, 0, sizeof(STARTUPINFOEX));
	startupInfo.StartupInfo.cb = sizeof(STARTUPINFOEX);
	startupInfo.lpAttributeList = attributeList;

	status = PhCreateProcessWin32Ex(
		NULL,
		PhGetString(commandline),
		NULL,
		NULL,
		&startupInfo,
		PH_CREATE_PROCESS_DEFAULT_ERROR_MODE | PH_CREATE_PROCESS_EXTENDED_STARTUPINFO,
		NULL,
		NULL,
		NULL,
		NULL
	);

	if (NT_SUCCESS(status))
	{
		PhExitApplication(STATUS_SUCCESS);
	}

	if (attributeList)
		PhDeleteProcThreadAttributeList(attributeList);

	PhDereferenceObject(commandline);

	return status;
}

BOOLEAN PhDoesOldKsiExist(
	VOID
)
{
	static PH_STRINGREF ksiOld = PH_STRINGREF_INIT(L"ksi.dll-old");
	BOOLEAN result = FALSE;
	PPH_STRING applicationDirectory;
	PPH_STRING fileName;

	if (!(applicationDirectory = PhGetApplicationDirectory()))
		return FALSE;

	if (fileName = PhConcatStringRef2(&applicationDirectory->sr, &ksiOld))
	{
		if (result = PhDoesFileExist(&fileName->sr))
		{
			// If the file exists try to delete it. If we can't a reboot is
			// still required since it's likely still mapped into the kernel.
			if (NT_SUCCESS(PhDeleteFile(&fileName->sr)))
				result = FALSE;
		}

		PhDereferenceObject(fileName);
	}

	PhDereferenceObject(applicationDirectory);
	return result;
}

bool IsOnARM64()
{
	HMODULE Kernel32 = GetModuleHandleW(L"Kernel32.dll");
	typedef BOOL(WINAPI* LPFN_ISWOW64PROCESS2)(HANDLE, PUSHORT, PUSHORT);
	LPFN_ISWOW64PROCESS2 pIsWow64Process2 = (LPFN_ISWOW64PROCESS2)GetProcAddress(Kernel32, "IsWow64Process2");
	if (pIsWow64Process2)
	{
		USHORT ProcessMachine = 0xFFFF;
		USHORT NativeMachine = 0xFFFF;
		BOOL ok = pIsWow64Process2(GetCurrentProcess(), &ProcessMachine, &NativeMachine);

		if (NativeMachine == IMAGE_FILE_MACHINE_ARM64)
			return true;
	}
	return false;
}

bool g_KsiDynDataLoaded = false;
ULONG g_KsiClientProtectionLevel = 0;

STATUS KsiActivateDynData(const QString& FileName, _In_ KPH_LEVEL Level)
{
	STATUS Status = OK;

	NTSTATUS status;
	PBYTE dynData = NULL;
	ULONG dynDataLength;
	PBYTE signature = NULL;
	ULONG signatureLength;

	status = KsiGetDynData(Split2(FileName, "\\", true).first, &dynData, &dynDataLength, &signature, &signatureLength);
	if (!NT_SUCCESS(status))
		Status = ERR(TE_UnsupportedWindowsVersion, STATUS_UNKNOWN_REVISION);
	else
	{
		status = KphActivateDynData(dynData, dynDataLength, signature, signatureLength);
		if (!NT_SUCCESS(status))
			Status = ERR(TE_DriverActivateFailed, status);
		else
			g_KsiDynDataLoaded = true;
	}

	if (signature)
		PhFree(signature);
	if (dynData)
		PhFree(dynData);

	return Status;
}

//
// Where the driver file for this machine lives. InitKSI, the driver window and
// the DynData update all used to work this out for themselves.
//
QString KsiGetDriverPath(const QString& AppDir)
{
	QString FileName = theConf->GetString("OptionsKSI/FileName", "KTaskExplorer.sys");

	// if the file name is not a full path add the application directory
	if (!FileName.contains("\\"))
	{
		if (IsOnARM64())
			FileName = AppDir + "\\ARM64\\" + FileName;
		else
			FileName = AppDir + "\\AMD64\\" + FileName;
	}

	return FileName.replace("/", "\\");
}

STATUS InitKSI(const QString& AppDir)
{
	QString FileName = theConf->GetString("OptionsKSI/FileName", "KTaskExplorer.sys");
	QString ServiceName = theConf->GetString("OptionsKSI/DeviceName", "KTaskExplorer");
	QString ObjectName = "\\Driver\\" + ServiceName;
	QString PortName = "\\" + ServiceName;
	QString Altitude = theConf->GetString("OptionsKSI/Altitude", "385210.8");
	QStringList Info;

	KsiEnableLoadNative = theConf->GetBool("OptionsKSI/EnableLoadNative", false);
	KsiEnableLoadFilter = theConf->GetBool("OptionsKSI/EnableLoadFilter", false);

	FileName = KsiGetDriverPath(AppDir);
	if (!QFile::exists(FileName))
		return ERR(TE_KernelDriverFile, QVariantList() << FileName, STATUS_NOT_FOUND);

	if (!PhGetOwnTokenAttributes().Elevated)
		return ERR(TE_DriverNeedsAdmin, STATUS_ELEVATION_REQUIRED);

	if(PhIsExecutingInWow64())
		return ERR(TE_DriverOnly64Bit, STATUS_IMAGE_MACHINE_TYPE_MISMATCH);

	NTSTATUS status;

	// todo: fix-me
	//if (PhDoesOldKsiExist())
	//{
	//    if (PhGetIntegerSetting(L"EnableKphWarnings") && !PhStartupParameters.PhSvc)
	//    {
	//        PhShowKsiError(
	//            L"Unable to load kernel driver, the last System Informer update requires a reboot.",
	//            STATUS_PENDING 
	//            );
	//    }
	//    return;
	//}

	STATUS Status = OK;
	KsiServiceName = CastQString(ServiceName);
	PPH_STRING ksiFileName = CastQString(FileName);
	KPH_CONFIG_PARAMETERS config = { 0 };
	PPH_STRING objectName = CastQString(ObjectName);
	PPH_STRING portName = CastQString(PortName);
	PPH_STRING altitude = CastQString(Altitude);
	PPH_STRING systemProcessName = NULL; //CastQString("System Informer Kernel");
	KPH_LEVEL level;
	KPH_PROCESS_STATE processState;
	
	UNICODE_STRING fileName;
	WCHAR buffer[MAX_PATH];
	DWORD size = GetProcessImageFileNameW(GetCurrentProcess(), buffer, MAX_PATH);
	*wcsrchr(buffer, L'\\') = '\0';
	UNICODE_STRING ustr;
	RtlInitUnicodeString(&ustr, buffer);
	PPH_STRING clientPath = PhCreateStringFromUnicodeString(&ustr);

	config.FileName = &ksiFileName->sr;
	config.ServiceName = &KsiServiceName->sr;
	config.ObjectName = &objectName->sr;
	config.PortName = &portName->sr;
	config.Altitude = &altitude->sr;
	config.SystemProcessName = (systemProcessName ? &systemProcessName->sr : NULL);

	config.FsSupportedFeatures = 0; // not yet in driver?
	if (theConf->GetBool("OptionsKSI/EnableFsFeatureOffloadRead", false))
		SetFlag(config.FsSupportedFeatures, SUPPORTED_FS_FEATURES_OFFLOAD_READ);
	if (theConf->GetBool("OptionsKSI/EnableFsFeatureOffloadWrite", false))
		SetFlag(config.FsSupportedFeatures, SUPPORTED_FS_FEATURES_OFFLOAD_WRITE);
	if (theConf->GetBool("OptionsKSI/EnableFsFeatureQueryOpen", false))
		SetFlag(config.FsSupportedFeatures, SUPPORTED_FS_FEATURES_QUERY_OPEN);
	if (theConf->GetBool("OptionsKSI/EnableFsFeatureBypassIO", false))
		SetFlag(config.FsSupportedFeatures, SUPPORTED_FS_FEATURES_BYPASS_IO);

	config.Flags.Flags = 0;
#ifdef _DEBUG 
	config.Flags.DisableImageLoadProtection = theConf->GetBool("OptionsKSI/DisableImageLoadProtection", false);
	config.Flags.AllowDebugging = theConf->GetBool("OptionsKSI/AllowDebugging", true);
#else
	config.Flags.DisableImageLoadProtection = theConf->GetBool("OptionsKSI/DisableImageLoadProtection", false);
	config.Flags.AllowDebugging = theConf->GetBool("OptionsKSI/AllowDebugging", false);
#endif
	config.Flags.RandomizedPoolTag = theConf->GetBool("OptionsKSI/RandomizedPoolTag", false);
	config.Flags.DynDataNoEmbedded = theConf->GetBool("OptionsKSI/DynDataNoEmbedded", false);
	config.Flags.DisableSystemProcess = theConf->GetBool("OptionsKSI/DisableSystemProcess", false);
	config.Flags.DisableThreadNames = theConf->GetBool("OptionsKSI/DisableThreadNames", false);

//#ifndef _DEBUG
	config.ClientPath = &clientPath->sr;
//#endif

	config.EnableNativeLoad = KsiEnableLoadNative;
	config.EnableFilterLoad = KsiEnableLoadFilter;

	config.RingBufferLength = theConf->GetInt("OptionsKSI/RingBufferLength", 10000000); // in bytes 10MB
	config.Callback = (PKPH_COMMS_CALLBACK)KsiCommsCallback;

	status = KphConnect(&config);

	if (status == STATUS_NO_SUCH_FILE)
	{
		//PPH_STRING tempFileName;

		//
		// N.B. We know the driver file exists from the check above. This
		// error indicates that the kernel driver is still mapped but
		// unloaded.
		//
		// The chain of calls and resulting return status is this:
		// IopLoadDriver
		// -> MmLoadSystemImage -> STATUS_IMAGE_ALREADY_LOADED
		// -> ObOpenObjectByName -> STATUS_OBJECT_NAME_NOT_FOUND
		// -> STATUS_DRIVER_FAILED_PRIOR_UNLOAD -> STATUS_NO_SUCH_FILE
		//
		// The driver object has been made temporary and the underlying
		// section will be unmapped by the kernel when it is safe to do so.
		// This generally happens when the pinned module is holding the
		// driver in memory until some code finishes executing. This can
		// also happen if another misbehaving driver is leaking or holding
		// a reference to the driver object.
		//
		// To continue to provide a good user experience, we will attempt copy
		// the driver to another file and load it again. First, try to use an
		// existing temporary driver file, if one exists.
		//

		//tempFileName = PhGetStringSetting(SETTING_KSI_PREVIOUS_TEMPORARY_DRIVER_FILE);
		//if (tempFileName)
		//{
		//	if (PhDoesFileExistWin32(PhGetString(tempFileName)))
		//	{
		//		config.FileName = &tempFileName->sr;
		//
		//		status = KphConnect(&config);
		//	}
		//
		//	PhMoveReference(&KsiFileName, tempFileName);
		//}

		//if (!NT_SUCCESS(status) && tempDriverDir)
		//{
		//	if (NT_SUCCESS(status = KsiCreateTemporaryDriverFile(ksiFileName, tempDriverDir, &tempFileName)))
		//	{
		//		PhSetStringSetting(SETTING_KSI_PREVIOUS_TEMPORARY_DRIVER_FILE, PhGetString(tempFileName));
		//
		//		config.FileName = &tempFileName->sr;
		//
		//		status = KphConnect(&config);
		//
		//		PhMoveReference(&KsiFileName, tempFileName);
		//	}
		//}
	}

	if (status == STATUS_SI_KSIDLL_VERSION_MISMATCH || status == STATUS_PROCEDURE_NOT_FOUND)
	{
		Status = ERR(TE_DriverNeedsReboot, status);
		goto CleanupExit;
	}

	if (!NT_SUCCESS(status)) {
		Status = ERR(TE_DriverConnectFailed, status);
		goto CleanupExit;
	}
	
	level = KphLevelEx(FALSE);

	processState = KphGetCurrentProcessState();
	if ((processState != 0) && (processState & KPH_PROCESS_STATE_MAXIMUM) != KPH_PROCESS_STATE_MAXIMUM)
	{
		if (!BooleanFlagOn(processState, KPH_PROCESS_SECURELY_CREATED))
			Info.append("not securely created");
		if (!BooleanFlagOn(processState, KPH_PROCESS_VERIFIED_PROCESS))
			Info.append("unverified primary image");
		if (!BooleanFlagOn(processState, KPH_PROCESS_PROTECTED_PROCESS))
			Info.append("inactive protections");
		if (!BooleanFlagOn(processState, KPH_PROCESS_NO_UNTRUSTED_IMAGES))
			Info.append("unsigned images (likely an unsigned plugin)");
		if (!BooleanFlagOn(processState, KPH_PROCESS_NOT_BEING_DEBUGGED))
			Info.append("process is being debugged");
		if (!BooleanFlagOn(processState, KPH_PROCESS_NO_WRITABLE_FILE_OBJECT))
			Info.append("writable file object");
		if (!BooleanFlagOn(processState, KPH_PROCESS_CREATE_NOTIFICATION))
			Info.append("missing create notification");
		//if (!BooleanFlagOn(processState, KPH_PROCESS_NO_VERIFY_TIMEOUT))
		//	Info.append("verify time out");
		if ((processState & KPH_PROCESS_STATE_MINIMUM) != KPH_PROCESS_STATE_MINIMUM)
			Info.append("tampered primary image");
	}

	Status = KsiActivateDynData(FileName, level);

	if (level != KphLevelMax)
	{
		Status = ERR(TE_Generic, QVariantList() << QString("Unable to access the kernel driver: %1.").arg(Info.join(", ")), STATUS_ACCESS_DENIED);

		if (config.Flags.AllowDebugging || !NtCurrentPeb()->BeingDebugged)
		{
			if ((level == KphLevelHigh) && !g_KphStartupMax)
			{
				PH_STRINGREF commandline = PH_STRINGREF_INIT(L" -kx");
				status = PhRestartSelf(&commandline);
			}

			if ((level < KphLevelHigh) && !g_KphStartupMax && !g_KphStartupHigh)
			{
				PH_STRINGREF commandline = PH_STRINGREF_INIT(L" -kh");
				status = PhRestartSelf(&commandline);
			}

			if (!NT_SUCCESS(status))
				Status = ERR(TE_RestartSelfFailed, STATUS_ACCESS_DENIED);
		}
	}

	if (level == KphLevelMax)
	{
		ACCESS_MASK process = 0;
		ACCESS_MASK thread = 0;

		if (theConf->GetBool("OptionsKSI/EnableUnloadProtection", false)) {
			if(NT_SUCCESS(KphAcquireDriverUnloadProtection(NULL, NULL)))
				KsiEnableUnloadProtection = TRUE;
		}

		int clientProtectionLevel = theConf->GetInt("OptionsKSI/ClientProcessProtectionLevel", 0);
		switch (clientProtectionLevel)
		{
		case 2:
			process |= (PROCESS_VM_READ | PROCESS_QUERY_INFORMATION);
			thread |= (THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION);
			__fallthrough;
		case 1:
			process |= (PROCESS_TERMINATE | PROCESS_SUSPEND_RESUME);
			thread |= (THREAD_TERMINATE | THREAD_SUSPEND_RESUME | THREAD_RESUME);
			__fallthrough;
		case 0:
		default:
			break;
		}

		if (process != 0 || thread != 0)
		{
			KphStripProtectedProcessMasks(NtCurrentProcess(), process, thread);
			// Remember that outside openers are now being refused: the seclogon
			// service cannot open us either, so the in-process run-as path
			// (CreateProcessWithLogonW) would fail. RunProgramAs checks this.
			g_KsiClientProtectionLevel = (ULONG)clientProtectionLevel;
		}
	}

	//PhInformerActivate();

CleanupExit:

	PhClearReference(&objectName);
	PhClearReference(&portName);
	PhClearReference(&altitude);
	PhClearReference(&ksiFileName);
	PhClearReference(&clientPath);

//#ifdef DEBUG
//	KsiDebugLogInitialize();
//#endif

	return Status;
}

STATUS CleanupKSI()
{
	NTSTATUS status;
	KPH_CONFIG_PARAMETERS config = { 0 };
	BOOLEAN shouldUnload;

	if (!KphCommsIsConnected())
		return OK;

	if (KsiEnableUnloadProtection) {
		KphReleaseDriverUnloadProtection(NULL, NULL);
		KsiEnableUnloadProtection = FALSE;
	}

	if (theConf->GetBool("OptionsKSI/UnloadOnExit", true))
	{
		ULONG clientCount;

		if (!NT_SUCCESS(status = KphGetConnectedClientCount(&clientCount)))
			return CStatus::Native(status);

		shouldUnload = (clientCount == 1);
	}
	else
	{
		shouldUnload = FALSE;
	}

	KphCommsStop();
//#ifdef DEBUG
//	KsiDebugLogFinalize();
//#endif

	if (!shouldUnload)
		return OK;

	if (KsiServiceName)
	{
		config.ServiceName = &KsiServiceName->sr;
		config.EnableNativeLoad = KsiEnableLoadNative;
		config.EnableFilterLoad = KsiEnableLoadFilter;
		status = KphServiceStop(&config);

		PhDereferenceObject(KsiServiceName);
	}

	if(!NT_SUCCESS(status))
		return ERR(TE_DriverServiceStopFailed, status);
	return OK;
}

static PPH_STRING KsiKernelFileName = NULL;
static PPH_STRING KsiKernelVersion = NULL;

PPH_STRING KsiGetKernelFileNameInternal(VOID)
{
	NTSTATUS status;
	UCHAR buffer[FIELD_OFFSET(RTL_PROCESS_MODULES, Modules) + sizeof(RTL_PROCESS_MODULE_INFORMATION)] = { 0 };
	PRTL_PROCESS_MODULES modules;
	ULONG modulesLength;

	modules = (PRTL_PROCESS_MODULES)buffer;
	modulesLength = sizeof(buffer);

	status = NtQuerySystemInformation(
		SystemModuleInformation,
		modules,
		modulesLength,
		&modulesLength
	);

	if (status != STATUS_SUCCESS && status != STATUS_INFO_LENGTH_MISMATCH)
		return NULL;
	if (status == STATUS_SUCCESS || modules->NumberOfModules < 1)
		return NULL;

	return PhConvertUtf8ToUtf16((PCSTR)modules->Modules[0].FullPathName);
}

PPH_STRING KsiGetKernelFileName(VOID)
{
	static PH_INITONCE initOnce = PH_INITONCE_INIT;

	if (PhBeginInitOnce(&initOnce))
	{
		KsiKernelFileName = KsiGetKernelFileNameInternal();

		PhEndInitOnce(&initOnce);
	}

	if (KsiKernelFileName)
		return (PPH_STRING)PhReferenceObject(KsiKernelFileName);

	return NULL;
}

PPH_STRING KsiGetKernelVersionString(VOID)
{
	static PH_INITONCE initOnce = PH_INITONCE_INIT;

	if (PhBeginInitOnce(&initOnce))
	{
		PPH_STRING fileName;
		PH_IMAGE_VERSION_INFO versionInfo;

		if (fileName = KsiGetKernelFileName())
		{
			if (PhInitializeImageVersionInfoEx(&versionInfo, &fileName->sr, FALSE))
			{
				KsiKernelVersion = versionInfo.FileVersion;

				versionInfo.FileVersion = NULL;
				PhDeleteImageVersionInfo(&versionInfo);
			}

			PhDereferenceObject(fileName);
		}

		PhEndInitOnce(&initOnce);
	}

	if (KsiKernelVersion)
		return (PPH_STRING)PhReferenceObject(KsiKernelVersion);

	return NULL;
}



extern "C" {
	VOID NTAPI PhAddDefaultSettings()
	{
	}

	VOID NTAPI PhUpdateCachedSettings()
	{
	}
}


bool IsRunningUnderWow64()
{
	return PhIsExecutingInWow64() ? true : false;
}

quint32 GetWindowsVersion()
{
	return (quint32)WindowsVersion;
}

QString GetNtStatusMessage(quint32 Status)
{
	return CastPhString(PhGetNtMessage((NTSTATUS)Status));
}

QString GetKernelVersionString()
{
	return CastPhString(KsiGetKernelVersionString());
}

int InitNativeApi()
{
	return InitPH();
}

void SetKernelDriverStartup(bool Max, bool High)
{
	g_KphStartupMax = Max ? TRUE : FALSE;
	g_KphStartupHigh = High ? TRUE : FALSE;
}

STATUS LoadKernelDriver(const QString& AppDir)
{
	return InitKSI(AppDir);
}

//
// Asked of the service manager, not of the driver.
//
// KphConnect would answer too, and answering it is the thing that must not
// happen yet: connecting is what loads the driver, and the whole point of this
// question is to know the state *before* anything loads anything.
//
bool IsKernelDriverRunning()
{
	const QString ServiceName = theConf->GetString("OptionsKSI/DeviceName", "KTaskExplorer");

	SC_HANDLE hManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CONNECT);
	if (!hManager)
		return false;

	const std::wstring Name = ServiceName.toStdWString();
	SC_HANDLE hService = OpenServiceW(hManager, Name.c_str(), SERVICE_QUERY_STATUS);
	if (!hService)
	{
		CloseServiceHandle(hManager);
		return false;
	}

	SERVICE_STATUS Status = {};
	const bool bRunning = QueryServiceStatus(hService, &Status)
		&& Status.dwCurrentState == SERVICE_RUNNING;

	CloseServiceHandle(hService);
	CloseServiceHandle(hManager);
	return bRunning;
}

int GetKernelDriverLevel()
{
	//
	// Plain numbers rather than CProcessInfo::EKphLevel, which this file cannot
	// see: it is the phlib side of the wall and including the object model here
	// would be the wrong direction. They are the same numbers - the header says
	// which enum to read them as, and the static_asserts there would be the
	// place to nail it down if the two ever drift.
	//
	switch (KsiLevel())
	{
	case KphLevelMin:	return 1;
	case KphLevelLow:	return 2;
	case KphLevelMed:	return 3;
	case KphLevelHigh:	return 4;
	case KphLevelMax:	return 5;
	default:			return 0;
	}
}

STATUS UnloadKernelDriver()
{
	return CleanupKSI();
}

bool IsUnsupportedKernel(quint32 Status)
{
	return Status == STATUS_SI_DYNDATA_UNSUPPORTED_KERNEL
		|| Status == STATUS_UNKNOWN_REVISION;
}

void EnableServicePrivileges()
{
	HANDLE tokenHandle;
	if (!NT_SUCCESS(PhOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
		return;

	PhSetTokenPrivilege2(tokenHandle, SE_ASSIGNPRIMARYTOKEN_PRIVILEGE, SE_PRIVILEGE_ENABLED);
	PhSetTokenPrivilege2(tokenHandle, SE_INCREASE_QUOTA_PRIVILEGE, SE_PRIVILEGE_ENABLED);
	PhSetTokenPrivilege2(tokenHandle, SE_BACKUP_PRIVILEGE, SE_PRIVILEGE_ENABLED);
	PhSetTokenPrivilege2(tokenHandle, SE_RESTORE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
	PhSetTokenPrivilege2(tokenHandle, SE_IMPERSONATE_PRIVILEGE, SE_PRIVILEGE_ENABLED);

	NtClose(tokenHandle);
}

void SetOwnProcessPriority()
{
	PhSetProcessPriorityClass(NtCurrentProcess(), PROCESS_PRIORITY_CLASS_ABOVE_NORMAL);
	PhSetProcessPagePriority(NtCurrentProcess(), MEMORY_PRIORITY_NORMAL);
	PhSetProcessIoPriority(NtCurrentProcess(), IoPriorityNormal);
}

void SetSystemDpiAware()
{
	typedef DPI_AWARENESS_CONTEXT(WINAPI* P_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT dpiContext);
	P_SetThreadDpiAwarenessContext pSetThreadDpiAwarenessContext =
		(P_SetThreadDpiAwarenessContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");

	if (pSetThreadDpiAwarenessContext) // not present on windows 7
		pSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
	else
		SetProcessDPIAware();
}

//
// The wording for a native status, produced on the machine that produced the
// status - see CStatus. Nothing else can say what an NTSTATUS means.
//
QString FormatNativeStatus(long Status)
{
	return CastPhString(PhGetStatusMessage((NTSTATUS)Status, 0));
}
