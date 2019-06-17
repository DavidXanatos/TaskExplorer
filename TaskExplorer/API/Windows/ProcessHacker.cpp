/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2020 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "ProcessHacker.h"
#include <settings.h>


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
	wstring wstr = qString.toStdWString();
	UNICODE_STRING ustr;
	RtlInitUnicodeString(&ustr, (wchar_t*)wstr.c_str());
	return PhCreateStringFromUnicodeString(&ustr);
}

/*BOOLEAN PhInitializeExceptionPolicy(
	VOID
)
{
#ifndef DEBUG
	ULONG errorMode;

	if (NT_SUCCESS(PhGetProcessErrorMode(NtCurrentProcess(), &errorMode)))
	{
		errorMode &= ~(SEM_NOOPENFILEERRORBOX | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
		PhSetProcessErrorMode(NtCurrentProcess(), errorMode);
	}

	// NOTE: We really shouldn't be using this function since it can be
	// preempted by the Win32 SetUnhandledExceptionFilter function. (dmex)
	RtlSetUnhandledExceptionFilter(PhpUnhandledExceptionCallback);
#endif

	return TRUE;
}
*/

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

/*
BOOLEAN PhInitializeMitigationPolicy(
	VOID
)
{
#ifndef DEBUG
#define DEFAULT_MITIGATION_POLICY_FLAGS \
    (PROCESS_CREATION_MITIGATION_POLICY_HEAP_TERMINATE_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_BOTTOM_UP_ASLR_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_HIGH_ENTROPY_ASLR_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_EXTENSION_POINT_DISABLE_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_PROHIBIT_DYNAMIC_CODE_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_CONTROL_FLOW_GUARD_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_Module_LOAD_NO_REMOTE_ALWAYS_ON | \
     PROCESS_CREATION_MITIGATION_POLICY_Module_LOAD_NO_LOW_LABEL_ALWAYS_ON)

	static PH_STRINGREF nompCommandlinePart = PH_STRINGREF_INIT(L" -nomp");
	static PH_STRINGREF rasCommandlinePart = PH_STRINGREF_INIT(L" -ras");
	BOOLEAN success = TRUE;
	PH_STRINGREF commandlineSr;
	PPH_STRING commandline = NULL;
	PS_SYSTEM_DLL_INIT_BLOCK(*LdrSystemDllInitBlock_I) = NULL;
	STARTUPINFOEX startupInfo = { sizeof(STARTUPINFOEX) };
	SIZE_T attributeListLength;

	if (WindowsVersion < WINDOWS_10_RS3)
		return TRUE;

	PhUnicodeStringToStringRef(&NtCurrentPeb()->ProcessParameters->CommandLine, &commandlineSr);

	// NOTE: The SCM has a bug where calling CreateProcess with PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY to restart the service with mitigations
	// causes the SCM to spew EVENT_SERVICE_DIFFERENT_PID_CONNECTED in the system eventlog and terminate the service. (dmex)
	// WARN: This bug makes it impossible to start services with mitigation polices when the service doesn't have an IFEO key...
	if (PhFindStringInStringRef(&commandlineSr, &rasCommandlinePart, FALSE) != -1)
		return TRUE;
	if (PhEndsWithStringRef(&commandlineSr, &nompCommandlinePart, FALSE))
		return TRUE;

	if (!(LdrSystemDllInitBlock_I = PhGetDllProcedureAddress(L"ntdll.dll", "LdrSystemDllInitBlock", 0)))
		goto CleanupExit;

	if (!RTL_CONTAINS_FIELD(LdrSystemDllInitBlock_I, LdrSystemDllInitBlock_I->Size, MitigationOptionsMap))
		goto CleanupExit;

	if ((LdrSystemDllInitBlock_I->MitigationOptionsMap.Map[0] & DEFAULT_MITIGATION_POLICY_FLAGS) == DEFAULT_MITIGATION_POLICY_FLAGS)
		goto CleanupExit;

	if (!InitializeProcThreadAttributeList(NULL, 1, 0, &attributeListLength) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		goto CleanupExit;

	startupInfo.lpAttributeList = PhAllocate(attributeListLength);

	if (!InitializeProcThreadAttributeList(startupInfo.lpAttributeList, 1, 0, &attributeListLength))
		goto CleanupExit;

	if (!UpdateProcThreadAttribute(startupInfo.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY, &(ULONG64){ DEFAULT_MITIGATION_POLICY_FLAGS }, sizeof(ULONG64), NULL, NULL))
		goto CleanupExit;

	commandline = PhConcatStringRef2(&commandlineSr, &nompCommandlinePart);

	if (NT_SUCCESS(PhCreateProcessWin32Ex(
		NULL,
		PhGetString(commandline),
		NULL,
		NULL,
		&startupInfo.StartupInfo,
		PH_CREATE_PROCESS_EXTENDED_STARTUPINFO | PH_CREATE_PROCESS_BREAKAWAY_FROM_JOB,
		NULL,
		NULL,
		NULL,
		NULL
	)))
	{
		success = FALSE;
	}

CleanupExit:

	if (commandline)
		PhDereferenceObject(commandline);

	if (startupInfo.lpAttributeList)
	{
		DeleteProcThreadAttributeList(startupInfo.lpAttributeList);
		PhFree(startupInfo.lpAttributeList);
	}

	return success;
#else
	return TRUE;
#endif
}*/

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

NTSTATUS PhpReadSignature(
    _In_ PWSTR FileName,
    _Out_ PUCHAR *Signature,
    _Out_ PULONG SignatureSize
    )
{
    NTSTATUS status;
    HANDLE fileHandle;
    PUCHAR signature;
    ULONG bufferSize;
    IO_STATUS_BLOCK iosb;

    if (!NT_SUCCESS(status = PhCreateFileWin32(&fileHandle, FileName, FILE_GENERIC_READ, FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_READ, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
    {
        return status;
    }

    bufferSize = 1024;
    signature = (PUCHAR)PhAllocate(bufferSize);

    status = NtReadFile(fileHandle, NULL, NULL, NULL, &iosb, signature, bufferSize, NULL, NULL);
    NtClose(fileHandle);

    if (NT_SUCCESS(status))
    {
        *Signature = signature;
        *SignatureSize = (ULONG)iosb.Information;
        return status;
    }
    else
    {
        PhFree(signature);
        return status;
    }
}

VOID PhInitializeKph(
    VOID
    )
{
    NTSTATUS status;
    PPH_STRING applicationDirectory;
    PPH_STRING kprocesshackerFileName;
    PPH_STRING processhackerSigFileName;
    KPH_PARAMETERS parameters;

    if (!(applicationDirectory = PhGetApplicationDirectory()))
        return;

    kprocesshackerFileName = PhConcatStringRefZ(&applicationDirectory->sr, L"kprocesshacker.sys");
    processhackerSigFileName = PhConcatStringRefZ(&applicationDirectory->sr, L"ProcessHacker.sig");
    PhDereferenceObject(applicationDirectory);

    if (!RtlDoesFileExists_U(kprocesshackerFileName->Buffer))
    {
		qDebug() << "The Process Hacker kernel driver 'kprocesshacker.sys' was not found in the application directory.";
        // return;
    }

    parameters.SecurityLevel = KphSecurityPrivilegeCheck;
    parameters.CreateDynamicConfiguration = TRUE;

    if (NT_SUCCESS(status = KphConnect2Ex(
        KPH_DEVICE_SHORT_NAME,
        kprocesshackerFileName->Buffer,
        &parameters
        )))
    {
		qDebug() << "Process Hacker kernel driver connected.";

#if 0
		PUCHAR signature;
        ULONG signatureSize;

        status = PhpReadSignature(
            processhackerSigFileName->Buffer, 
            &signature, 
            &signatureSize
            );

        if (NT_SUCCESS(status))
        {
            status = KphVerifyClient(signature, signatureSize);

            if (!NT_SUCCESS(status))
            {
				qDebug() << "Unable to verify the kernel driver signature.";
            }

            PhFree(signature);
        }
        else
        {
			qDebug() << "Unable to load the kernel driver signature.";
        }
#endif
    }
    else
    {
		qDebug() << "Unable to load the kernel driver.";
    }

    PhDereferenceObject(kprocesshackerFileName);
    PhDereferenceObject(processhackerSigFileName);
}

int InitPH()
{
	HINSTANCE Instance = NULL; // todo?
	LONG result;
#ifdef DEBUG
	PHP_BASE_THREAD_DBG dbg;
#endif

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	if (!NT_SUCCESS(PhInitializePhLibEx(L"Task Explorer", ULONG_MAX, Instance, 0, 0)))
		return 1;
	//if (!PhInitializeExceptionPolicy())
	//	return 1;
	if (!PhInitializeNamespacePolicy())
		return 1;
	//if (!PhInitializeMitigationPolicy())
	//	return 1;
	//if (!PhInitializeRestartPolicy())
	//    return 1;

	//PhpProcessStartupParameters();
	PhpEnablePrivileges();

	/*if (PhStartupParameters.RunAsServiceMode)
    {
        RtlExitUserProcess(PhRunAsServiceStart(PhStartupParameters.RunAsServiceMode));
    }*/

    /*if (PhStartupParameters.CommandMode &&
        PhStartupParameters.CommandType &&
        PhStartupParameters.CommandAction)
    {
        RtlExitUserProcess(PhCommandModeStart());
    }*/

	PhSettingsInitialization();
    //PhpInitializeSettings();
	// note: this is needed to open the permissions panel
	PhpAddIntegerSetting(L"EnableSecurityAdvancedDialog", L"1");

    /*if (PhGetIntegerSetting(L"AllowOnlyOneInstance") &&
        !PhStartupParameters.NewInstance &&
        !PhStartupParameters.ShowOptions &&
        !PhStartupParameters.CommandMode &&
        !PhStartupParameters.PhSvc)
    {
        PhActivatePreviousInstance();
    }*/

    if (/*PhGetIntegerSetting(L"EnableKph") &&
        !PhStartupParameters.NoKph &&
        !PhStartupParameters.CommandMode &&*/
        !PhIsExecutingInWow64()
        )
    {
        PhInitializeKph();
    }

	return 0;
}

void ClearPH()
{
	
}

extern "C" {
	VOID PhAddDefaultSettings()
	{
	}

	VOID PhUpdateCachedSettings()
	{
	}
}

// procpriv.c


// appsup.c
PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(
	_In_ HANDLE ProcessId,
	_In_ PPH_STRING FileName
)
{
	int knownProcessType;
	PH_STRINGREF systemRootPrefix;
	PPH_STRING fileName;
	PH_STRINGREF name;
#ifdef _WIN64
	BOOLEAN isWow64 = FALSE;
#endif

	if (ProcessId == SYSTEM_PROCESS_ID || ProcessId == SYSTEM_IDLE_PROCESS_ID)
		return SystemProcessType;

	if (PhIsNullOrEmptyString(FileName))
		return UnknownProcessType;

	PhGetSystemRoot(&systemRootPrefix);

	fileName = (PPH_STRING)PhReferenceObject(FileName);
	name = fileName->sr;

	knownProcessType = UnknownProcessType;

	if (PhStartsWithStringRef(&name, &systemRootPrefix, TRUE))
	{
		// Skip the system root, and we now have three cases:
		// 1. \\xyz.exe - Windows executable.
		// 2. \\System32\\xyz.exe - system32 executable.
		// 3. \\SysWow64\\xyz.exe - system32 executable + WOW64.
		PhSkipStringRef(&name, systemRootPrefix.Length);

		if (PhEqualStringRef2(&name, L"\\explorer.exe", TRUE))
		{
			knownProcessType = ExplorerProcessType;
		}
		else if (
			PhStartsWithStringRef2(&name, L"\\System32", TRUE)
#ifdef _WIN64
			|| (PhStartsWithStringRef2(&name, L"\\SysWow64", TRUE) && (isWow64 = TRUE, TRUE)) // ugly but necessary
#endif
			)
		{
			// SysTem32 and SysWow64 are both 8 characters long.
			PhSkipStringRef(&name, 9 * sizeof(WCHAR));

			if (FALSE)
				; // Dummy
			else if (PhEqualStringRef2(&name, L"\\smss.exe", TRUE))
				knownProcessType = SessionManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\csrss.exe", TRUE))
				knownProcessType = WindowsSubsystemProcessType;
			else if (PhEqualStringRef2(&name, L"\\wininit.exe", TRUE))
				knownProcessType = WindowsStartupProcessType;
			else if (PhEqualStringRef2(&name, L"\\services.exe", TRUE))
				knownProcessType = ServiceControlManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\lsass.exe", TRUE))
				knownProcessType = LocalSecurityAuthorityProcessType;
			else if (PhEqualStringRef2(&name, L"\\lsm.exe", TRUE))
				knownProcessType = LocalSessionManagerProcessType;
			else if (PhEqualStringRef2(&name, L"\\winlogon.exe", TRUE))
				knownProcessType = WindowsLogonProcessType;
			else if (PhEqualStringRef2(&name, L"\\svchost.exe", TRUE))
				knownProcessType = ServiceHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\rundll32.exe", TRUE))
				knownProcessType = RunDllAsAppProcessType;
			else if (PhEqualStringRef2(&name, L"\\dllhost.exe", TRUE))
				knownProcessType = ComSurrogateProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskeng.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhost.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhostex.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\taskhostw.exe", TRUE))
				knownProcessType = TaskHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\wudfhost.exe", TRUE))
				knownProcessType = UmdfHostProcessType;
			else if (PhEqualStringRef2(&name, L"\\wbem\\WmiPrvSE.exe", TRUE))
				knownProcessType = WmiProviderHostType;
			else if (PhEqualStringRef2(&name, L"\\MicrosoftEdgeCP.exe", TRUE)) // RS5
				knownProcessType = EdgeProcessType;
			else if (PhEqualStringRef2(&name, L"\\MicrosoftEdgeSH.exe", TRUE)) // RS5
				knownProcessType = EdgeProcessType;
		}
		else
		{
			if (PhEndsWithStringRef2(&name, L"\\MicrosoftEdgeCP.exe", TRUE)) // RS4
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\MicrosoftEdge.exe", TRUE))
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\ServiceWorkerHost.exe", TRUE))
				knownProcessType = EdgeProcessType;
			else if (PhEndsWithStringRef2(&name, L"\\Windows.WARP.JITService.exe", TRUE))
				knownProcessType = EdgeProcessType;
		}
	}

	PhDereferenceObject(fileName);

#ifdef _WIN64
	if (isWow64)
		knownProcessType |= KnownProcessWow64;
#endif

	return (PH_KNOWN_PROCESS_TYPE)knownProcessType;
}

// appsup.c
GUID XP_CONTEXT_GUID = { 0xbeb1b341, 0x6837, 0x4c83, { 0x83, 0x66, 0x2b, 0x45, 0x1e, 0x7c, 0xe6, 0x9b } };
GUID VISTA_CONTEXT_GUID = { 0xe2011457, 0x1546, 0x43c5, { 0xa5, 0xfe, 0x00, 0x8d, 0xee, 0xe3, 0xd3, 0xf0 } };
GUID WIN7_CONTEXT_GUID = { 0x35138b9a, 0x5d96, 0x4fbd, { 0x8e, 0x2d, 0xa2, 0x44, 0x02, 0x25, 0xf9, 0x3a } };
GUID WIN8_CONTEXT_GUID = { 0x4a2f28e3, 0x53b9, 0x4441, { 0xba, 0x9c, 0xd6, 0x9d, 0x4a, 0x4a, 0x6e, 0x38 } };
GUID WINBLUE_CONTEXT_GUID = { 0x1f676c76, 0x80e1, 0x4239, { 0x95, 0xbb, 0x83, 0xd0, 0xf6, 0xd0, 0xda, 0x78 } };
GUID WIN10_CONTEXT_GUID = { 0x8e0f7a12, 0xbfb3, 0x4fe8, { 0xb9, 0xa5, 0x48, 0xfd, 0x50, 0xa1, 0x5a, 0x9a } };

/**
 * Determines the OS compatibility context of a process.
 *
 * \param ProcessHandle A handle to a process.
 * \param Guid A variable which receives a GUID identifying an
 * operating system version.
 */
NTSTATUS PhGetProcessSwitchContext(
    _In_ HANDLE ProcessHandle,
    _Out_ PGUID Guid
    )
{
    NTSTATUS status;
    PROCESS_BASIC_INFORMATION basicInfo;
#ifdef _WIN64
    PVOID peb32;
    ULONG data32;
#endif
    PVOID data;

    // Reverse-engineered from WdcGetProcessSwitchContext (wdc.dll).
    // On Windows 8, the function is now SdbGetAppCompatData (apphelp.dll).
    // On Windows 10, the function is again WdcGetProcessSwitchContext.

#ifdef _WIN64
    if (NT_SUCCESS(PhGetProcessPeb32(ProcessHandle, &peb32)) && peb32)
    {
        if (WindowsVersion >= WINDOWS_8)
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, pShimData)),
                &data32,
                sizeof(ULONG),
                NULL
                )))
                return status;
        }
        else
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, pContextData)),
                &data32,
                sizeof(ULONG),
                NULL
                )))
                return status;
        }

        data = UlongToPtr(data32);
    }
    else
    {
#endif
        if (!NT_SUCCESS(status = PhGetProcessBasicInformation(ProcessHandle, &basicInfo)))
            return status;

        if (WindowsVersion >= WINDOWS_8)
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, pShimData)),
                &data,
                sizeof(PVOID),
                NULL
                )))
                return status;
        }
        else
        {
            if (!NT_SUCCESS(status = NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, pUnused)),
                &data,
                sizeof(PVOID),
                NULL
                )))
                return status;
        }
#ifdef _WIN64
    }
#endif

    if (!data)
        return STATUS_UNSUCCESSFUL; // no compatibility context data

    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 24), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_10_RS2)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 1544),
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_10)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 24), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_8_1)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040 + 16), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else if (WindowsVersion >= WINDOWS_8)
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 2040), // Magic value from SbReadProcContextByHandle
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }
    else
    {
        if (!NT_SUCCESS(status = NtReadVirtualMemory(
            ProcessHandle,
            PTR_ADD_OFFSET(data, 32), // Magic value from WdcGetProcessSwitchContext
            Guid,
            sizeof(GUID),
            NULL
            )))
            return status;
    }

    return STATUS_SUCCESS;
}



//netprv.c
BOOLEAN PhGetNetworkConnections(
    _Out_ PPH_NETWORK_CONNECTION *Connections,
    _Out_ PULONG NumberOfConnections
    )
{
    PVOID table;
    ULONG tableSize;
    PMIB_TCPTABLE_OWNER_MODULE tcp4Table;
    PMIB_UDPTABLE_OWNER_MODULE udp4Table;
    PMIB_TCP6TABLE_OWNER_MODULE tcp6Table;
    PMIB_UDP6TABLE_OWNER_MODULE udp6Table;
    ULONG count = 0;
    ULONG i;
    ULONG index = 0;
    PPH_NETWORK_CONNECTION connections;

    // TCP IPv4

    tableSize = 0;
    GetExtendedTcpTable(NULL, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0);
    table = PhAllocate(tableSize);

    if (GetExtendedTcpTable(table, &tableSize, FALSE, AF_INET, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
    {
        tcp4Table = (PMIB_TCPTABLE_OWNER_MODULE)table;
        count += tcp4Table->dwNumEntries;
    }
    else
    {
        PhFree(table);
        tcp4Table = NULL;
    }

    // TCP IPv6

    tableSize = 0;
    GetExtendedTcpTable(NULL, &tableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0);

    table = PhAllocate(tableSize);

    if (GetExtendedTcpTable(table, &tableSize, FALSE, AF_INET6, TCP_TABLE_OWNER_MODULE_ALL, 0) == NO_ERROR)
    {
        tcp6Table = (PMIB_TCP6TABLE_OWNER_MODULE)table;
        count += tcp6Table->dwNumEntries;
    }
    else
    {
        PhFree(table);
        tcp6Table = NULL;
    }

    // UDP IPv4

    tableSize = 0;
    GetExtendedUdpTable(NULL, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0);
    table = PhAllocate(tableSize);

    if (GetExtendedUdpTable(table, &tableSize, FALSE, AF_INET, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
    {
        udp4Table = (PMIB_UDPTABLE_OWNER_MODULE)table;
        count += udp4Table->dwNumEntries;
    }
    else
    {
        PhFree(table);
        udp4Table = NULL;
    }

    // UDP IPv6

    tableSize = 0;
    GetExtendedUdpTable(NULL, &tableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0);
    table = PhAllocate(tableSize);

    if (GetExtendedUdpTable(table, &tableSize, FALSE, AF_INET6, UDP_TABLE_OWNER_MODULE, 0) == NO_ERROR)
    {
        udp6Table = (PMIB_UDP6TABLE_OWNER_MODULE)table;
        count += udp6Table->dwNumEntries;
    }
    else
    {
        PhFree(table);
        udp6Table = NULL;
    }

    connections = (PPH_NETWORK_CONNECTION)PhAllocate(sizeof(PH_NETWORK_CONNECTION) * count);
    memset(connections, 0, sizeof(PH_NETWORK_CONNECTION) * count);

    if (tcp4Table)
    {
        for (i = 0; i < tcp4Table->dwNumEntries; i++)
        {
            connections[index].ProtocolType = PH_TCP4_NETWORK_PROTOCOL;

            connections[index].LocalEndpoint.Address.Type = PH_IPV4_NETWORK_TYPE;
            connections[index].LocalEndpoint.Address.Ipv4 = tcp4Table->table[i].dwLocalAddr;
            connections[index].LocalEndpoint.Port = _byteswap_ushort((USHORT)tcp4Table->table[i].dwLocalPort);

            connections[index].RemoteEndpoint.Address.Type = PH_IPV4_NETWORK_TYPE;
            connections[index].RemoteEndpoint.Address.Ipv4 = tcp4Table->table[i].dwRemoteAddr;
            connections[index].RemoteEndpoint.Port = _byteswap_ushort((USHORT)tcp4Table->table[i].dwRemotePort);

            connections[index].State = tcp4Table->table[i].dwState;
            connections[index].ProcessId = UlongToHandle(tcp4Table->table[i].dwOwningPid);
            connections[index].CreateTime = tcp4Table->table[i].liCreateTimestamp;
            memcpy(
                connections[index].OwnerInfo,
                tcp4Table->table[i].OwningModuleInfo,
                sizeof(ULONGLONG) * min(PH_NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE)
                );

            index++;
        }

        PhFree(tcp4Table);
    }

    if (tcp6Table)
    {
        for (i = 0; i < tcp6Table->dwNumEntries; i++)
        {
            connections[index].ProtocolType = PH_TCP6_NETWORK_PROTOCOL;

            connections[index].LocalEndpoint.Address.Type = PH_IPV6_NETWORK_TYPE;
            memcpy(connections[index].LocalEndpoint.Address.Ipv6, tcp6Table->table[i].ucLocalAddr, 16);
            connections[index].LocalEndpoint.Port = _byteswap_ushort((USHORT)tcp6Table->table[i].dwLocalPort);

            connections[index].RemoteEndpoint.Address.Type = PH_IPV6_NETWORK_TYPE;
            memcpy(connections[index].RemoteEndpoint.Address.Ipv6, tcp6Table->table[i].ucRemoteAddr, 16);
            connections[index].RemoteEndpoint.Port = _byteswap_ushort((USHORT)tcp6Table->table[i].dwRemotePort);

            connections[index].State = tcp6Table->table[i].dwState;
            connections[index].ProcessId = UlongToHandle(tcp6Table->table[i].dwOwningPid);
            connections[index].CreateTime = tcp6Table->table[i].liCreateTimestamp;
            memcpy(
                connections[index].OwnerInfo,
                tcp6Table->table[i].OwningModuleInfo,
                sizeof(ULONGLONG) * min(PH_NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE)
                );

            connections[index].LocalScopeId = tcp6Table->table[i].dwLocalScopeId;
            connections[index].RemoteScopeId = tcp6Table->table[i].dwRemoteScopeId;

            index++;
        }

        PhFree(tcp6Table);
    }

    if (udp4Table)
    {
        for (i = 0; i < udp4Table->dwNumEntries; i++)
        {
            connections[index].ProtocolType = PH_UDP4_NETWORK_PROTOCOL;

            connections[index].LocalEndpoint.Address.Type = PH_IPV4_NETWORK_TYPE;
            connections[index].LocalEndpoint.Address.Ipv4 = udp4Table->table[i].dwLocalAddr;
            connections[index].LocalEndpoint.Port = _byteswap_ushort((USHORT)udp4Table->table[i].dwLocalPort);

            connections[index].RemoteEndpoint.Address.Type = 0;

            connections[index].State = 0;
            connections[index].ProcessId = UlongToHandle(udp4Table->table[i].dwOwningPid);
            connections[index].CreateTime = udp4Table->table[i].liCreateTimestamp;
            memcpy(
                connections[index].OwnerInfo,
                udp4Table->table[i].OwningModuleInfo,
                sizeof(ULONGLONG) * min(PH_NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE)
                );

            index++;
        }

        PhFree(udp4Table);
    }

    if (udp6Table)
    {
        for (i = 0; i < udp6Table->dwNumEntries; i++)
        {
            connections[index].ProtocolType = PH_UDP6_NETWORK_PROTOCOL;

            connections[index].LocalEndpoint.Address.Type = PH_IPV6_NETWORK_TYPE;
            memcpy(connections[index].LocalEndpoint.Address.Ipv6, udp6Table->table[i].ucLocalAddr, 16);
            connections[index].LocalEndpoint.Port = _byteswap_ushort((USHORT)udp6Table->table[i].dwLocalPort);

            connections[index].RemoteEndpoint.Address.Type = 0;

            connections[index].State = 0;
            connections[index].ProcessId = UlongToHandle(udp6Table->table[i].dwOwningPid);
            connections[index].CreateTime = udp6Table->table[i].liCreateTimestamp;
            memcpy(
                connections[index].OwnerInfo,
                udp6Table->table[i].OwningModuleInfo,
                sizeof(ULONGLONG) * min(PH_NETWORK_OWNER_INFO_SIZE, TCPIP_OWNING_MODULE_SIZE)
                );

            index++;
        }

        PhFree(udp6Table);
    }

    *NumberOfConnections = count;
    *Connections = connections;

    return TRUE;
}

VOID PhpWorkaroundWindows10ServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServieEntry)
{
    // https://github.com/processhacker2/processhacker/issues/120 (dmex)
    if (ServieEntry->ServiceStatusProcess.dwServiceType == SERVICE_WIN32)
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_WIN32_SHARE_PROCESS;
    if (ServieEntry->ServiceStatusProcess.dwServiceType == (SERVICE_WIN32 | SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE))
        ServieEntry->ServiceStatusProcess.dwServiceType = SERVICE_USER_SHARE_PROCESS | SERVICE_USERSERVICE_INSTANCE;
}

VOID WeInvertWindowBorder(_In_ HWND hWnd)
{
    RECT rect;
    HDC hdc;

    GetWindowRect(hWnd, &rect);
    hdc = GetWindowDC(hWnd);

    if (hdc)
    {
        ULONG penWidth = GetSystemMetrics(SM_CXBORDER) * 3;
        INT oldDc;
        HPEN pen;
        HBRUSH brush;

        oldDc = SaveDC(hdc);

        // Get an inversion effect.
        SetROP2(hdc, R2_NOT);

        pen = CreatePen(PS_INSIDEFRAME, penWidth, RGB(0x00, 0x00, 0x00));
        SelectObject(hdc, pen);

        brush = (HBRUSH)GetStockObject(NULL_BRUSH);
        SelectObject(hdc, brush);

        // Draw the rectangle.
        Rectangle(hdc, 0, 0, rect.right - rect.left, rect.bottom - rect.top);

        // Cleanup.
        DeleteObject(pen);

        RestoreDC(hdc, oldDc);
        ReleaseDC(hWnd, hdc);
    }
}
// procmtgn.c
NTSTATUS PhpCopyProcessMitigationPolicy(
    _Inout_ PNTSTATUS Status,
    _In_ HANDLE ProcessHandle,
    _In_ PROCESS_MITIGATION_POLICY Policy,
    _In_ SIZE_T Offset,
    _In_ SIZE_T Size,
    _Out_writes_bytes_(Size) PVOID Destination
    )
{
    NTSTATUS status;
    PROCESS_MITIGATION_POLICY_INFORMATION policyInfo;

    policyInfo.Policy = Policy;
    status = NtQueryInformationProcess(
        ProcessHandle,
        ProcessMitigationPolicy,
        &policyInfo,
        sizeof(PROCESS_MITIGATION_POLICY_INFORMATION),
        NULL
        );

    if (!NT_SUCCESS(status))
    {
        if (*Status == STATUS_NONE_MAPPED)
            *Status = status;
        return status;
    }

    memcpy(Destination, PTR_ADD_OFFSET(&policyInfo, Offset), Size);
    *Status = STATUS_SUCCESS;

    return status;
}

// procmtgn.c
NTSTATUS PhGetProcessMitigationPolicy(
    _In_ HANDLE ProcessHandle,
    _Out_ PPH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION Information
    )
{
    NTSTATUS status = STATUS_NONE_MAPPED;
    NTSTATUS subStatus;
#ifdef _WIN64
    BOOLEAN isWow64;
#endif
    ULONG depStatus;

    memset(Information, 0, sizeof(PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION));

#ifdef _WIN64
    if (NT_SUCCESS(subStatus = PhGetProcessIsWow64(ProcessHandle, &isWow64)) && !isWow64)
    {
        depStatus = PH_PROCESS_DEP_ENABLED | PH_PROCESS_DEP_PERMANENT;
    }
    else
    {
#endif
        subStatus = PhGetProcessDepStatus(ProcessHandle, &depStatus);
#ifdef _WIN64
    }
#endif

    if (NT_SUCCESS(subStatus))
    {
        status = STATUS_SUCCESS;
        Information->DEPPolicy.Enable = !!(depStatus & PH_PROCESS_DEP_ENABLED);
        Information->DEPPolicy.DisableAtlThunkEmulation = !!(depStatus & PH_PROCESS_DEP_ATL_THUNK_EMULATION_DISABLED);
        Information->DEPPolicy.Permanent = !!(depStatus & PH_PROCESS_DEP_PERMANENT);
        Information->Pointers[ProcessDEPPolicy] = &Information->DEPPolicy;
    }
    else if (status == STATUS_NONE_MAPPED)
    {
        status = subStatus;
    }

#define COPY_PROCESS_MITIGATION_POLICY(PolicyName, StructName) \
    if (NT_SUCCESS(PhpCopyProcessMitigationPolicy(&status, ProcessHandle, Process##PolicyName##Policy, \
        UFIELD_OFFSET(PROCESS_MITIGATION_POLICY_INFORMATION, PolicyName##Policy), \
        sizeof(StructName), \
        &Information->PolicyName##Policy))) \
    { \
        Information->Pointers[Process##PolicyName##Policy] = &Information->PolicyName##Policy; \
    }

    COPY_PROCESS_MITIGATION_POLICY(ASLR, PROCESS_MITIGATION_ASLR_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(DynamicCode, PROCESS_MITIGATION_DYNAMIC_CODE_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(StrictHandleCheck, PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(SystemCallDisable, PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(ExtensionPointDisable, PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(ControlFlowGuard, PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(Signature, PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(FontDisable, PROCESS_MITIGATION_FONT_DISABLE_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(ImageLoad, PROCESS_MITIGATION_IMAGE_LOAD_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(SystemCallFilter, PROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY); // REDSTONE3
    COPY_PROCESS_MITIGATION_POLICY(PayloadRestriction, PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(ChildProcess, PROCESS_MITIGATION_CHILD_PROCESS_POLICY);
    COPY_PROCESS_MITIGATION_POLICY(SideChannelIsolation, PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY);

    return status;
}

// procmtgn.c
BOOLEAN PhDescribeProcessMitigationPolicy(
    _In_ PROCESS_MITIGATION_POLICY Policy,
    _In_ PVOID Data,
    _Out_opt_ PPH_STRING *ShortDescription,
    _Out_opt_ PPH_STRING *LongDescription
    )
{
    BOOLEAN result = FALSE;
    PH_STRING_BUILDER sb;

    switch (Policy)
    {
    case ProcessDEPPolicy:
        {
            PPROCESS_MITIGATION_DEP_POLICY data = (PPROCESS_MITIGATION_DEP_POLICY)Data;

            if (data->Enable)
            {
                if (ShortDescription)
                {
                    PhInitializeStringBuilder(&sb, 20);
                    PhAppendStringBuilder2(&sb, L"DEP");
                    if (data->Permanent) PhAppendStringBuilder2(&sb, L" (permanent)");
                    *ShortDescription = PhFinalStringBuilderString(&sb);
                }

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 50);
                    PhAppendFormatStringBuilder(&sb, L"Data Execution Prevention (DEP) is%s enabled for this process.\r\n", data->Permanent ? L" permanently" : L"");
                    if (data->DisableAtlThunkEmulation) PhAppendStringBuilder2(&sb, L"ATL thunk emulation is disabled.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessASLRPolicy:
        {
            PPROCESS_MITIGATION_ASLR_POLICY data = (PPROCESS_MITIGATION_ASLR_POLICY)Data;

            if (data->EnableBottomUpRandomization || data->EnableForceRelocateImages || data->EnableHighEntropy)
            {
                if (ShortDescription)
                {
                    PhInitializeStringBuilder(&sb, 20);
                    PhAppendStringBuilder2(&sb, L"ASLR");

                    if (data->EnableHighEntropy || data->EnableForceRelocateImages)
                    {
                        PhAppendStringBuilder2(&sb, L" (");
                        if (data->EnableHighEntropy) PhAppendStringBuilder2(&sb, L"high entropy, ");
                        if (data->EnableForceRelocateImages) PhAppendStringBuilder2(&sb, L"force relocate, ");
                        if (data->DisallowStrippedImages) PhAppendStringBuilder2(&sb, L"disallow stripped, ");
                        if (PhEndsWithStringRef2(&sb.String->sr, L", ", FALSE)) PhRemoveEndStringBuilder(&sb, 2);
                        PhAppendCharStringBuilder(&sb, ')');
                    }

                    *ShortDescription = PhFinalStringBuilderString(&sb);
                }

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 100);
                    PhAppendStringBuilder2(&sb, L"Address Space Layout Randomization is enabled for this process.\r\n");
                    if (data->EnableHighEntropy) PhAppendStringBuilder2(&sb, L"High entropy randomization is enabled.\r\n");
                    if (data->EnableForceRelocateImages) PhAppendStringBuilder2(&sb, L"All images are being forcibly relocated (regardless of whether they support ASLR).\r\n");
                    if (data->DisallowStrippedImages) PhAppendStringBuilder2(&sb, L"Images with stripped relocation data are disallowed.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessDynamicCodePolicy:
        {
            PPROCESS_MITIGATION_DYNAMIC_CODE_POLICY data = (PPROCESS_MITIGATION_DYNAMIC_CODE_POLICY)Data;

            if (data->ProhibitDynamicCode)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Dynamic code prohibited");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Dynamically loaded code is not allowed to execute.\r\n");

                result = TRUE;
            }

            if (data->AllowThreadOptOut)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Dynamic code prohibited (per-thread)");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Allows individual threads to opt out of the restrictions on dynamic code generation.\r\n");

                result = TRUE;
            }

            if (data->AllowRemoteDowngrade)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Dynamic code downgradable");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Allow non-AppContainer processes to modify all of the dynamic code settings for the calling process, including relaxing dynamic code restrictions after they have been set.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessStrictHandleCheckPolicy:
        {
            PPROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY data = (PPROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY)Data;

            if (data->RaiseExceptionOnInvalidHandleReference)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Strict handle checks");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"An exception is raised when an invalid handle is used by the process.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessSystemCallDisablePolicy:
        {
            PPROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY data = (PPROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY)Data;

            if (data->DisallowWin32kSystemCalls)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Win32k system calls disabled");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Win32k (GDI/USER) system calls are not allowed.\r\n");

                result = TRUE;
            }

            if (data->AuditDisallowWin32kSystemCalls)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Win32k system calls (Audit)");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Win32k (GDI/USER) system calls will trigger an ETW event.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessExtensionPointDisablePolicy:
        {
            PPROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY data = (PPROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY)Data;

            if (data->DisableExtensionPoints)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Extension points disabled");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Legacy extension point DLLs cannot be loaded into the process.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessControlFlowGuardPolicy:
        {
            PPROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY data = (PPROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY)Data;

            if (data->EnableControlFlowGuard)
            {
                if (ShortDescription)
                {
                    PhInitializeStringBuilder(&sb, 50);
                    if (data->StrictMode) PhAppendStringBuilder2(&sb, L"Strict ");
                    PhAppendStringBuilder2(&sb, L"CF Guard");
                    *ShortDescription = PhFinalStringBuilderString(&sb);
                }

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 100);
                    PhAppendStringBuilder2(&sb, L"Control Flow Guard (CFG) is enabled for the process.\r\n");
                    if (data->StrictMode) PhAppendStringBuilder2(&sb, L"Strict CFG : only CFG modules can be loaded.\r\n");
                    if (data->EnableExportSuppression) PhAppendStringBuilder2(&sb, L"Dll Exports can be marked as CFG invalid targets.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessSignaturePolicy:
        {
            PPROCESS_MITIGATION_BINARY_SIGNATURE_POLICY data = (PPROCESS_MITIGATION_BINARY_SIGNATURE_POLICY)Data;

            if (data->MicrosoftSignedOnly || data->StoreSignedOnly)
            {
                if (ShortDescription)
                {
                    PhInitializeStringBuilder(&sb, 50);
                    PhAppendStringBuilder2(&sb, L"Signatures restricted (");
                    if (data->MicrosoftSignedOnly) PhAppendStringBuilder2(&sb, L"Microsoft only, ");
                    if (data->StoreSignedOnly) PhAppendStringBuilder2(&sb, L"Store only, ");
                    if (PhEndsWithStringRef2(&sb.String->sr, L", ", FALSE)) PhRemoveEndStringBuilder(&sb, 2);
                    PhAppendCharStringBuilder(&sb, ')');

                    *ShortDescription = PhFinalStringBuilderString(&sb);
                }

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 100);
                    PhAppendStringBuilder2(&sb, L"Image signature restrictions are enabled for this process.\r\n");
                    if (data->MicrosoftSignedOnly) PhAppendStringBuilder2(&sb, L"Only Microsoft signatures are allowed.\r\n");
                    if (data->StoreSignedOnly) PhAppendStringBuilder2(&sb, L"Only Windows Store signatures are allowed.\r\n");
                    if (data->MitigationOptIn) PhAppendStringBuilder2(&sb, L"This is an opt-in restriction.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessFontDisablePolicy:
        {
            PPROCESS_MITIGATION_FONT_DISABLE_POLICY data = (PPROCESS_MITIGATION_FONT_DISABLE_POLICY)Data;

            if (data->DisableNonSystemFonts)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Non-system fonts disabled");

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 100);
                    PhAppendStringBuilder2(&sb, L"Non-system fonts cannot be used in this process.\r\n");
                    if (data->AuditNonSystemFontLoading) PhAppendStringBuilder2(&sb, L"Loading a non-system font in this process will trigger an ETW event.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessImageLoadPolicy:
        {
            PPROCESS_MITIGATION_IMAGE_LOAD_POLICY data = (PPROCESS_MITIGATION_IMAGE_LOAD_POLICY)Data;

            if (data->NoRemoteImages || data->NoLowMandatoryLabelImages)
            {
                if (ShortDescription)
                {
                    PhInitializeStringBuilder(&sb, 50);
                    PhAppendStringBuilder2(&sb, L"Images restricted (");
                    if (data->NoRemoteImages) PhAppendStringBuilder2(&sb, L"remote images, ");
                    if (data->NoLowMandatoryLabelImages) PhAppendStringBuilder2(&sb, L"low mandatory label images, ");
                    if (PhEndsWithStringRef2(&sb.String->sr, L", ", FALSE)) PhRemoveEndStringBuilder(&sb, 2);
                    PhAppendCharStringBuilder(&sb, ')');

                    *ShortDescription = PhFinalStringBuilderString(&sb);
                }

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 50);
                    if (data->NoRemoteImages) PhAppendStringBuilder2(&sb, L"Remotely located images cannot be loaded into the process.\r\n");
                    if (data->NoLowMandatoryLabelImages) PhAppendStringBuilder2(&sb, L"Images with a Low mandatory label cannot be loaded into the process.\r\n");

                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }

            if (data->PreferSystem32Images)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Prefer system32 images");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Forces images to load from the System32 folder in which Windows is installed first, then from the application directory before the standard DLL search order.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessSystemCallFilterPolicy:
        {
            PPROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY data = (PPROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY)Data;
            
            if (data->FilterId)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"System call filtering");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"System call filtering is active.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessPayloadRestrictionPolicy:
        {
            PPROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY data = (PPROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY)Data;

            if (data->EnableExportAddressFilter || data->EnableExportAddressFilterPlus ||
                data->EnableImportAddressFilter || data->EnableRopStackPivot ||
                data->EnableRopCallerCheck || data->EnableRopSimExec)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Payload restrictions");

                if (LongDescription)
                {
                    PhInitializeStringBuilder(&sb, 100);
                    PhAppendStringBuilder2(&sb, L"Payload restrictions are enabled for this process.\r\n");
                    if (data->EnableExportAddressFilter) PhAppendStringBuilder2(&sb, L"Export Address Filtering is enabled.\r\n");
                    if (data->EnableExportAddressFilterPlus) PhAppendStringBuilder2(&sb, L"Export Address Filtering (Plus) is enabled.\r\n");
                    if (data->EnableImportAddressFilter) PhAppendStringBuilder2(&sb, L"Import Address Filtering is enabled.\r\n");
                    if (data->EnableRopStackPivot) PhAppendStringBuilder2(&sb, L"StackPivot is enabled.\r\n");
                    if (data->EnableRopCallerCheck) PhAppendStringBuilder2(&sb, L"CallerCheck is enabled.\r\n");
                    if (data->EnableRopSimExec) PhAppendStringBuilder2(&sb, L"SimExec is enabled.\r\n");
                    *LongDescription = PhFinalStringBuilderString(&sb);
                }

                result = TRUE;
            }
        }
        break;
    case ProcessChildProcessPolicy:
        {
            PPROCESS_MITIGATION_CHILD_PROCESS_POLICY data = (PPROCESS_MITIGATION_CHILD_PROCESS_POLICY)Data;

            if (data->NoChildProcessCreation)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Child process creation disabled");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Child processes cannot be created by this process.\r\n");

                result = TRUE;
            }
        }
        break;
    case ProcessSideChannelIsolationPolicy:
        {
            PPROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY data = (PPROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY)Data;

            if (data->SmtBranchTargetIsolation)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"SMT-thread branch target isolation");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Branch target pollution cross-SMT-thread in user mode is enabled.\r\n");

                result = TRUE;
            }

            if (data->IsolateSecurityDomain)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Distinct security domain");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Isolated security domain is enabled.\r\n");

                result = TRUE;
            }

            if (data->DisablePageCombine)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Restricted page combining");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Disables all page combining for this process.\r\n");

                result = TRUE;
            }

            if (data->SpeculativeStoreBypassDisable)
            {
                if (ShortDescription)
                    *ShortDescription = PhCreateString(L"Restricted page combining");

                if (LongDescription)
                    *LongDescription = PhCreateString(L"Memory Disambiguation is enabled for this process.\r\n");

                result = TRUE;
            }
        }
        break;
    default:
        result = FALSE;
    }

    return result;
}


void PhShowAbout(QWidget* parent)
{
		QString AboutCaption = QString(
			"<h3>Process Hacker</h3>"
			"<p>Licensed under the GNU GPL, v3.</p>"
			"<p>Copyright (c) 2008-2019</p>"
		).arg("3.0.2450");
		QString AboutText = QString(
                "<p>Thanks to:<br>"
                "    <a href=\"https://github.com/wj32\">wj32</a> - Wen Jia Liu<br>"
                "    <a href=\"https://github.com/dmex\">dmex</a> - Steven G<br>"
                "    <a href=\"https://github.com/xhmikosr\">XhmikosR</a><br>"
                "    <a href=\"https://github.com/processhacker/processhacker/graphs/contributors\">Contributors</a> - thank you for your additions!<br>"
                "    Donors - thank you for your support!</p>"
                "<p>Process Hacker uses the following components:<br>"
                "    <a href=\"https://github.com/michaelrsweet/mxml\">Mini-XML</a> by Michael Sweet<br>"
                "    <a href=\"https://www.pcre.org\">PCRE</a><br>"
                "    <a href=\"https://github.com/json-c/json-c\">json-c</a><br>"
                "    MD5 code by Jouni Malinen<br>"
                "    SHA1 code by Filip Navara, based on code by Steve Reid<br>"
                "    <a href=\"http://www.famfamfam.com/lab/icons/silk\">Silk icons</a><br>"
                "    <a href=\"https://www.fatcow.com/free-icons\">Farm-fresh web icons</a><br></p>"
			"<p></p>"
			"<p>Visit <a href=\"https://github.com/processhacker/processhacker\">Process Hacker on github</a> for more information.</p>"
		);
		QMessageBox *msgBox = new QMessageBox(parent);
		msgBox->setAttribute(Qt::WA_DeleteOnClose);
		msgBox->setWindowTitle(QString("About ProcessHacker Library"));
		msgBox->setText(AboutCaption);
		msgBox->setInformativeText(AboutText);

		QIcon ico(QLatin1String(":/ProcessHacker.png"));
		msgBox->setIconPixmap(ico.pixmap(64, 64));
#if defined(Q_WS_WINCE)
		msgBox->setDefaultButton(msgBox->addButton(QMessageBox::Ok));
#endif
		
		msgBox->exec();
}

#include <shellapi.h>

ULONG SelectedRunAsMode = ULONG_MAX;

PHAPPAPI HWND PhMainWndHandle = NULL;

BOOLEAN PhMwpOnNotify(
    _In_ NMHDR *Header,
    _Out_ LRESULT *Result
    )
{
    if (Header->code == RFN_VALIDATE)
    {
        LPNMRUNFILEDLG runFileDlg = (LPNMRUNFILEDLG)Header;

        if (SelectedRunAsMode == RUNAS_MODE_ADMIN)
        {
            PH_STRINGREF string;
            PH_STRINGREF fileName;
            PH_STRINGREF arguments;
            PPH_STRING fullFileName;
            PPH_STRING argumentsString;

            PhInitializeStringRefLongHint(&string, runFileDlg->lpszFile);
            PhParseCommandLineFuzzy(&string, &fileName, &arguments, &fullFileName);

            if (!fullFileName)
                fullFileName = PhCreateString2(&fileName);

            argumentsString = PhCreateString2(&arguments);

            if (PhShellExecuteEx(
                PhMainWndHandle,
                fullFileName->Buffer,
                argumentsString->Buffer,
                runFileDlg->ShowCmd,
                PH_SHELL_EXECUTE_ADMIN,
                0,
                NULL
                ))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                *Result = RF_RETRY;
            }

            PhDereferenceObject(fullFileName);
            PhDereferenceObject(argumentsString);

            return TRUE;
        }
        else if (SelectedRunAsMode == RUNAS_MODE_LIMITED)
        {
            NTSTATUS status;
            HANDLE tokenHandle;
            HANDLE newTokenHandle;

            if (NT_SUCCESS(status = PhOpenProcessToken(
                NtCurrentProcess(),
                TOKEN_ASSIGN_PRIMARY | TOKEN_DUPLICATE | TOKEN_QUERY | TOKEN_ADJUST_GROUPS |
                TOKEN_ADJUST_DEFAULT | READ_CONTROL | WRITE_DAC,
                &tokenHandle
                )))
            {
                if (NT_SUCCESS(status = PhFilterTokenForLimitedUser(
                    tokenHandle,
                    &newTokenHandle
                    )))
                {
                    status = PhCreateProcessWin32(
                        NULL,
                        runFileDlg->lpszFile,
                        NULL,
                        NULL,
                        0,
                        newTokenHandle,
                        NULL,
                        NULL
                        );

                    NtClose(newTokenHandle);
                }

                NtClose(tokenHandle);
            }

            if (NT_SUCCESS(status))
            {
                *Result = RF_CANCEL;
            }
            else
            {
                PhShowStatus(PhMainWndHandle, L"Unable to execute the program.", status, 0);
                *Result = RF_RETRY;
            }

            return TRUE;
        }
    }
    else if (Header->code == RFN_LIMITEDRUNAS)
    {
        LPNMRUNFILEDLG runFileDlg = (LPNMRUNFILEDLG)Header;
        PVOID WdcLibraryHandle;
        ULONG (WINAPI* WdcRunTaskAsInteractiveUser_I)(
            _In_ PCWSTR CommandLine,
            _In_ PCWSTR CurrentDirectory,
            _In_ ULONG Reserved
            );


        // dmex: Task Manager uses RFF_OPTRUNAS and RFN_LIMITEDRUNAS to show the 'Create this task with administrative privileges' checkbox
        // on the RunFileDlg when the current process is elevated. Task Manager also uses the WdcRunTaskAsInteractiveUser function to launch processes
        // as the interactive user from an elevated token. The WdcRunTaskAsInteractiveUser function
        // invokes the "\Microsoft\Windows\Task Manager\Interactive" Task Scheduler task for launching the process but 
        // doesn't return error information and we need to perform some sanity checks before invoking the task. 
        // Ideally, we should use our own task but for now just re-use the existing task and do what Task Manager does...

        if (WdcLibraryHandle = LoadLibrary(L"wdc.dll"))
        {
            if (WdcRunTaskAsInteractiveUser_I = (ULONG (WINAPI* )(_In_ PCWSTR CommandLine, _In_ PCWSTR CurrentDirectory, _In_ ULONG Reserved)) PhGetDllBaseProcedureAddress(WdcLibraryHandle, "WdcRunTaskAsInteractiveUser", 0))
            {
                PH_STRINGREF string;
                PPH_STRING commandlineString;
                PPH_STRING executeString = NULL;
                INT cmdlineArgCount;
                PWSTR* cmdlineArgList;

                PhInitializeStringRefLongHint(&string, (PWSTR)runFileDlg->lpszFile);
                commandlineString = PhCreateString2(&string);

                // Extract the filename.
                if (cmdlineArgList = CommandLineToArgvW(commandlineString->Buffer, &cmdlineArgCount))
                {
                    PPH_STRING fileName = PhCreateString(cmdlineArgList[0]);

                    if (fileName && !RtlDoesFileExists_U(fileName->Buffer))
                    {
                        PPH_STRING filePathString;

                        // The user typed a name without a path so attempt to locate the executable.
                        if (filePathString = PhSearchFilePath(fileName->Buffer, L".exe"))
                            PhMoveReference((PVOID*)&fileName, filePathString);
                        else
                            PhClearReference((PVOID*)&fileName);
                    }

                    if (fileName)
                    {
                        // Escape the filename.
                        PhMoveReference((PVOID*)&fileName, PhConcatStrings(3, L"\"", fileName->Buffer, L"\""));

                        if (cmdlineArgCount == 2)
                        {
                            PPH_STRING fileArgs = PhCreateString(cmdlineArgList[1]);

                            // Escape the parameters.
                            PhMoveReference((PVOID*)&fileArgs, PhConcatStrings(3, L"\"", fileArgs->Buffer, L"\""));

                            // Create the escaped execute string.
                            PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, fileName->Buffer, L" ", fileArgs->Buffer));

                            // Cleanup.
                            PhDereferenceObject(fileArgs);
                        }
                        else
                        {
                            // Create the escaped execute string.
                            executeString = (PPH_STRING)PhReferenceObject(fileName);
                        }

                        PhDereferenceObject(fileName);
                    }

                    LocalFree(cmdlineArgList);
                }

                if (!PhIsNullOrEmptyString(executeString))
                {
                    if (WdcRunTaskAsInteractiveUser_I(executeString->Buffer, NULL, 0) == 0)
                        *Result = RF_CANCEL;
                    else
                        *Result = RF_RETRY;
                }
                else
                {
                    *Result = RF_RETRY;
                }

                if (executeString)
                    PhDereferenceObject(executeString);
                PhDereferenceObject(commandlineString);
            }

            FreeLibrary((HMODULE)WdcLibraryHandle);
        }
        return TRUE;
    }

    return FALSE;
}

