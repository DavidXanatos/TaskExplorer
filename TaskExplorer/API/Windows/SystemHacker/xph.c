/*
 * This file is part of ProcessHacker.
 *
 * MIT License
 * 
 * Copyright (C) 2025 DavidXanatos
 * Copyright (C) 2022 Winsider Seminars & Solutions, Inc.
 * Copyright (C) 2009-2016 wj32
 * 
 * https://github.com/winsiderss/systeminformer/blob/c6e44b2feaaa0eb50d8d5fbd2f3db3fa4c176dbb/LICENSE.txt
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

// Windows Header Files
//#define WIN32_LEAN_AND_MEAN
//#include <windows.h>
#include <phnt_windows.h>
#include <phnt.h>

#include "xphuser.h"
#include "xphuserp.h"

#define PTR_ADD_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) + (ULONG_PTR)(Offset)))
#define PTR_SUB_OFFSET(Pointer, Offset) ((PVOID)((ULONG_PTR)(Pointer) - (ULONG_PTR)(Offset)))

static FORCEINLINE NTSTATUS PhGetLastWin32ErrorAsNtStatus(VOID)
{
    ULONG win32Result;

    // This is needed because NTSTATUS_FROM_WIN32 uses the argument multiple times.
    win32Result = GetLastError();

    return NTSTATUS_FROM_WIN32(win32Result);
}

HANDLE PhXphHandle = NULL;
BOOLEAN PhXphVerified = FALSE;
KPH_KEY PhXphL1Key = 0;

//CONST SID PhSeInteractiveSid = { SID_REVISION, 1, SECURITY_NT_AUTHORITY, { SECURITY_INTERACTIVE_RID } };
//CONST SID PhSeServiceSid = { SID_REVISION, 1, SECURITY_NT_AUTHORITY, { SECURITY_SERVICE_RID } };
extern CONST SID PhSeInteractiveSid;
extern CONST SID PhSeServiceSid;

BOOL HasMasterKey()
{
    return PhXphL1Key == -1; // LOL "Master"-key
}

NTSTATUS NTAPI XphConnect(
    _In_opt_ PWSTR DeviceName
    )
{
    NTSTATUS status;
    HANDLE kphHandle;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK isb;
    OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

    if (PhXphHandle)
        return STATUS_ADDRESS_ALREADY_EXISTS;

    if (DeviceName)
        RtlInitUnicodeString(&objectName, DeviceName);
    else
        RtlInitUnicodeString(&objectName, KPH_DEVICE_NAME);

    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE,
        NULL,
        NULL
        );

    status = NtOpenFile(
        &kphHandle,
        FILE_GENERIC_READ | FILE_GENERIC_WRITE,
        &objectAttributes,
        &isb,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        FILE_NON_DIRECTORY_FILE
        );

    if (NT_SUCCESS(status))
    {
        // Protect the handle from being closed.

        handleFlagInfo.Inherit = FALSE;
        handleFlagInfo.ProtectFromClose = TRUE;

        NtSetInformationObject(
            kphHandle,
            ObjectHandleFlagInformation,
            &handleFlagInfo,
            sizeof(OBJECT_HANDLE_FLAG_INFORMATION)
            );

        PhXphHandle = kphHandle;
        PhXphVerified = FALSE;
        PhXphL1Key = 0;
    }

    return status;
}

NTSTATUS NTAPI XphConnect2(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    )
{
    return XphConnect2Ex(DeviceName, FileName, NULL);
}

NTSTATUS NTAPI XphConnect2Ex(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKPH_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    WCHAR fullDeviceName[256];
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle;
    BOOLEAN started = FALSE;
    BOOLEAN created = FALSE;

    if (!DeviceName)
        DeviceName = KPH_DEVICE_SHORT_NAME;

	wcscpy(fullDeviceName, L"\\Device\\");
	wcscat(fullDeviceName, DeviceName);
    
    // Try to open the device.
    status = XphConnect(fullDeviceName);

    if (NT_SUCCESS(status) || status == STATUS_ADDRESS_ALREADY_EXISTS)
        return status;

    if (
        status != STATUS_NO_SUCH_DEVICE &&
        status != STATUS_NO_SUCH_FILE &&
        status != STATUS_OBJECT_NAME_NOT_FOUND &&
        status != STATUS_OBJECT_PATH_NOT_FOUND
        )
        return status;

    // Load the driver, and try again.

    // Try to start the service, if it exists.

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (scmHandle)
    {
        serviceHandle = OpenService(scmHandle, DeviceName, SERVICE_START);

        if (serviceHandle)
        {
            if (StartService(serviceHandle, 0, NULL))
                started = TRUE;

            CloseServiceHandle(serviceHandle);
        }

        CloseServiceHandle(scmHandle);
    }

    if (!started /*&& PhDoesFileExistsWin32(FileName)*/)
    {
        // Try to create the service.

        scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

        if (scmHandle)
        {
            serviceHandle = CreateService(
                scmHandle,
                DeviceName,
                DeviceName,
                SERVICE_ALL_ACCESS,
                SERVICE_KERNEL_DRIVER,
                SERVICE_DEMAND_START,
                SERVICE_ERROR_IGNORE,
                FileName,
                NULL,
                NULL,
                NULL,
                NULL,
                L""
                );

            if (serviceHandle)
            {
                created = TRUE;

                XphSetServiceSecurity(serviceHandle);

                // Set parameters if the caller supplied them. Note that we fail the entire function
                // if this fails, because failing to set parameters like SecurityLevel may result in
                // security vulnerabilities.
                if (Parameters)
                {
                    status = XphSetParameters(DeviceName, Parameters);

                    if (!NT_SUCCESS(status))
                    {
                        // Delete the service and fail.
                        goto CreateAndConnectEnd;
                    }
                }

                if (StartService(serviceHandle, 0, NULL))
                    started = TRUE;
                else
                    status = PhGetLastWin32ErrorAsNtStatus();
            }
            else
            {
                status = PhGetLastWin32ErrorAsNtStatus();
            }

            CloseServiceHandle(scmHandle);
        }
    }

    if (started)
    {
        // Try to open the device again.
        status = XphConnect(fullDeviceName);
    }

CreateAndConnectEnd:
    if (created && serviceHandle)
    {
        // "Delete" the service. Since we (may) have a handle to the device, the SCM will delete the
        // service automatically when it is stopped (upon reboot). If we don't have a handle to the
        // device, the service will get deleted immediately, which is a good thing anyway.
        DeleteService(serviceHandle);
        CloseServiceHandle(serviceHandle);
    }

    return status;
}

NTSTATUS NTAPI XphDisconnect(
    VOID
    )
{
    NTSTATUS status;
    OBJECT_HANDLE_FLAG_INFORMATION handleFlagInfo;

    if (!PhXphHandle)
        return STATUS_ALREADY_DISCONNECTED;

    // Unprotect the handle.

    handleFlagInfo.Inherit = FALSE;
    handleFlagInfo.ProtectFromClose = FALSE;

    NtSetInformationObject(
        PhXphHandle,
        ObjectHandleFlagInformation,
        &handleFlagInfo,
        sizeof(OBJECT_HANDLE_FLAG_INFORMATION)
        );

    status = NtClose(PhXphHandle);
    PhXphHandle = NULL;
    PhXphVerified = FALSE;
    PhXphL1Key = 0;

    return status;
}

BOOLEAN NTAPI XphIsConnected(
    VOID
    )
{
    return PhXphHandle != NULL;
}

BOOLEAN NTAPI XphIsVerified(
    VOID
    )
{
    return PhXphVerified;
}

NTSTATUS NTAPI XphSetParameters(
    _In_opt_ PWSTR DeviceName,
    _In_ PKPH_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    HANDLE parametersKeyHandle = NULL;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING keyName;
    ULONG disposition;
    UNICODE_STRING valueName;
    SIZE_T returnLength;
    WCHAR parametersKeyName[MAX_PATH];

	wcscpy(parametersKeyName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\");
	wcscat(parametersKeyName, DeviceName ? DeviceName : KPH_DEVICE_SHORT_NAME);
	wcscat(parametersKeyName, L"\\Parameters");
    
    RtlInitUnicodeString(&keyName, parametersKeyName);
    InitializeObjectAttributes(&objAttr, &keyName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = NtCreateKey(
        &parametersKeyHandle,
        KEY_WRITE | DELETE,
        &objAttr,
        0,
        NULL,
        REG_OPTION_NON_VOLATILE,
        &disposition
    );

    if (!NT_SUCCESS(status))
        return status;

    RtlInitUnicodeString(&valueName, L"SecurityLevel");
    status = NtSetValueKey(
        parametersKeyHandle,
        &valueName,
        0,
        REG_DWORD,
        &Parameters->SecurityLevel,
        sizeof(ULONG)
        );

    if (!NT_SUCCESS(status))
        goto SetValuesEnd;

#ifdef XPH_USE_DYN_DATA
    if (Parameters->CreateDynamicConfiguration)
    {
        KPH_DYN_CONFIGURATION configuration;

        configuration.Version = KPH_DYN_CONFIGURATION_VERSION;
        configuration.NumberOfPackages = 1;

        if (NT_SUCCESS(XphInitializeDynamicPackage(&configuration.Packages[0])))
        {
            RtlInitUnicodeString(&valueName, L"DynamicConfiguration");
            status = NtSetValueKey(
                parametersKeyHandle,
                &valueName,
                0,
                REG_BINARY,
                &configuration,
                sizeof(KPH_DYN_CONFIGURATION)
                );

            if (!NT_SUCCESS(status))
                goto SetValuesEnd;
        }
    }
#endif

    // Put more parameters here...

SetValuesEnd:
    if (!NT_SUCCESS(status))
    {
        // Delete the key if we created it.
        if (disposition == REG_CREATED_NEW_KEY)
            NtDeleteKey(parametersKeyHandle);
    }

    NtClose(parametersKeyHandle);

    return status;
}

NTSTATUS NTAPI XphResetParameters(
    _In_opt_ PWSTR DeviceName
    )
{
    NTSTATUS status = STATUS_UNSUCCESSFUL;
    HANDLE parametersKeyHandle = NULL;
    OBJECT_ATTRIBUTES objAttr;
    UNICODE_STRING keyName;
    SIZE_T returnLength;
    WCHAR parametersKeyName[MAX_PATH];

	wcscpy(parametersKeyName, L"\\Registry\\Machine\\System\\CurrentControlSet\\Services\\");
	wcscat(parametersKeyName, DeviceName ? DeviceName : KPH_DEVICE_SHORT_NAME);
	wcscat(parametersKeyName, L"\\Parameters");

    status = XphUninstall(DeviceName);
    status = WIN32_FROM_NTSTATUS(status);

    if (status == ERROR_SERVICE_DOES_NOT_EXIST)
        status = STATUS_SUCCESS;

    if (NT_SUCCESS(status))
    {
        RtlInitUnicodeString(&keyName, parametersKeyName);
        InitializeObjectAttributes(&objAttr, &keyName, OBJ_CASE_INSENSITIVE, NULL, NULL);

        status = NtOpenKey(
            &parametersKeyHandle,
            DELETE, // Desired access
            &objAttr
        );
    }

    if (NT_SUCCESS(status) && parametersKeyHandle)
        status = NtDeleteKey(parametersKeyHandle);

    if (status == STATUS_OBJECT_NAME_NOT_FOUND)
        status = STATUS_SUCCESS;

    if (parametersKeyHandle)
        NtClose(parametersKeyHandle);

    return status;
}

VOID NTAPI XphSetServiceSecurity(
    _In_ SC_HANDLE ServiceHandle
    )
{
    static SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSECURITY_DESCRIPTOR securityDescriptor;
    ULONG sdAllocationLength;
    UCHAR administratorsSidBuffer[FIELD_OFFSET(SID, SubAuthority) + sizeof(ULONG) * 2];
    PSID administratorsSid;
    PACL dacl;

    administratorsSid = (PSID)administratorsSidBuffer;
    RtlInitializeSid(administratorsSid, &ntAuthority, 2);
    *RtlSubAuthoritySid(administratorsSid, 0) = SECURITY_BUILTIN_DOMAIN_RID;
    *RtlSubAuthoritySid(administratorsSid, 1) = DOMAIN_ALIAS_RID_ADMINS;

    sdAllocationLength = SECURITY_DESCRIPTOR_MIN_LENGTH +
        (ULONG)sizeof(ACL) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        RtlLengthSid((PSID)&PhSeServiceSid) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        RtlLengthSid(administratorsSid) +
        (ULONG)sizeof(ACCESS_ALLOWED_ACE) +
        RtlLengthSid((PSID)&PhSeInteractiveSid);

    securityDescriptor = malloc(sdAllocationLength);
    dacl = (PACL)PTR_ADD_OFFSET(securityDescriptor, SECURITY_DESCRIPTOR_MIN_LENGTH);

    RtlCreateSecurityDescriptor(securityDescriptor, SECURITY_DESCRIPTOR_REVISION);
    RtlCreateAcl(dacl, sdAllocationLength - SECURITY_DESCRIPTOR_MIN_LENGTH, ACL_REVISION);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, SERVICE_ALL_ACCESS, (PSID)&PhSeServiceSid);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, SERVICE_ALL_ACCESS, administratorsSid);
    RtlAddAccessAllowedAce(dacl, ACL_REVISION, 
        SERVICE_QUERY_CONFIG |
        SERVICE_QUERY_STATUS |
        SERVICE_START |
        SERVICE_STOP |
        SERVICE_INTERROGATE |
        DELETE,
        (PSID)&PhSeInteractiveSid
        );
    RtlSetDaclSecurityDescriptor(securityDescriptor, TRUE, dacl, FALSE);

    SetServiceObjectSecurity(ServiceHandle, DACL_SECURITY_INFORMATION, securityDescriptor);

    free(securityDescriptor);
}

NTSTATUS NTAPI XphInstall(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    )
{
    return XphInstallEx(DeviceName, FileName, NULL);
}

NTSTATUS NTAPI XphInstallEx(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKPH_PARAMETERS Parameters
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle;

    if (!DeviceName)
        DeviceName = KPH_DEVICE_SHORT_NAME;

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE);

    if (!scmHandle)
        return PhGetLastWin32ErrorAsNtStatus();

    serviceHandle = CreateService(
        scmHandle,
        DeviceName,
        DeviceName,
        SERVICE_ALL_ACCESS,
        SERVICE_KERNEL_DRIVER,
        SERVICE_SYSTEM_START,
        SERVICE_ERROR_IGNORE,
        FileName,
        NULL,
        NULL,
        NULL,
        NULL,
        L""
        );

    if (serviceHandle)
    {
        XphSetServiceSecurity(serviceHandle);

        // See XphConnect2Ex for more details.
        if (Parameters)
        {
            status = XphSetParameters(DeviceName, Parameters);

            if (!NT_SUCCESS(status))
            {
                DeleteService(serviceHandle);
                goto CreateEnd;
            }
        }

        if (!StartService(serviceHandle, 0, NULL))
            status = PhGetLastWin32ErrorAsNtStatus();

CreateEnd:
        CloseServiceHandle(serviceHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    CloseServiceHandle(scmHandle);

    return status;
}

NTSTATUS NTAPI XphUninstall(
    _In_opt_ PWSTR DeviceName
    )
{
    NTSTATUS status = STATUS_SUCCESS;
    SC_HANDLE scmHandle;
    SC_HANDLE serviceHandle;

    scmHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);

    if (!scmHandle)
        return PhGetLastWin32ErrorAsNtStatus();

    serviceHandle = OpenService(scmHandle, DeviceName ? DeviceName : KPH_DEVICE_SHORT_NAME, SERVICE_STOP | DELETE);

    if (serviceHandle)
    {
        SERVICE_STATUS serviceStatus;

        ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus);

        if (!DeleteService(serviceHandle))
            status = PhGetLastWin32ErrorAsNtStatus();

        CloseServiceHandle(serviceHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    CloseServiceHandle(scmHandle);

    return status;
}

NTSTATUS NTAPI XphGetFeatures(
    _Inout_ PULONG Features
    )
{
    struct
    {
        PULONG Features;
    } input = { Features };

    return XphpDeviceIoControl(
        KPH_GETFEATURES,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphVerifyClient(
    _In_reads_bytes_(SignatureSize) PUCHAR Signature,
    _In_ ULONG SignatureSize
    )
{
    NTSTATUS status;
    struct
    {
        PVOID CodeAddress;
        PUCHAR Signature;
        ULONG SignatureSize;
    } input = { XphpWithKeyApcRoutine, Signature, SignatureSize };

    status = XphpDeviceIoControl(
        KPH_VERIFYCLIENT,
        &input,
        sizeof(input)
        );

    if (NT_SUCCESS(status))
        PhXphVerified = TRUE;

    return status;
}

NTSTATUS NTAPI XphOpenProcess(
    _Inout_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    )
{
    KPH_OPEN_PROCESS_INPUT input = { ProcessHandle, DesiredAccess, ClientId, 0 };

    if ((DesiredAccess & KPH_PROCESS_READ_ACCESS) == DesiredAccess || HasMasterKey())
    {
        XphpGetL1Key(&input.Key);
        return XphpDeviceIoControl(
            KPH_OPENPROCESS,
            &input,
            sizeof(input)
            );
    }
    else
    {
        return XphpWithKey(KphKeyLevel2, XphpOpenProcessContinuation, &input);
    }
}

NTSTATUS NTAPI XphOpenProcessToken(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE TokenHandle
    )
{
    KPH_OPEN_PROCESS_TOKEN_INPUT input = { ProcessHandle, DesiredAccess, TokenHandle, 0 };

    if ((DesiredAccess & KPH_TOKEN_READ_ACCESS) == DesiredAccess || HasMasterKey())
    {
        XphpGetL1Key(&input.Key);
        return XphpDeviceIoControl(
            KPH_OPENPROCESSTOKEN,
            &input,
            sizeof(input)
            );
    }
    else
    {
        return XphpWithKey(KphKeyLevel2, XphpOpenProcessTokenContinuation, &input);
    }
}

NTSTATUS NTAPI XphOpenProcessJob(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE JobHandle
    )
{
    struct
    {
        HANDLE ProcessHandle;
        ACCESS_MASK DesiredAccess;
        PHANDLE JobHandle;
    } input = { ProcessHandle, DesiredAccess, JobHandle };

    return XphpDeviceIoControl(
        KPH_OPENPROCESSJOB,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphTerminateProcess(
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    )
{
    NTSTATUS status;
    KPH_TERMINATE_PROCESS_INPUT input = { ProcessHandle, ExitStatus, 0 };

    if (HasMasterKey())
    {
        XphpGetL1Key(&input.Key);
        status = XphpDeviceIoControl(
            KPH_TERMINATEPROCESS,
            &input,
            sizeof(input)
            );
    }
    else
    {
        status = XphpWithKey(KphKeyLevel2, XphpTerminateProcessContinuation, &input);
    }
    // Check if we're trying to terminate the current process, because kernel-mode can't do it.
    if (status == STATUS_CANT_TERMINATE_SELF)
    {
        RtlExitUserProcess(ExitStatus);
    }

    return status;
}

NTSTATUS NTAPI XphReadVirtualMemoryUnsafe(
    _In_opt_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Inout_opt_ PSIZE_T NumberOfBytesRead
    )
{
    KPH_READ_VIRTUAL_MEMORY_UNSAFE_INPUT input = { ProcessHandle, BaseAddress, Buffer, BufferSize, NumberOfBytesRead, 0 };

    if (HasMasterKey())
    {
        XphpGetL1Key(&input.Key);
        return XphpDeviceIoControl(
            KPH_READVIRTUALMEMORYUNSAFE,
            &input,
            sizeof(input)
            );
    }
    else
    {
        return XphpWithKey(KphKeyLevel2, XphpReadVirtualMemoryUnsafeContinuation, &input);
    }
}

NTSTATUS NTAPI XphQueryInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength,
    _Inout_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass;
        PVOID ProcessInformation;
        ULONG ProcessInformationLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength, ReturnLength };

    return XphpDeviceIoControl(
        KPH_QUERYINFORMATIONPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphSetInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _In_reads_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass;
        PVOID ProcessInformation;
        ULONG ProcessInformationLength;
    } input = { ProcessHandle, ProcessInformationClass, ProcessInformation, ProcessInformationLength };

    return XphpDeviceIoControl(
        KPH_SETINFORMATIONPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphOpenThread(
    _Inout_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    )
{
    KPH_OPEN_THREAD_INPUT input = { ThreadHandle, DesiredAccess, ClientId, 0 };

    if ((DesiredAccess & KPH_THREAD_READ_ACCESS) == DesiredAccess || HasMasterKey())
    {
        XphpGetL1Key(&input.Key);
        return XphpDeviceIoControl(
            KPH_OPENTHREAD,
            &input,
            sizeof(input)
            );
    }
    else
    {
        return XphpWithKey(KphKeyLevel2, XphpOpenThreadContinuation, &input);
    }
}

NTSTATUS NTAPI XphOpenThreadProcess(
    _In_ HANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE ProcessHandle
    )
{
    struct
    {
        HANDLE ThreadHandle;
        ACCESS_MASK DesiredAccess;
        PHANDLE ProcessHandle;
    } input = { ThreadHandle, DesiredAccess, ProcessHandle };

    return XphpDeviceIoControl(
        KPH_OPENTHREADPROCESS,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphCaptureStackBackTraceThread(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG FramesToSkip,
    _In_ ULONG FramesToCapture,
    _Out_writes_(FramesToCapture) PVOID *BackTrace,
    _Inout_opt_ PULONG CapturedFrames,
    _Inout_opt_ PULONG BackTraceHash
    )
{
    struct
    {
        HANDLE ThreadHandle;
        ULONG FramesToSkip;
        ULONG FramesToCapture;
        PVOID *BackTrace;
        PULONG CapturedFrames;
        PULONG BackTraceHash;
    } input = { ThreadHandle, FramesToSkip, FramesToCapture, BackTrace, CapturedFrames, BackTraceHash };

    return XphpDeviceIoControl(
        KPH_CAPTURESTACKBACKTRACETHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Inout_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ThreadHandle;
        KPH_THREAD_INFORMATION_CLASS ThreadInformationClass;
        PVOID ThreadInformation;
        ULONG ThreadInformationLength;
        PULONG ReturnLength;
    } input = { ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength, ReturnLength };

    return XphpDeviceIoControl(
        KPH_QUERYINFORMATIONTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphSetInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _In_reads_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength
    )
{
    struct
    {
        HANDLE ThreadHandle;
        KPH_THREAD_INFORMATION_CLASS ThreadInformationClass;
        PVOID ThreadInformation;
        ULONG ThreadInformationLength;
    } input = { ThreadHandle, ThreadInformationClass, ThreadInformation, ThreadInformationLength };

    return XphpDeviceIoControl(
        KPH_SETINFORMATIONTHREAD,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphEnumerateProcessHandles(
    _In_ HANDLE ProcessHandle,
    _Out_writes_bytes_(BufferLength) PVOID Buffer,
    _In_opt_ ULONG BufferLength,
    _Inout_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        PVOID Buffer;
        ULONG BufferLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, Buffer, BufferLength, ReturnLength };

    return XphpDeviceIoControl(
        KPH_ENUMERATEPROCESSHANDLES,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphEnumerateProcessHandles2(
    _In_ HANDLE ProcessHandle,
    _Out_ PKPH_PROCESS_HANDLE_INFORMATION *Handles
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize = 2048;

    buffer = malloc(bufferSize);

    while (TRUE)
    {
        status = XphEnumerateProcessHandles(
            ProcessHandle,
            buffer,
            bufferSize,
            &bufferSize
            );

        if (status == STATUS_BUFFER_TOO_SMALL)
        {
            free(buffer);
            buffer = malloc(bufferSize);
        }
        else
        {
            break;
        }
    }

    if (!NT_SUCCESS(status))
    {
        free(buffer);
        return status;
    }

    *Handles = buffer;

    return status;
}

NTSTATUS NTAPI XphQueryInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _Out_writes_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength,
    _Inout_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        HANDLE Handle;
        KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass;
        PVOID ObjectInformation;
        ULONG ObjectInformationLength;
        PULONG ReturnLength;
    } input = { ProcessHandle, Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength, ReturnLength };

    return XphpDeviceIoControl(
        KPH_QUERYINFORMATIONOBJECT,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphSetInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _In_reads_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength
    )
{
    struct
    {
        HANDLE ProcessHandle;
        HANDLE Handle;
        KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass;
        PVOID ObjectInformation;
        ULONG ObjectInformationLength;
    } input = { ProcessHandle, Handle, ObjectInformationClass, ObjectInformation, ObjectInformationLength };

    return XphpDeviceIoControl(
        KPH_SETINFORMATIONOBJECT,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphOpenDriver(
    _Inout_ PHANDLE DriverHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
    )
{
    struct
    {
        PHANDLE DriverHandle;
        ACCESS_MASK DesiredAccess;
        POBJECT_ATTRIBUTES ObjectAttributes;
    } input = { DriverHandle, DesiredAccess, ObjectAttributes };

    return XphpDeviceIoControl(
        KPH_OPENDRIVER,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphQueryInformationDriver(
    _In_ HANDLE DriverHandle,
    _In_ DRIVER_INFORMATION_CLASS DriverInformationClass,
    _Out_writes_bytes_(DriverInformationLength) PVOID DriverInformation,
    _In_ ULONG DriverInformationLength,
    _Inout_opt_ PULONG ReturnLength
    )
{
    struct
    {
        HANDLE DriverHandle;
        DRIVER_INFORMATION_CLASS DriverInformationClass;
        PVOID DriverInformation;
        ULONG DriverInformationLength;
        PULONG ReturnLength;
    } input = { DriverHandle, DriverInformationClass, DriverInformation, DriverInformationLength, ReturnLength };

    return XphpDeviceIoControl(
        KPH_QUERYINFORMATIONDRIVER,
        &input,
        sizeof(input)
        );
}

NTSTATUS NTAPI XphpDeviceIoControl(
    _In_ ULONG XphControlCode,
    _In_ PVOID InBuffer,
    _In_ ULONG InBufferLength
    )
{
    IO_STATUS_BLOCK iosb;

    return NtDeviceIoControlFile(
        PhXphHandle,
        NULL,
        NULL,
        NULL,
        &iosb,
        XphControlCode,
        InBuffer,
        InBufferLength,
        NULL,
        0
        );
}

VOID NTAPI XphpWithKeyApcRoutine(
    _In_ PVOID ApcContext,
    _In_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_ ULONG Reserved
    )
{
    PKPHP_RETRIEVE_KEY_CONTEXT context = CONTAINING_RECORD(IoStatusBlock, KPHP_RETRIEVE_KEY_CONTEXT, Iosb);
    KPH_KEY key = PtrToUlong(ApcContext);

    if (context->Continuation != XphpGetL1KeyContinuation &&
        context->Continuation != XphpOpenProcessContinuation &&
        context->Continuation != XphpOpenProcessTokenContinuation &&
        context->Continuation != XphpTerminateProcessContinuation &&
        context->Continuation != XphpReadVirtualMemoryUnsafeContinuation &&
        context->Continuation != XphpOpenThreadContinuation)
    {
        RtlRaiseStatus(STATUS_ACCESS_DENIED);
        context->Status = STATUS_ACCESS_DENIED;
        return;
    }

    context->Status = context->Continuation(key, context->Context);
}

NTSTATUS NTAPI XphpWithKey(
    _In_ KPH_KEY_LEVEL KeyLevel,
    _In_ PKPHP_WITH_KEY_CONTINUATION Continuation,
    _In_ PVOID Context
    )
{
    NTSTATUS status;
    struct
    {
        KPH_KEY_LEVEL KeyLevel;
    } input = { KeyLevel };
    KPHP_RETRIEVE_KEY_CONTEXT context;

    context.Continuation = Continuation;
    context.Context = Context;
    context.Status = STATUS_UNSUCCESSFUL;

    status = NtDeviceIoControlFile(
        PhXphHandle,
        NULL,
        XphpWithKeyApcRoutine,
        NULL,
        &context.Iosb,
        KPH_RETRIEVEKEY,
        &input,
        sizeof(input),
        NULL,
        0
        );

    NtTestAlert();

    if (!NT_SUCCESS(status))
        return status;

    return context.Status;
}

NTSTATUS NTAPI XphpGetL1KeyContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPHP_GET_L1_KEY_CONTEXT context = Context;

    *context->Key = Key;
    PhXphL1Key = Key;

    return STATUS_SUCCESS;
}

NTSTATUS NTAPI XphpGetL1Key(
    _Inout_ PKPH_KEY Key
    )
{
    KPHP_GET_L1_KEY_CONTEXT context;

    if (PhXphL1Key)
    {
        *Key = PhXphL1Key;
        return STATUS_SUCCESS;
    }

    context.Key = Key;

    return XphpWithKey(KphKeyLevel1, XphpGetL1KeyContinuation, &context);
}

NTSTATUS NTAPI XphpOpenProcessContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPH_OPEN_PROCESS_INPUT input = Context;

    input->Key = Key;

    return XphpDeviceIoControl(
        KPH_OPENPROCESS,
        input,
        sizeof(*input)
        );
}

NTSTATUS NTAPI XphpOpenProcessTokenContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPH_OPEN_PROCESS_TOKEN_INPUT input = Context;

    input->Key = Key;

    return XphpDeviceIoControl(
        KPH_OPENPROCESSTOKEN,
        input,
        sizeof(*input)
        );
}

NTSTATUS NTAPI XphpTerminateProcessContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPH_TERMINATE_PROCESS_INPUT input = Context;

    input->Key = Key;

    return XphpDeviceIoControl(
        KPH_TERMINATEPROCESS,
        input,
        sizeof(*input)
        );
}

NTSTATUS NTAPI XphpReadVirtualMemoryUnsafeContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPH_READ_VIRTUAL_MEMORY_UNSAFE_INPUT input = Context;

    input->Key = Key;

    return XphpDeviceIoControl(
        KPH_READVIRTUALMEMORYUNSAFE,
        input,
        sizeof(*input)
        );
}

NTSTATUS NTAPI XphpOpenThreadContinuation(
    _In_ KPH_KEY Key,
    _In_ PVOID Context
    )
{
    PKPH_OPEN_PROCESS_INPUT input = Context;

    input->Key = Key;

    return XphpDeviceIoControl(
        KPH_OPENTHREAD,
        input,
        sizeof(*input)
        );
}
