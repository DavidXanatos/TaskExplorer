/*
 * System Informer -
 *   qt wrapper and appsup.c functions
 *
 * Copyright (C) 2010-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "appsup.h"
#include <settings.h>

PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(quint64 ProcessId, const QString& FileName)
{
	int knownProcessType;
	PH_STRINGREF systemRootPrefix;
	PPH_STRING fileName;
	PH_STRINGREF name;
#ifdef _WIN64
	BOOLEAN isWow64 = FALSE;
#endif

	if (ProcessId == (quint64)SYSTEM_PROCESS_ID || ProcessId == (quint64)SYSTEM_IDLE_PROCESS_ID || PH_IS_FAKE_PROCESS_ID((HANDLE)ProcessId) || FileName == "Registry")
		return SystemProcessType;

	if (FileName.isEmpty())
		return UnknownProcessType;

	PhGetSystemRoot(&systemRootPrefix);

	fileName = CastQString(FileName);
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
			else if (PhEqualStringRef2(&name, L"\\fontdrvhost.exe", TRUE))
				knownProcessType = WindowsOtherType;
			else if (PhEqualStringRef2(&name, L"\\dwm.exe", TRUE))
				knownProcessType = WindowsOtherType;
			else if (PhEqualStringRef2(&name, L"\\dasHost.exe", TRUE))
				knownProcessType = WindowsOtherType;
			else if (PhEqualStringRef2(&name, L"\\spoolsv.exe", TRUE))
				knownProcessType = WindowsOtherType;
			else if (PhEqualStringRef2(&name, L"\\wlanext.exe", TRUE))
				knownProcessType = WindowsOtherType;
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

BOOLEAN NTAPI PhpSvchostCommandLineCallback(_In_opt_ PPH_COMMAND_LINE_OPTION Option, _In_opt_ PPH_STRING Value, _In_opt_ PVOID Context)
{
    PPH_KNOWN_PROCESS_COMMAND_LINE knownCommandLine = (PPH_KNOWN_PROCESS_COMMAND_LINE)Context;

	if (Option && Option->Id == 1)
	{
		PhSwapReference((PVOID*)&knownCommandLine->ServiceHost.GroupName, Value);
	}

    return TRUE;
}


BOOLEAN PhaGetProcessKnownCommandLine(
    _In_ PPH_STRING CommandLine,
    _In_ PH_KNOWN_PROCESS_TYPE KnownProcessType,
    _Out_ PPH_KNOWN_PROCESS_COMMAND_LINE KnownCommandLine
    )
{
    switch (KnownProcessType & KnownProcessTypeMask)
    {
    case ServiceHostProcessType:
        {
            // svchost.exe -k <GroupName>

            static PH_COMMAND_LINE_OPTION options[] =
            {
                { 1, L"k", MandatoryArgumentType }
            };

            KnownCommandLine->ServiceHost.GroupName = NULL;

            PhParseCommandLine(
                &CommandLine->sr,
                options,
                sizeof(options) / sizeof(PH_COMMAND_LINE_OPTION),
                PH_COMMAND_LINE_IGNORE_UNKNOWN_OPTIONS,
                PhpSvchostCommandLineCallback,
                KnownCommandLine
                );

            if (KnownCommandLine->ServiceHost.GroupName)
            {
                PH_AUTO(KnownCommandLine->ServiceHost.GroupName);
                return TRUE;
            }
            else
            {
                return FALSE;
            }
        }
        break;
    case RunDllAsAppProcessType:
        {
            // rundll32.exe <DllName>,<ProcedureName> ...

            SIZE_T i;
            PH_STRINGREF dllNamePart;
            PH_STRINGREF procedureNamePart;
            PPH_STRING dllName;
            PPH_STRING procedureName;

            i = 0;

            // Get the rundll32.exe part.

            dllName = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!dllName)
                return FALSE;

            PhDereferenceObject(dllName);

            // Get the DLL name part.

            while (i < CommandLine->Length / sizeof(WCHAR) && CommandLine->Buffer[i] == ' ')
                i++;

            dllName = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!dllName)
                return FALSE;

            PH_AUTO(dllName);

            // The procedure name begins after the last comma.

            if (!PhSplitStringRefAtLastChar(&dllName->sr, ',', &dllNamePart, &procedureNamePart))
                return FALSE;

            dllName = (PPH_STRING)PH_AUTO(PhCreateString2(&dllNamePart));
            procedureName = (PPH_STRING)PH_AUTO(PhCreateString2(&procedureNamePart));

            // If the DLL name isn't an absolute path, assume it's in system32.
            // TO-DO: Use a proper search function.

            if (RtlDetermineDosPathNameType_U(dllName->Buffer) == RtlPathTypeRelative)
            {
                dllName = PhaConcatStrings(
                    3,
                    PH_AUTO_T(PH_STRING, PhGetSystemDirectory())->Buffer,
                    L"\\",
                    dllName->Buffer
                    );
            }

            KnownCommandLine->RunDllAsApp.FileName = dllName;
            KnownCommandLine->RunDllAsApp.ProcedureName = procedureName;
        }
        break;
    case ComSurrogateProcessType:
        {
            // dllhost.exe /processid:<Guid>

            static PH_STRINGREF inprocServer32Name = PH_STRINGREF_INIT(L"InprocServer32");

            SIZE_T i;
            ULONG_PTR indexOfProcessId;
            PPH_STRING argPart;
            PPH_STRING guidString;
            UNICODE_STRING guidStringUs;
            GUID guid;
            HANDLE rootKeyHandle;
            HANDLE inprocServer32KeyHandle;
            PPH_STRING fileName;

            i = 0;

            // Get the dllhost.exe part.

            argPart = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!argPart)
                return FALSE;

            PhDereferenceObject(argPart);

            // Get the argument part.

            while (i < (ULONG)CommandLine->Length / sizeof(WCHAR) && CommandLine->Buffer[i] == ' ')
                i++;

            argPart = PhParseCommandLinePart(&CommandLine->sr, &i);

            if (!argPart)
                return FALSE;

            PH_AUTO(argPart);

            // Find "/processid:"; the GUID is just after that.

            _wcsupr(argPart->Buffer);
            indexOfProcessId = PhFindStringInString(argPart, 0, L"/PROCESSID:");

            if (indexOfProcessId == -1)
                return FALSE;

            guidString = PhaSubstring(
                argPart,
                indexOfProcessId + 11,
                (ULONG)argPart->Length / sizeof(WCHAR) - indexOfProcessId - 11
                );
            PhStringRefToUnicodeString(&guidString->sr, &guidStringUs);

            if (!NT_SUCCESS(RtlGUIDFromString(
                &guidStringUs,
                &guid
                )))
                return FALSE;

            KnownCommandLine->ComSurrogate.Guid = guid;
            KnownCommandLine->ComSurrogate.Name = NULL;
            KnownCommandLine->ComSurrogate.FileName = NULL;

            // Lookup the GUID in the registry to determine the name and file name.

            if (NT_SUCCESS(PhOpenKey(
                &rootKeyHandle,
                KEY_READ,
                PH_KEY_CLASSES_ROOT,
                &PhaConcatStrings2(L"CLSID\\", guidString->Buffer)->sr,
                0
                )))
            {
                KnownCommandLine->ComSurrogate.Name =
                    (PPH_STRING)PH_AUTO(PhQueryRegistryString(rootKeyHandle, NULL));

                if (NT_SUCCESS(PhOpenKey(
                    &inprocServer32KeyHandle,
                    KEY_READ,
                    rootKeyHandle,
                    &inprocServer32Name,
                    0
                    )))
                {
                    KnownCommandLine->ComSurrogate.FileName =
                        (PPH_STRING)PH_AUTO(PhQueryRegistryString(inprocServer32KeyHandle, NULL));

                    if (fileName = (PPH_STRING)PH_AUTO(PhExpandEnvironmentStrings(
                        &KnownCommandLine->ComSurrogate.FileName->sr
                        )))
                    {
                        KnownCommandLine->ComSurrogate.FileName = fileName;
                    }

                    NtClose(inprocServer32KeyHandle);
                }

                NtClose(rootKeyHandle);
            }
            else if (NT_SUCCESS(PhOpenKey(
                &rootKeyHandle,
                KEY_READ,
                PH_KEY_CLASSES_ROOT,
                &PhaConcatStrings2(L"AppID\\", guidString->Buffer)->sr,
                0
                )))
            {
                KnownCommandLine->ComSurrogate.Name =
                    (PPH_STRING)PH_AUTO(PhQueryRegistryString(rootKeyHandle, NULL));
                NtClose(rootKeyHandle);
            }
        }
        break;
    default:
        return FALSE;
    }

    return TRUE;
}

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

ULONG GetProcessDpiAwareness(HANDLE QueryHandle)
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static BOOL (WINAPI *getProcessDpiAwarenessInternal)(_In_ HANDLE hprocess,_Out_ ULONG *value);

    if (PhBeginInitOnce(&initOnce))
    {
        getProcessDpiAwarenessInternal = (BOOL (WINAPI *)(_In_ HANDLE hprocess,_Out_ ULONG *value))PhGetDllProcedureAddress(L"user32.dll", "GetProcessDpiAwarenessInternal", 0);
        PhEndInitOnce(&initOnce);
    }

    if (!getProcessDpiAwarenessInternal)
        return 0;

    if (QueryHandle)
    {
        ULONG dpiAwareness;
        if (getProcessDpiAwarenessInternal(QueryHandle, &dpiAwareness))
            return dpiAwareness + 1;
    }
	return 0;
}

BOOLEAN PhpSelectFavoriteInRegedit(
    _In_ HWND RegeditWindow,
    _In_ PPH_STRINGREF FavoriteName,
    _In_ BOOLEAN UsePhSvc
    )
{
    HMENU menu;
    HMENU favoritesMenu;
    ULONG count;
    ULONG i;
    ULONG id = ULONG_MAX;

    if (!(menu = GetMenu(RegeditWindow)))
        return FALSE;

    // Cause the Registry Editor to refresh the Favorites menu.
    if (UsePhSvc)
        PhSvcCallSendMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(3, MF_POPUP), (LPARAM)menu);
    else
        SendMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(3, MF_POPUP), (LPARAM)menu);

    if (!(favoritesMenu = GetSubMenu(menu, 3)))
        return FALSE;

    // Find our entry.

    count = GetMenuItemCount(favoritesMenu);

    if (count == -1)
        return FALSE;
    if (count > 1000)
        count = 1000;

    for (i = 3; i < count; i++)
    {
        MENUITEMINFO info = { sizeof(MENUITEMINFO) };
        WCHAR buffer[MAX_PATH];

        info.fMask = MIIM_ID | MIIM_STRING;
        info.dwTypeData = buffer;
        info.cch = RTL_NUMBER_OF(buffer);
        GetMenuItemInfo(favoritesMenu, i, TRUE, &info);

        if (info.cch == FavoriteName->Length / sizeof(WCHAR))
        {
            PH_STRINGREF text;

            text.Buffer = buffer;
            text.Length = info.cch * sizeof(WCHAR);

            if (PhEqualStringRef(&text, FavoriteName, TRUE))
            {
                id = info.wID;
                break;
            }
        }
    }

    if (id == ULONG_MAX)
        return FALSE;

    // Activate our entry.
    if (UsePhSvc)
        PhSvcCallSendMessage(RegeditWindow, WM_COMMAND, MAKEWPARAM(id, 0), 0);
    else
        SendMessage(RegeditWindow, WM_COMMAND, MAKEWPARAM(id, 0), 0);

    // "Close" the Favorites menu and restore normal status bar text.
    if (UsePhSvc)
        PhSvcCallPostMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(0, 0xffff), 0);
    else
        PostMessage(RegeditWindow, WM_MENUSELECT, MAKEWPARAM(0, 0xffff), 0);

    // Bring regedit to the top.
    //if (IsMinimized(RegeditWindow))
	if (IsIconic(RegeditWindow))
    {
        ShowWindow(RegeditWindow, SW_RESTORE);
        SetForegroundWindow(RegeditWindow);
    }
    else
    {
        SetForegroundWindow(RegeditWindow);
    }

    return TRUE;
}

/**
 * Opens a key in the Registry Editor.
 *
 * \param WindowHandle A handle to the parent window.
 * \param KeyName The key name to open.
 */
VOID PhShellOpenKey(
    _In_ HWND WindowHandle,
    _In_ PPH_STRING KeyName
    )
{
    static PH_STRINGREF regeditKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit");
    NTSTATUS status;
    HANDLE regeditKeyHandle;
    PPH_STRING lastKey;
    PPH_STRING regeditFileName;
    PH_STRINGREF systemRootString;

    status = PhCreateKey(
        &regeditKeyHandle,
        KEY_WRITE,
        PH_KEY_CURRENT_USER,
        &regeditKeyName,
        0,
        0,
        NULL
        );

    if (!NT_SUCCESS(status))
    {
        PhShowStatus(WindowHandle, L"Unable to execute the program.", status, 0);
        return;
    }

    lastKey = PhExpandKeyName(KeyName, FALSE);
    PhSetValueKeyZ(regeditKeyHandle, L"LastKey", REG_SZ, lastKey->Buffer, (ULONG)lastKey->Length + sizeof(UNICODE_NULL));
    NtClose(regeditKeyHandle);
    PhDereferenceObject(lastKey);

    // Start regedit. If we aren't elevated, request that regedit be elevated. This is so we can get
    // the consent dialog in the center of the specified window. (wj32)

    PhGetSystemRoot(&systemRootString);
    regeditFileName = PhConcatStringRefZ(&systemRootString, L"\\regedit.exe");

    status = PhShellExecuteEx(WindowHandle, regeditFileName->Buffer, NULL, NULL, SW_SHOW, PhGetOwnTokenAttributes().Elevated ? PH_SHELL_EXECUTE_ADMIN : 0, 0, NULL);
    if (!NT_SUCCESS(status))
    {
        PhShowStatus(WindowHandle, L"Unable to execute the program.", status, 0);
    }

    PhDereferenceObject(regeditFileName);
}

/**
 * Opens a key in the Registry Editor. If the Registry Editor is already open,
 * the specified key is selected in the Registry Editor.
 *
 * \param hWnd A handle to the parent window.
 * \param KeyName The key name to open.
 */
BOOLEAN PhShellOpenKey2(
    _In_ HWND hWnd,
    _In_ PPH_STRING KeyName
    )
{
    static PH_STRINGREF favoritesKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit\\Favorites");

    BOOLEAN result = FALSE;
    HWND regeditWindow;
    HANDLE favoritesKeyHandle;
    WCHAR favoriteName[32];
    UNICODE_STRING valueName;
    PH_STRINGREF valueNameSr;
    PPH_STRING expandedKeyName;

    regeditWindow = FindWindow(L"RegEdit_RegEdit", NULL);

    if (!regeditWindow)
    {
        PhShellOpenKey(hWnd, KeyName);
        return TRUE;
    }

	BOOLEAN UsePhSvc = !PhGetOwnTokenAttributes().Elevated;

    //if (UsePhSvc)
    //{
    //    if (!PhUiConnectToPhSvc(hWnd, FALSE))
    //        return FALSE;
    //}

    // Create our entry in Favorites.

    if (!NT_SUCCESS(PhCreateKey(
        &favoritesKeyHandle,
        KEY_WRITE,
        PH_KEY_CURRENT_USER,
        &favoritesKeyName,
        0,
        0,
        NULL
        )))
        goto CleanupExit;

    memcpy(favoriteName, L"A_ProcessHacker", 15 * sizeof(WCHAR));
    PhGenerateRandomAlphaString(&favoriteName[15], 16);
    RtlInitUnicodeString(&valueName, favoriteName);
    PhUnicodeStringToStringRef(&valueName, &valueNameSr);

    expandedKeyName = PhExpandKeyName(KeyName, FALSE);
    NtSetValueKey(favoritesKeyHandle, &valueName, 0, REG_SZ, expandedKeyName->Buffer, (ULONG)expandedKeyName->Length + sizeof(UNICODE_NULL));
    PhDereferenceObject(expandedKeyName);

    // Select our entry in regedit.
    result = PhpSelectFavoriteInRegedit(regeditWindow, &valueNameSr, UsePhSvc);

    NtDeleteValueKey(favoritesKeyHandle, &valueName);
    NtClose(favoritesKeyHandle);

CleanupExit:
    //if (UsePhSvc)
    //    PhUiDisconnectFromPhSvc();

    return result;
}

#include <shellapi.h>

VOID PhShellExecuteUserString(
    _In_ HWND hWnd,
    _In_ PWSTR Setting,
    _In_ PWSTR String,
    _In_ BOOLEAN UseShellExecute,
    _In_opt_ PWSTR ErrorMessage
    )
{
    static PH_STRINGREF replacementToken = PH_STRINGREF_INIT(L"%s");
    PPH_STRING applicationDirectory;
    PPH_STRING executeString;
    PH_STRINGREF stringBefore;
    PH_STRINGREF stringAfter;
    PPH_STRING ntMessage;

    if (!(applicationDirectory = PhGetApplicationDirectory()))
    {
        PhShowStatus(hWnd, L"Unable to locate the application directory.", STATUS_NOT_FOUND, 0);
        return;
    }

    // Get the execute command.
    executeString = PhGetStringSetting(Setting);

    // Expand environment strings.
    PhMoveReference((PVOID*)&executeString, PhExpandEnvironmentStrings(&executeString->sr));

    // Make sure the user executable string is absolute. We can't use RtlDetermineDosPathNameType_U
    // here because the string may be a URL.
    if (PhFindCharInString(executeString, 0, ':') == -1)
    {
        INT stringArgCount;
        PWSTR* stringArgList;

        // (dmex) HACK: Escape the individual executeString components.
        if ((stringArgList = CommandLineToArgvW(executeString->Buffer, &stringArgCount)) && stringArgCount == 2)
        {
            PPH_STRING fileName = PhCreateString(stringArgList[0]);
            PPH_STRING fileArgs = PhCreateString(stringArgList[1]);

            // Make sure the string is absolute and escape the filename.
            if (RtlDetermineDosPathNameType_U(fileName->Buffer) == RtlPathTypeRelative)
                PhMoveReference((PVOID*)&fileName, PhConcatStrings(4, L"\"", applicationDirectory->Buffer, fileName->Buffer, L"\""));
            else
                PhMoveReference((PVOID*)&fileName, PhConcatStrings(3, L"\"", fileName->Buffer, L"\""));

            // Escape the parameters.
            PhMoveReference((PVOID*)&fileArgs, PhConcatStrings(3, L"\"", fileArgs->Buffer, L"\""));

            // Create the escaped execute string.
            PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, fileName->Buffer, L" ", fileArgs->Buffer));

            PhDereferenceObject(fileArgs);
            PhDereferenceObject(fileName);
            LocalFree(stringArgList);
        }
        else
        {
            if (RtlDetermineDosPathNameType_U(executeString->Buffer) == RtlPathTypeRelative)
                PhMoveReference((PVOID*)&executeString, PhConcatStrings(4, L"\"", applicationDirectory->Buffer, executeString->Buffer, L"\""));
            else
                PhMoveReference((PVOID*)&executeString, PhConcatStrings(3, L"\"", executeString->Buffer, L"\""));
        }
    }

    // Replace the token with the string, or use the original string if the token is not present.
    if (PhSplitStringRefAtString(&executeString->sr, &replacementToken, FALSE, &stringBefore, &stringAfter))
    {
        PPH_STRING stringTemp;
        PPH_STRING stringMiddle;

        // Note: This code is needed to solve issues with faulty RamDisk software that doesn't use the Mount Manager API
        // and instead returns \device\ FileName strings. We also can't change the way the process provider stores 
        // the FileName string since it'll break various features and use-cases required by developers 
        // who need the raw untranslated FileName string.
        stringTemp = PhCreateString(String);
        stringMiddle = PhGetFileName(stringTemp);

        PhMoveReference((PVOID*)&executeString, PhConcatStringRef3(&stringBefore, &stringMiddle->sr, &stringAfter));

        PhDereferenceObject(stringMiddle);
        PhDereferenceObject(stringTemp);
    }

    if (UseShellExecute)
    {
        PhShellExecute(hWnd, executeString->Buffer, NULL);
    }
    else
    {
        NTSTATUS status;

        status = PhCreateProcessWin32(NULL, executeString->Buffer, NULL, NULL, 0, NULL, NULL, NULL);

        if (!NT_SUCCESS(status))
        {
            if (ErrorMessage)
            {
                ntMessage = PhGetNtMessage(status);
                PhShowError2(
                    hWnd,
                    L"Unable to execute the command.",
                    L"%s\n%s",
                    PhGetStringOrDefault(ntMessage, L"An unknown error occurred."),
                    ErrorMessage
                    );
                PhDereferenceObject(ntMessage);
            }
            else
            {
                PhShowStatus(hWnd, L"Unable to execute the command.", status, 0);
            }
        }
    }

    PhDereferenceObject(executeString);
    PhDereferenceObject(applicationDirectory);
}


////////////////////////////////////////////////////////////////////////////////////
// other

#include "../../../SVC/TaskService.h"

NTSTATUS PhSvcCallSendMessage(
	_In_opt_ HWND hWnd,
	_In_ UINT Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam,
	_In_ BOOLEAN Post
)
{
	QString SocketName = CTaskService::RunWorker();
	if (SocketName.isEmpty())
		return STATUS_ACCESS_DENIED;

	QVariantMap Parameters;
	Parameters["hWnd"] = (quint64)hWnd;
	Parameters["Msg"] = (quint64)Msg;
	Parameters["wParam"] = (quint64)wParam;
	Parameters["lParam"] = (quint64)lParam;
	Parameters["Post"] = (bool)Post;

	QVariantMap Request;
	Request["Command"] = "SendMessage";
	Request["Parameters"] = Parameters;

	CTaskService::SendCommand(SocketName, Request);

	return STATUS_SUCCESS;
}

