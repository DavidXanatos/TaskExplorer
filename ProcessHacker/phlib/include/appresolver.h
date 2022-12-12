/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     dmex    2017-2018
 *
 */

#ifndef _PH_APPRESOLVER_H
#define _PH_APPRESOLVER_H

#ifdef __cplusplus
extern "C" {
#endif

_Success_(return)
BOOLEAN 
NTAPI
PhAppResolverGetAppIdForProcess(
    _In_ HANDLE ProcessId,
    _Out_ PPH_STRING *ApplicationUserModelId
    );

_Success_(return)
BOOLEAN 
NTAPI
PhAppResolverGetAppIdForWindow(
    _In_ HWND WindowHandle,
    _Out_ PPH_STRING *ApplicationUserModelId
    );

HRESULT 
NTAPI
PhAppResolverActivateAppId(
    _In_ PPH_STRING AppUserModelId,
    _In_opt_ PWSTR CommandLine,
    _Out_opt_ HANDLE *ProcessId
    );

typedef struct _PH_PACKAGE_TASK_ENTRY
{
    PPH_STRING TaskName;
    GUID TaskGuid;
} PH_PACKAGE_TASK_ENTRY, *PPH_PACKAGE_TASK_ENTRY;

PPH_LIST 
NTAPI
PhAppResolverEnumeratePackageBackgroundTasks(
    _In_ PPH_STRING PackageFullName
    );

HRESULT 
NTAPI 
PhAppResolverPackageStopSessionRedirection(
    _In_ PPH_STRING PackageFullName
    );

PPH_STRING 
NTAPI
PhGetAppContainerName(
    _In_ PSID AppContainerSid
    );

PPH_STRING 
NTAPI
PhGetAppContainerSidFromName(
    _In_ PWSTR AppContainerName
    );

PPH_STRING 
NTAPI
PhGetAppContainerPackageName(
    _In_ PSID Sid
    );

PPH_STRING 
NTAPI
PhGetProcessPackageFullName(
    _In_ HANDLE ProcessHandle
    );

BOOLEAN 
NTAPI
PhIsTokenFullTrustAppPackage(
    _In_ HANDLE TokenHandle
    );

BOOLEAN 
NTAPI
PhIsPackageCapabilitySid(
    _In_ PSID AppContainerSid,
    _In_ PSID Sid
    );

PPH_STRING 
NTAPI
PhGetPackagePath(
    _In_ PPH_STRING PackageFullName
    );

PPH_LIST 
NTAPI
PhGetPackageAssetsFromResourceFile(
    _In_ PWSTR FilePath
    );

// Immersive PLM task support

HRESULT 
NTAPI
PhAppResolverBeginCrashDumpTask(
    _In_ HANDLE ProcessId,
    _Out_ HANDLE* TaskHandle
    );

HRESULT 
NTAPI
PhAppResolverBeginCrashDumpTaskByHandle(
    _In_ HANDLE ProcessHandle,
    _Out_ HANDLE* TaskHandle
    );

HRESULT 
NTAPI
PhAppResolverEndCrashDumpTask(
    _In_ HANDLE TaskHandle
    );

#ifdef __cplusplus
}
#endif

#endif
