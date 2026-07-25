#include "stdafx.h"

// ProcessHacker library includes - MUST come before Windows.h to avoid macro conflicts
#define _PHLIB_
#define CINTERFACE
#define COBJMACROS

#include <ph.h>
#include <guisup.h>
#include <kphuser.h>
#include <svcsup.h>
#include <lsasup.h>
#include <phnative.h>
#include <phutil.h>
#include <symprv.h>
extern "C" {
#include <phconsole.h>
}

// Windows headers after ProcessHacker
#include <stdio.h>

// CVariant includes (STL-only, no Qt)
#include "../TaskExplorer/Common/Types.h"
#include "../TaskExplorer/Common/VariantDefs.h"
//#include "../TaskExplorer/Common/Strings.h"
#include "../TaskExplorer/Common/Buffer.h"
#include "../TaskExplorer/Common/Variant.h"

// Type aliases for compatibility
typedef unsigned long long quint64;
typedef unsigned long quint32;
typedef unsigned short quint16;
typedef unsigned char quint8;

typedef struct _PH_RUNAS_SERVICE_PARAMETERS
{
    ULONG ProcessId;
    PCWSTR UserName;
    PCWSTR Password;
    ULONG LogonType;
    ULONG SessionId;
    PCWSTR CurrentDirectory;
    PCWSTR CommandLine;
    PCWSTR FileName;
    PCWSTR DesktopName;
    BOOLEAN UseLinkedToken;
    PCWSTR ServiceName;
    BOOLEAN CreateSuspendedProcess;
    BOOLEAN CreateUIAccessProcess;
} PH_RUNAS_SERVICE_PARAMETERS, *PPH_RUNAS_SERVICE_PARAMETERS;

// Global variables
static HANDLE g_PipeHandle = INVALID_HANDLE_VALUE;
static ULONGLONG g_LastActivity = 0;
static ULONG g_Timeout = 5000; // milliseconds
static BOOLEAN g_Running = TRUE;

// Service mode variables
static SERVICE_STATUS g_ServiceStatus = { 0 };
static SERVICE_STATUS_HANDLE g_ServiceStatusHandle = NULL;
static HANDLE g_ServiceStopEvent = NULL;
static WCHAR g_ServiceName[256] = L"TaskHelperSvc";
static BOOLEAN g_ServiceMode = FALSE;

// Forward declarations
BOOLEAN InitializeProcessHacker(VOID);
CVariant ProcessCommand(const CVariant& Request, ULONG pid);
BOOLEAN SendCVariant(HANDLE hPipe, const CVariant& variant);
BOOLEAN RecvCVariant(HANDLE hPipe, CVariant& variant);
ULONGLONG GetTickCount64Compat(VOID);

// Service mode functions
VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv);
VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode);
BOOLEAN RunServiceMode(PCWSTR ServiceName);
BOOLEAN InstallAndRunService(PCWSTR ServiceName);
VOID ServiceWorkerThread(PVOID Parameter);
DWORD WINAPI ClientHandlerThread(LPVOID lpParam);

// Shared pipe server function
VOID RunPipeServer(PCWSTR PipeName, ULONG Timeout, HANDLE StopEvent);

// Worker function implementations (used by ProcessCommand)
NTSTATUS ExecTaskActionProcess(HANDLE ProcessId, PCSTR Action, PVOID Data, ULONG DataSize);
NTSTATUS ExecTaskActionThread(HANDLE ProcessId, HANDLE ThreadId, PCSTR Action, PVOID Data, ULONG DataSize);
NTSTATUS ExecServiceAction(PCWSTR ServiceName, PCSTR Action, PVOID Data, ULONG DataSize);

// RunAs support functions
VOID PhpSplitUserName(_In_ PWSTR UserName, _Out_ PPH_STRING *DomainPart, _Out_ PPH_STRING *UserPart);
NTSTATUS PhSvcpValidateRunAsServiceParameters(_In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters);
NTSTATUS PhInvokeRunAsService(_In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters);

// RunAsTrustedInstaller support functions
NTSTATUS StartTrustedInstallerService(_Out_ PULONG ProcessId);
NTSTATUS EnablePrivilege(_In_ PCWSTR PrivilegeName);
NTSTATUS ImpersonateSystem(VOID);
NTSTATUS RunAsTrustedInstaller(_In_ PCWSTR CommandLine);

// Helper function implementations

NTSTATUS PhSvcpValidateRunAsServiceParameters(
    _In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters
    )
{
    if ((!Parameters->UserName || !Parameters->Password) && !Parameters->ProcessId)
        return STATUS_INVALID_PARAMETER_MIX;
    if (!Parameters->FileName && !Parameters->CommandLine)
        return STATUS_INVALID_PARAMETER_MIX;
    if (!Parameters->ServiceName)
        return STATUS_INVALID_PARAMETER;

    return STATUS_SUCCESS;
}

VOID PhpSplitUserName(
    _In_ PWSTR UserName,
    _Out_ PPH_STRING *DomainPart,
    _Out_ PPH_STRING *UserPart
    )
{
    PH_STRINGREF userName;
    PH_STRINGREF domainPart;
    PH_STRINGREF userPart;

    PhInitializeStringRefLongHint(&userName, UserName);

    if (PhSplitStringRefAtChar(&userName, '\\', &domainPart, &userPart))
    {
        *DomainPart = PhCreateString2(&domainPart);
        *UserPart = PhCreateString2(&userPart);
    }
    else
    {
        *DomainPart = NULL;
        *UserPart = PhCreateString2(&userName);
    }
}

_Success_(return)
BOOLEAN PhRunAsGetLogonSid(
    _In_ HANDLE ProcessHandle,
    _Out_ PSID* UserSid,
    _Out_ PSID* LogonSid
)
{
    PSID userSid = NULL;
    PSID groupSid = NULL;
    HANDLE tokenHandle;

    if (NT_SUCCESS(PhOpenProcessToken(
        ProcessHandle,
        TOKEN_QUERY,
        &tokenHandle
    )))
    {
        PTOKEN_GROUPS tokenGroups = NULL;
        PH_TOKEN_USER tokenUser;

        if (NT_SUCCESS(PhGetTokenUser(tokenHandle, &tokenUser)))
        {
            userSid = PhAllocateCopy(tokenUser.User.Sid, PhLengthSid((PCSID)tokenUser.User.Sid));
        }

        if (NT_SUCCESS(PhGetTokenGroups(
            tokenHandle,
            &tokenGroups
        )))
        {
            for (ULONG i = 0; i < tokenGroups->GroupCount; i++)
            {
                PSID_AND_ATTRIBUTES group = &tokenGroups->Groups[i];

                if (FlagOn(group->Attributes, SE_GROUP_LOGON_ID))
                {
                    groupSid = PhAllocateCopy(group->Sid, PhLengthSid((PCSID)group->Sid));
                    break;
                }
            }

            PhFree(tokenGroups);
        }
    }

    if (userSid && groupSid)
    {
        *UserSid = userSid;
        *LogonSid = groupSid;
        return TRUE;
    }

    if (userSid)
        PhFree(userSid);
    if (groupSid)
        PhFree(groupSid);
    return FALSE;
}

NTSTATUS PhRunAsUpdateDesktop(
    _In_ PSID UserSid
)
{
    NTSTATUS status;
    HDESK desktopHandle;

    if (desktopHandle = OpenDesktop(
        L"Default",
        0,
        FALSE,
        READ_CONTROL | WRITE_DAC | DESKTOP_READOBJECTS | DESKTOP_WRITEOBJECTS
    ))
    {
        ULONG i;
        BOOLEAN currentDaclPresent;
        BOOLEAN currentDaclDefaulted;
        PACL currentDacl;
        PACE_HEADER currentAce;
        ULONG newDaclLength;
        PACL newDacl;
        SECURITY_DESCRIPTOR newSecurityDescriptor;
        PSECURITY_DESCRIPTOR currentSecurityDescriptor;

        status = PhGetObjectSecurity(
            desktopHandle,
            DACL_SECURITY_INFORMATION,
            &currentSecurityDescriptor
        );

        if (NT_SUCCESS(status))
        {
            if (!NT_SUCCESS(PhGetDaclSecurityDescriptor(
                currentSecurityDescriptor,
                &currentDaclPresent,
                &currentDacl,
                &currentDaclDefaulted
            )))
            {
                currentDaclPresent = FALSE;
            }

            newDaclLength = sizeof(ACL) + FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) + PhLengthSid((PCSID)UserSid);

            if (currentDaclPresent && currentDacl)
                newDaclLength += currentDacl->AclSize - sizeof(ACL);

            newDacl = (PACL)PhAllocateStack(newDaclLength);

            if (!newDacl)
            {
                status = STATUS_NO_MEMORY;
                goto CleanupExit;
            }

            RtlZeroMemory(newDacl, newDaclLength);

            status = PhCreateAcl(newDacl, newDaclLength, ACL_REVISION);

            if (!NT_SUCCESS(status))
                goto CleanupExit;

            // Add the existing DACL entries.

            if (currentDaclPresent && currentDacl)
            {
                for (i = 0; i < currentDacl->AceCount; i++)
                {
                    if (NT_SUCCESS(PhGetAce(currentDacl, i, (PVOID*)&currentAce)))
                    {
                        if (currentAce->AceType == ACCESS_ALLOWED_ACE_TYPE)
                        {
                            PSID aceSid = (PSID)&((PACCESS_ALLOWED_ACE)currentAce)->SidStart;

                            if (PhEqualSid((PCSID)aceSid, (PCSID)UserSid))
                            {
                                if (((PACCESS_ALLOWED_ACE)currentAce)->Mask == DESKTOP_ALL_ACCESS)
                                    continue;
                            }
                        }

                        RtlAddAce(newDacl, ACL_REVISION, ULONG_MAX, currentAce, currentAce->AceSize);
                    }
                }
            }

            // Allow access for the user.

            if (NT_SUCCESS(status))
            {
                status = PhAddAccessAllowedAce(newDacl, ACL_REVISION, DESKTOP_ALL_ACCESS, (PCSID)UserSid);
            }

            // Set the security descriptor of the new token.

            if (NT_SUCCESS(status))
            {
                status = PhCreateSecurityDescriptor(&newSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
            }

            if (NT_SUCCESS(status))
            {
                status = PhSetDaclSecurityDescriptor(&newSecurityDescriptor, TRUE, newDacl, FALSE);
            }

            if (NT_SUCCESS(status))
            {
                assert(RtlValidSecurityDescriptor(&newSecurityDescriptor));

                status = PhSetObjectSecurity(desktopHandle, DACL_SECURITY_INFORMATION, &newSecurityDescriptor);
            }

            PhFreeStack(newDacl);
        }

    CleanupExit:
        CloseDesktop(desktopHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    return status;
}

NTSTATUS PhRunAsUpdateWindowStation(
    _In_opt_ PSID UserSid,
    _In_opt_ PSID LogonSid
)
{
    NTSTATUS status;
    HWINSTA wsHandle;

    if (wsHandle = OpenWindowStation(
        L"WinSta0",
        FALSE,
        READ_CONTROL | WRITE_DAC
    ))
    {
        ULONG i;
        BOOLEAN currentDaclPresent;
        BOOLEAN currentDaclDefaulted;
        PACL currentDacl;
        PACE_HEADER currentAce;
        ULONG newDaclLength;
        PACL newDacl;
        SECURITY_DESCRIPTOR newSecurityDescriptor;
        PSECURITY_DESCRIPTOR currentSecurityDescriptor;

        status = PhGetObjectSecurity(
            wsHandle,
            DACL_SECURITY_INFORMATION,
            &currentSecurityDescriptor
        );

        if (NT_SUCCESS(status))
        {
            if (!NT_SUCCESS(PhGetDaclSecurityDescriptor(
                currentSecurityDescriptor,
                &currentDaclPresent,
                &currentDacl,
                &currentDaclDefaulted
            )))
            {
                currentDaclPresent = FALSE;
            }

            newDaclLength = (sizeof(ACL) + FIELD_OFFSET(ACCESS_ALLOWED_ACE, SidStart) * 3) +
                (UserSid ? PhLengthSid((PCSID)UserSid) : 0) + (LogonSid ? PhLengthSid((PCSID)LogonSid) : 0);

            if (currentDaclPresent && currentDacl)
                newDaclLength += currentDacl->AclSize - sizeof(ACL);

            newDacl = (PACL)PhAllocate(newDaclLength);
            PhCreateAcl(newDacl, newDaclLength, ACL_REVISION);

            // Add the existing DACL entries.

            if (currentDaclPresent && currentDacl)
            {
                for (i = 0; i < currentDacl->AceCount; i++)
                {
                    if (NT_SUCCESS(PhGetAce(currentDacl, i, (PVOID*)&currentAce)))
                    {
                        if (currentAce->AceType == ACCESS_ALLOWED_ACE_TYPE)
                        {
                            PSID aceSid = (PSID)&((PACCESS_ALLOWED_ACE)currentAce)->SidStart;

                            if (UserSid && PhEqualSid((PCSID)aceSid, (PCSID)UserSid))
                            {
                                if (((PACCESS_ALLOWED_ACE)currentAce)->Mask == (WINSTA_ACCESSCLIPBOARD | WINSTA_ACCESSGLOBALATOMS))
                                    continue;
                            }

                            if (LogonSid && PhEqualSid((PCSID)aceSid, (PCSID)LogonSid))
                            {
                                if (((PACCESS_ALLOWED_ACE)currentAce)->Mask == WINSTA_ALL_ACCESS)
                                    continue;
                            }
                        }

                        RtlAddAce(newDacl, ACL_REVISION, ULONG_MAX, currentAce, currentAce->AceSize);
                    }
                }
            }

            if (NT_SUCCESS(status))
            {
                if (UserSid)
                {
                    PhAddAccessAllowedAce(
                        newDacl,
                        ACL_REVISION,
                        WINSTA_ACCESSCLIPBOARD | WINSTA_ACCESSGLOBALATOMS,
                        (PCSID)UserSid
                    );

                    //PhAddAccessAllowedAce(
                    //    newDacl,
                    //    ACL_REVISION,
                    //    WINSTA_ENUMDESKTOPS | WINSTA_READATTRIBUTES | WINSTA_ACCESSGLOBALATOMS |
                    //    WINSTA_EXITWINDOWS | WINSTA_ENUMERATE | WINSTA_READSCREEN | READ_CONTROL,
                    //    UserSid
                    //    );
                }

                if (UserSid)
                {
                    PhAddAccessAllowedAceEx(
                        newDacl,
                        ACL_REVISION,
                        OBJECT_INHERIT_ACE | CONTAINER_INHERIT_ACE | INHERIT_ONLY_ACE,
                        GENERIC_ALL,
                        (PCSID)LogonSid
                    );
                    PhAddAccessAllowedAceEx(
                        newDacl,
                        ACL_REVISION,
                        NO_PROPAGATE_INHERIT_ACE,
                        WINSTA_ALL_ACCESS,
                        (PCSID)LogonSid
                    );
                }

                // Set the security descriptor of the new token.

                status = PhCreateSecurityDescriptor(&newSecurityDescriptor, SECURITY_DESCRIPTOR_REVISION);
            }

            if (NT_SUCCESS(status))
            {
                status = PhSetDaclSecurityDescriptor(&newSecurityDescriptor, TRUE, newDacl, FALSE);
            }

            if (NT_SUCCESS(status))
            {
                assert(RtlValidSecurityDescriptor(&newSecurityDescriptor));

                status = PhSetObjectSecurity(wsHandle, DACL_SECURITY_INFORMATION, &newSecurityDescriptor);
            }
        }

        CloseWindowStation(wsHandle);
    }
    else
    {
        status = PhGetLastWin32ErrorAsNtStatus();
    }

    return status;
}

NTSTATUS PhInvokeRunAsService(
    _In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters
    )
{
    NTSTATUS status;
    PPH_STRING domainName;
    PPH_STRING userName;
    PH_CREATE_PROCESS_AS_USER_INFO createInfo;
    HANDLE newProcessHandle = NULL;
    ULONG flags;

    if (Parameters->UserName)
    {
        PhpSplitUserName((PWSTR)Parameters->UserName, &domainName, &userName);
    }
    else
    {
        domainName = NULL;
        userName = NULL;
    }

    memset(&createInfo, 0, sizeof(PH_CREATE_PROCESS_AS_USER_INFO));
    createInfo.ApplicationName = Parameters->FileName;
    createInfo.CommandLine = Parameters->CommandLine;
    createInfo.CurrentDirectory = Parameters->CurrentDirectory;
    createInfo.DomainName = PhGetString(domainName);
    createInfo.UserName = PhGetString(userName);
    createInfo.Password = Parameters->Password;
    createInfo.LogonType = Parameters->LogonType;
    createInfo.SessionId = Parameters->SessionId;
    createInfo.DesktopName = Parameters->DesktopName;

    flags = PH_CREATE_PROCESS_SET_SESSION_ID | PH_CREATE_PROCESS_DEFAULT_ERROR_MODE;

    if (Parameters->ProcessId)
    {
        createInfo.ProcessIdWithToken = UlongToHandle(Parameters->ProcessId);
        flags |= PH_CREATE_PROCESS_USE_PROCESS_TOKEN;
    }

    if (Parameters->UseLinkedToken)
        flags |= PH_CREATE_PROCESS_USE_LINKED_TOKEN;
    if (Parameters->CreateSuspendedProcess)
        flags |= PH_CREATE_PROCESS_SUSPENDED;
    if (Parameters->CreateUIAccessProcess)
        flags |= PH_CREATE_PROCESS_SET_UIACCESS;

    status = PhCreateProcessAsUser(
        &createInfo,
        flags,
        NULL,
        NULL,
        &newProcessHandle,
        NULL
    );

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

        if (!Parameters->CreateSuspendedProcess)
        {
            status = PhGetProcessBasicInformation(newProcessHandle, &basicInfo);

            if (NT_SUCCESS(status))
            {
                AllowSetForegroundWindow(HandleToUlong(basicInfo.UniqueProcessId));
            }

            PhConsoleSetForeground(newProcessHandle, TRUE);

            PhResumeProcess(newProcessHandle);
        }
    }

CleanupExit:
    if (newProcessHandle) NtClose(newProcessHandle);
    if (domainName) PhDereferenceObject(domainName);
    if (userName) PhDereferenceObject(userName);

    return status;
}

// RunAsTrustedInstaller implementation

NTSTATUS EnablePrivilege(_In_ PCWSTR PrivilegeName)
{
    HANDLE tokenHandle;
    NTSTATUS status;

    if (!NT_SUCCESS(status = NtOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
        return status;

    LUID privilegeLuid;
    if (!LookupPrivilegeValueW(NULL, PrivilegeName, &privilegeLuid))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        NtClose(tokenHandle);
        return status;
    }

    TOKEN_PRIVILEGES privileges;
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Luid = privilegeLuid;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (!AdjustTokenPrivileges(tokenHandle, FALSE, &privileges, 0, NULL, NULL))
        status = PhGetLastWin32ErrorAsNtStatus();
    else
        status = STATUS_SUCCESS;

    NtClose(tokenHandle);
    return status;
}

NTSTATUS ImpersonateSystem(VOID)
{
    NTSTATUS status;
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    HANDLE dupTokenHandle = NULL;
    PROCESSENTRY32W processEntry;
    HANDLE snapshot;
    ULONG systemPid = 0;

    // Find winlogon.exe process (runs as SYSTEM)
    snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return PhGetLastWin32ErrorAsNtStatus();

    processEntry.dwSize = sizeof(PROCESSENTRY32W);
    if (Process32FirstW(snapshot, &processEntry))
    {
        do
        {
            if (_wcsicmp(processEntry.szExeFile, L"winlogon.exe") == 0)
            {
                systemPid = processEntry.th32ProcessID;
                break;
            }
        } while (Process32NextW(snapshot, &processEntry));
    }
    CloseHandle(snapshot);

    if (systemPid == 0)
        return STATUS_NOT_FOUND;

    // Open winlogon.exe process
    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, UlongToHandle(systemPid))))
        return status;

    // Get token from winlogon.exe
    if (!NT_SUCCESS(status = NtOpenProcessToken(processHandle, MAXIMUM_ALLOWED, &tokenHandle)))
    {
        NtClose(processHandle);
        return status;
    }

    // Duplicate token
    SECURITY_ATTRIBUTES tokenAttributes;
    tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    tokenAttributes.lpSecurityDescriptor = NULL;
    tokenAttributes.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(tokenHandle, MAXIMUM_ALLOWED, &tokenAttributes,
        SecurityImpersonation, TokenImpersonation, &dupTokenHandle))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        NtClose(tokenHandle);
        NtClose(processHandle);
        return status;
    }

    // Impersonate
    if (!ImpersonateLoggedOnUser(dupTokenHandle))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        CloseHandle(dupTokenHandle);
        NtClose(tokenHandle);
        NtClose(processHandle);
        return status;
    }

    CloseHandle(dupTokenHandle);
    NtClose(tokenHandle);
    NtClose(processHandle);
    return STATUS_SUCCESS;
}

NTSTATUS StartTrustedInstallerService(_Out_ PULONG ProcessId)
{
    SC_HANDLE scManager = NULL;
    SC_HANDLE serviceHandle = NULL;
    SERVICE_STATUS_PROCESS statusBuffer;
    DWORD bytesNeeded;
    NTSTATUS status = STATUS_SUCCESS;
    ULONG attempts = 10;

    *ProcessId = 0;

    scManager = OpenSCManagerW(NULL, SERVICES_ACTIVE_DATABASE, GENERIC_EXECUTE);
    if (!scManager)
        return PhGetLastWin32ErrorAsNtStatus();

    serviceHandle = OpenServiceW(scManager, L"TrustedInstaller", GENERIC_READ | GENERIC_EXECUTE);
    if (!serviceHandle)
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        CloseServiceHandle(scManager);
        return status;
    }

    // Start service if not running and wait for it to start
    while (attempts-- > 0)
    {
        if (!QueryServiceStatusEx(serviceHandle, SC_STATUS_PROCESS_INFO,
            (LPBYTE)&statusBuffer, sizeof(SERVICE_STATUS_PROCESS), &bytesNeeded))
        {
            status = PhGetLastWin32ErrorAsNtStatus();
            break;
        }

        if (statusBuffer.dwCurrentState == SERVICE_STOPPED)
        {
            if (!StartServiceW(serviceHandle, 0, NULL))
            {
                status = PhGetLastWin32ErrorAsNtStatus();
                break;
            }
        }

        if (statusBuffer.dwCurrentState == SERVICE_START_PENDING ||
            statusBuffer.dwCurrentState == SERVICE_STOP_PENDING)
        {
            Sleep(statusBuffer.dwWaitHint ? statusBuffer.dwWaitHint : 1000);
            continue;
        }

        if (statusBuffer.dwCurrentState == SERVICE_RUNNING)
        {
            *ProcessId = statusBuffer.dwProcessId;
            status = STATUS_SUCCESS;
            break;
        }

        Sleep(1000);
    }

    CloseServiceHandle(serviceHandle);
    CloseServiceHandle(scManager);

    if (*ProcessId == 0)
        return STATUS_UNSUCCESSFUL;

    return status;
}

NTSTATUS RunAsTrustedInstaller(_In_ PCWSTR CommandLine)
{
    NTSTATUS status;
    ULONG trustedInstallerPid = 0;
    HANDLE processHandle = NULL;
    HANDLE tokenHandle = NULL;
    HANDLE dupTokenHandle = NULL;
    STARTUPINFOW startupInfo;
    PROCESS_INFORMATION processInfo;
    PWSTR commandLineCopy = NULL;

    // Enable required privileges
    EnablePrivilege(SE_DEBUG_NAME);
    EnablePrivilege(SE_IMPERSONATE_NAME);

    // Impersonate SYSTEM
    if (!NT_SUCCESS(status = ImpersonateSystem()))
        return status;

    // Start TrustedInstaller service
    if (!NT_SUCCESS(status = StartTrustedInstallerService(&trustedInstallerPid)))
    {
        RevertToSelf();
        return status;
    }

    // Open TrustedInstaller process
    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE | PROCESS_QUERY_INFORMATION,
        UlongToHandle(trustedInstallerPid))))
    {
        RevertToSelf();
        return status;
    }

    // Get token from TrustedInstaller
    if (!NT_SUCCESS(status = NtOpenProcessToken(processHandle, MAXIMUM_ALLOWED, &tokenHandle)))
    {
        NtClose(processHandle);
        RevertToSelf();
        return status;
    }

    // Duplicate token
    SECURITY_ATTRIBUTES tokenAttributes;
    tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    tokenAttributes.lpSecurityDescriptor = NULL;
    tokenAttributes.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(tokenHandle, MAXIMUM_ALLOWED, &tokenAttributes,
        SecurityImpersonation, TokenImpersonation, &dupTokenHandle))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        NtClose(tokenHandle);
        NtClose(processHandle);
        RevertToSelf();
        return status;
    }

    // Create process with TrustedInstaller token
    ZeroMemory(&startupInfo, sizeof(STARTUPINFOW));
    startupInfo.cb = sizeof(STARTUPINFOW);
    startupInfo.lpDesktop = (PWSTR)L"Winsta0\\Default";
    ZeroMemory(&processInfo, sizeof(PROCESS_INFORMATION));

    // Make a writable copy of command line
    size_t cmdLen = wcslen(CommandLine) + 1;
    commandLineCopy = (PWSTR)PhAllocate(cmdLen * sizeof(WCHAR));
    wcscpy_s(commandLineCopy, cmdLen, CommandLine);

    if (!CreateProcessWithTokenW(dupTokenHandle, LOGON_WITH_PROFILE, NULL, commandLineCopy,
        CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo, &processInfo))
    {
        status = PhGetLastWin32ErrorAsNtStatus();
        PhFree(commandLineCopy);
        CloseHandle(dupTokenHandle);
        NtClose(tokenHandle);
        NtClose(processHandle);
        RevertToSelf();
        return status;
    }

    CloseHandle(processInfo.hProcess);
    CloseHandle(processInfo.hThread);
    PhFree(commandLineCopy);
    CloseHandle(dupTokenHandle);
    NtClose(tokenHandle);
    NtClose(processHandle);
    RevertToSelf();

    return STATUS_SUCCESS;
}

// Shared pipe server implementation - used by both worker and service modes
VOID RunPipeServer(PCWSTR PipeName, ULONG Timeout, HANDLE StopEvent)
{
    g_LastActivity = GetTickCount64Compat();

    // Main pipe server loop - create named pipe server
    while (g_Running)
    {
        // Check for stop signal
        if (StopEvent && WaitForSingleObject(StopEvent, 0) == WAIT_OBJECT_0)
        {
            break;
        }

        // Check for timeout
        if (Timeout && (GetTickCount64Compat() - g_LastActivity > Timeout))
        {
            break;
        }

        // Create named pipe with overlapped I/O support
        HANDLE hPipe = CreateNamedPipeW(
            PipeName,
            PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            4096,
            4096,
            0,
            NULL
        );

        if (hPipe == INVALID_HANDLE_VALUE)
        {
            Sleep(1000);
            continue;
        }

        // Wait for client connection with overlapped I/O
        OVERLAPPED overlapped = { 0 };
        overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
        if (!overlapped.hEvent)
        {
            CloseHandle(hPipe);
            continue;
        }

        BOOL connected = ConnectNamedPipe(hPipe, &overlapped);
        DWORD lastError = GetLastError();

        if (!connected)
        {
            if (lastError == ERROR_IO_PENDING)
            {
                // Wait for either connection or stop signal (if provided)
                HANDLE waitHandles[2];
                DWORD numHandles = 1;
                waitHandles[0] = overlapped.hEvent;
                if (StopEvent)
                {
                    waitHandles[1] = StopEvent;
                    numHandles = 2;
                }

                DWORD waitResult = WaitForMultipleObjects(numHandles, waitHandles, FALSE, 5000);

                if (waitResult == WAIT_OBJECT_0)
                {
                    // Connection completed
                    DWORD bytesTransferred;
                    if (!GetOverlappedResult(hPipe, &overlapped, &bytesTransferred, FALSE))
                    {
                        CloseHandle(overlapped.hEvent);
                        CloseHandle(hPipe);
                        continue;
                    }
                }
                else if (waitResult == WAIT_OBJECT_0 + 1 && StopEvent)
                {
                    // Stop signal received
                    CancelIo(hPipe);
                    CloseHandle(overlapped.hEvent);
                    CloseHandle(hPipe);
                    break;
                }
                else
                {
                    // Timeout or error
                    CancelIo(hPipe);
                    CloseHandle(overlapped.hEvent);
                    CloseHandle(hPipe);
                    continue;
                }
            }
            else if (lastError != ERROR_PIPE_CONNECTED)
            {
                CloseHandle(overlapped.hEvent);
                CloseHandle(hPipe);
                Sleep(100);
                continue;
            }
        }

        CloseHandle(overlapped.hEvent);
        g_LastActivity = GetTickCount64Compat();

        // Handle client in separate thread
        DWORD threadId;
        HANDLE hThread = CreateThread(NULL, 0, ClientHandlerThread, hPipe, 0, &threadId);
        if (hThread)
        {
            CloseHandle(hThread);
        }
        else
        {
            CloseHandle(hPipe);
        }
    }
}

int main(int argc, char* argv[])
{
    WCHAR pipeName[256] = L"\\\\.\\pipe\\";
    ULONG timeout = 5000;
    BOOLEAN debugWait = FALSE;
    BOOLEAN runService = FALSE;
    BOOLEAN installService = FALSE;
    WCHAR serviceName[256] = L"TaskHelperSvc";

    // Parse command line arguments
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-wrk") == 0 && i + 1 < argc)
        {
            // Convert pipe name from argv
            MultiByteToWideChar(CP_ACP, 0, argv[i + 1], -1, pipeName + wcslen(pipeName),
                256 - (ULONG)wcslen(pipeName));
            i++;
        }
        else if (strcmp(argv[i], "-svc") == 0 && i + 1 < argc)
        {
            // Run as Windows service
            runService = TRUE;
            MultiByteToWideChar(CP_ACP, 0, argv[i + 1], -1, serviceName, 256);
            i++;
        }
        else if (strcmp(argv[i], "-runsvc") == 0 && i + 1 < argc)
        {
            // Install and run service
            installService = TRUE;
            MultiByteToWideChar(CP_ACP, 0, argv[i + 1], -1, serviceName, 256);
            i++;
        }
        else if (strcmp(argv[i], "-timeout") == 0 && i + 1 < argc)
        {
            timeout = atoi(argv[i + 1]);
            i++;
        }
        else if (strcmp(argv[i], "-dbg_wait") == 0)
        {
            debugWait = TRUE;
        }
    }

    // Handle service installation request
    if (installService)
    {
        return InstallAndRunService(serviceName) ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    g_Timeout = timeout;

    // Wait for debugger if requested
    if (debugWait)
    {
        while (!IsDebuggerPresent())
            Sleep(100);
    }

    // Initialize ProcessHacker library
    if (!InitializeProcessHacker())
    {
        return 1;
    }

    g_LastActivity = GetTickCount64Compat();

    // Handle service mode
    if (runService)
    {
        wcscpy_s(g_ServiceName, 256, serviceName);
        g_ServiceMode = TRUE;

        SERVICE_TABLE_ENTRYW serviceTable[] =
        {
            { g_ServiceName, (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
            { NULL, NULL }
        };

        if (!StartServiceCtrlDispatcherW(serviceTable))
        {
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }

    // Worker mode: Run pipe server (same as service mode, just without Windows Service registration)
    RunPipeServer(pipeName, timeout, NULL);

    return 0;
}

BOOLEAN InitializeProcessHacker(VOID)
{
    if (!NT_SUCCESS(PhInitializePhLib(L"TaskHelper")))
        return FALSE;

    KphInitialize();

    // Enable privileges
    HANDLE tokenHandle;
    if (NT_SUCCESS(PhOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
    {
        PhSetTokenPrivilege2(tokenHandle, SE_DEBUG_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_LOAD_DRIVER_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_TAKE_OWNERSHIP_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_BACKUP_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_RESTORE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_IMPERSONATE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_ASSIGNPRIMARYTOKEN_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        PhSetTokenPrivilege2(tokenHandle, SE_INCREASE_QUOTA_PRIVILEGE, SE_PRIVILEGE_ENABLED);
        NtClose(tokenHandle);
    }

    return TRUE;
}

// ============================================================================
// CVariant Protocol Implementation
// ============================================================================
// Protocol: [ULONG length][CVariant serialized data]
// Matches TaskExplorer SendXVariant/RecvXVariant

BOOLEAN SendCVariant(HANDLE hPipe, const CVariant& variant)
{
    // Serialize CVariant to CBuffer
    CBuffer buffer;
    variant.ToPacket(&buffer);

    // Send length prefix
    ULONG len = (ULONG)buffer.GetSize();
    DWORD bytesWritten;

    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent)
        return FALSE;

    if (!WriteFile(hPipe, &len, sizeof(ULONG), &bytesWritten, &overlapped))
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            if (!GetOverlappedResult(hPipe, &overlapped, &bytesWritten, TRUE))
            {
                CloseHandle(overlapped.hEvent);
                return FALSE;
            }
        }
        else
        {
            CloseHandle(overlapped.hEvent);
            return FALSE;
        }
    }

    if (bytesWritten != sizeof(ULONG))
    {
        CloseHandle(overlapped.hEvent);
        return FALSE;
    }

    // Send serialized data
    ResetEvent(overlapped.hEvent);
    if (!WriteFile(hPipe, buffer.GetBuffer(), len, &bytesWritten, &overlapped))
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            if (!GetOverlappedResult(hPipe, &overlapped, &bytesWritten, TRUE))
            {
                CloseHandle(overlapped.hEvent);
                return FALSE;
            }
        }
        else
        {
            CloseHandle(overlapped.hEvent);
            return FALSE;
        }
    }

    CloseHandle(overlapped.hEvent);

    if (bytesWritten != len)
        return FALSE;

    FlushFileBuffers(hPipe);
    return TRUE;
}

BOOLEAN RecvCVariant(HANDLE hPipe, CVariant& variant)
{
    // Read length prefix
    ULONG len = 0;
    DWORD bytesRead;

    OVERLAPPED overlapped = { 0 };
    overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!overlapped.hEvent)
        return FALSE;

    if (!ReadFile(hPipe, &len, sizeof(ULONG), &bytesRead, &overlapped))
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            if (!GetOverlappedResult(hPipe, &overlapped, &bytesRead, TRUE))
            {
                CloseHandle(overlapped.hEvent);
                return FALSE;
            }
        }
        else
        {
            CloseHandle(overlapped.hEvent);
            return FALSE;
        }
    }

    if (bytesRead != sizeof(ULONG))
    {
        CloseHandle(overlapped.hEvent);
        return FALSE;
    }

    if (len == 0 || len > 100 * 1024 * 1024) // Sanity check: max 100MB
    {
        CloseHandle(overlapped.hEvent);
        return FALSE;
    }

    // Allocate buffer for serialized data
    PVOID data = PhAllocate(len);
    if (!data)
    {
        CloseHandle(overlapped.hEvent);
        return FALSE;
    }

    // Read serialized data
    ResetEvent(overlapped.hEvent);
    if (!ReadFile(hPipe, data, len, &bytesRead, &overlapped))
    {
        if (GetLastError() == ERROR_IO_PENDING)
        {
            if (!GetOverlappedResult(hPipe, &overlapped, &bytesRead, TRUE))
            {
                PhFree(data);
                CloseHandle(overlapped.hEvent);
                return FALSE;
            }
        }
        else
        {
            PhFree(data);
            CloseHandle(overlapped.hEvent);
            return FALSE;
        }
    }

    CloseHandle(overlapped.hEvent);

    if (bytesRead != len)
    {
        PhFree(data);
        return FALSE;
    }

    // Deserialize CBuffer to CVariant
    CBuffer buffer(data, len, TRUE); // TRUE = derive from existing data
    variant.FromPacket(&buffer);

    PhFree(data);
    return TRUE;
}

// ============================================================================
// Command Processing - CVariant Helpers
// ============================================================================

CVariant GetProcessUnloadedDllsCV(HANDLE ProcessId)
{
    PVOID capturedEventTrace = NULL;
    ULONG capturedElementSize = 0;
    ULONG capturedElementCount = 0;

    if (!NT_SUCCESS(PhGetProcessUnloadedDlls(ProcessId, &capturedEventTrace, &capturedElementSize, &capturedElementCount)))
    {
        return CVariant(FALSE);
    }

    // Build CVariant list
    CVariant result;
    result.BeginList();

    PVOID currentEvent = capturedEventTrace;
    for (ULONG i = 0; i < capturedElementCount; i++)
    {
        PRTL_UNLOAD_EVENT_TRACE rtlEvent = (PRTL_UNLOAD_EVENT_TRACE)currentEvent;
        if (rtlEvent->BaseAddress)
        {
            CVariant entry;
            entry.BeginMap();
            entry.Write("Sequence", (uint32)rtlEvent->Sequence);
            entry.Write("BaseAddress", (uint64)rtlEvent->BaseAddress);
            entry.Write("Size", (uint64)rtlEvent->SizeOfImage);
            entry.Write("TimeDateStamp", (uint32)rtlEvent->TimeDateStamp);
            entry.Write("CheckSum", (uint32)rtlEvent->CheckSum);
            entry.Write("ImageName", rtlEvent->ImageName, wcslen(rtlEvent->ImageName));
            entry.Finish();

            result.WriteVariant(entry);
        }

        currentEvent = PTR_ADD_OFFSET(currentEvent, capturedElementSize);
    }

    result.Finish();
    PhFree(capturedEventTrace);

    return result;
}

CVariant GetProcessHeapsCV(HANDLE ProcessId)
{
    NTSTATUS status;
    PPH_PROCESS_DEBUG_HEAP_INFORMATION heapInfo = NULL;

    status = PhQueryProcessHeapInformation(ProcessId, &heapInfo);

    if (!NT_SUCCESS(status) || !heapInfo)
    {
        return CVariant(FALSE);
    }

    // Build CVariant list
    CVariant result;
    result.BeginList();

    for (ULONG i = 0; i < heapInfo->NumberOfHeaps; i++)
    {
        PPH_PROCESS_DEBUG_HEAP_ENTRY entry = &heapInfo->Heaps[i];

        CVariant heapEntry;
        heapEntry.BeginMap();
        heapEntry.Write("BaseAddress", (uint64)entry->BaseAddress);
        heapEntry.Write("Flags", (uint32)entry->Flags);
        heapEntry.Write("Signature", (uint32)entry->Signature);
        heapEntry.Write("HeapFrontEndType", (uint32)entry->HeapFrontEndType);
        heapEntry.Write("NumberOfEntries", (uint32)entry->NumberOfEntries);
        heapEntry.Write("BytesAllocated", (uint64)entry->BytesAllocated);
        heapEntry.Write("BytesCommitted", (uint64)entry->BytesCommitted);
        heapEntry.Finish();

        result.WriteVariant(heapEntry);
    }

    result.Finish();
    PhFree(heapInfo);

    return result;
}

CVariant ProcessCommand(const CVariant& Request, ULONG pid)
{
    CVariant Response;

    // Determine command type: simple string or map with Command/Parameters
    std::string Command;
    CVariant Parameters;

    if (Request.GetType() == VAR_TYPE_ASCII || Request.GetType() == VAR_TYPE_UTF8 || Request.GetType() == VAR_TYPE_UNICODE)
    {
        // Simple string command
        Command = Request.ToString();
    }
    else if (Request.GetType() == VAR_TYPE_MAP)
    {
        // Complex command with parameters
        CVariant cmdVar = Request.Find("Command");
        if (cmdVar.IsValid())
            Command = cmdVar.ToString();

        Parameters = Request.Find("Parameters");
    }
    else
    {
        // Unknown command format
        Response = CVariant("Unknown Command");
        return Response;
    }

    // Process commands (aligned with original TaskExplorer receiveConnection())
    if (Command == "GetProcessId")
    {
        Response = CVariant((quint64)HandleToUlong(NtCurrentProcessId()));
    }
    else if (Command == "GetProcessUnloadedDlls")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            quint64 processId = Parameters.Find("ProcessId").To<quint64>();
            Response = GetProcessUnloadedDllsCV((HANDLE)processId);
        }
        else
        {
            Response = CVariant(FALSE);
        }
    }
    else if (Command == "GetProcessHeaps")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            quint64 processId = Parameters.Find("ProcessId").To<quint64>();
            Response = GetProcessHeapsCV((HANDLE)processId);
        }
        else
        {
            Response = CVariant(FALSE);
        }
    }
    else if (Command == "ExecTaskAction")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            quint64 processId = Parameters.Find("ProcessId").To<quint64>();
            quint64 threadId = Parameters.Find("ThreadId").To<quint64>();
            std::string action = Parameters.Find("Action").ToString();
            CVariant data = Parameters.Find("Data");

            // Convert CVariant data to raw bytes for actions that need it
            PVOID pData = NULL;
            ULONG dataSize = 0;

            // Storage for different data types
            union {
                BOOLEAN boolVal;
                UCHAR ucharVal;
                ULONG ulongVal;
                ULONGLONG ulonglongVal;
                IO_PRIORITY_HINT ioPriorityVal;
            } dataStorage;

            if (data.IsValid())
            {
                if (action == "SetPriorityBoost")
                {
                    dataStorage.boolVal = data.To<BOOLEAN>();
                    pData = &dataStorage.boolVal;
                    dataSize = sizeof(BOOLEAN);
                }
                else if (action == "SetPriority")
                {
                    dataStorage.ucharVal = data.To<UCHAR>();
                    pData = &dataStorage.ucharVal;
                    dataSize = sizeof(UCHAR);
                }
                else if (action == "SetPagePriority")
                {
                    dataStorage.ulongVal = data.To<uint32>();
                    pData = &dataStorage.ulongVal;
                    dataSize = sizeof(ULONG);
                }
                else if (action == "SetIOPriority")
                {
                    dataStorage.ioPriorityVal = (IO_PRIORITY_HINT)data.To<uint32>();
                    pData = &dataStorage.ioPriorityVal;
                    dataSize = sizeof(IO_PRIORITY_HINT);
                }
                else if (action == "SetAffinityMask")
                {
                    dataStorage.ulonglongVal = data.To<uint64>();
                    pData = &dataStorage.ulonglongVal;
                    dataSize = sizeof(ULONGLONG);
                }
            }

            NTSTATUS result;
            if (threadId)
                result = ExecTaskActionThread((HANDLE)processId, (HANDLE)threadId, action.c_str(), pData, dataSize);
            else
                result = ExecTaskActionProcess((HANDLE)processId, action.c_str(), pData, dataSize);

            Response = CVariant((sint32)result);
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "ExecServiceAction")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            std::wstring serviceName = Parameters.Find("Name").ToWString();
            std::string action = Parameters.Find("Action").ToString();

            NTSTATUS result = ExecServiceAction(serviceName.c_str(), action.c_str(), NULL, 0);
            Response = CVariant((sint32)result);
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "SendMessage")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            HWND hWnd = (HWND)(ULONG_PTR)Parameters.Find("hWnd").To<quint64>();
            UINT Msg = (UINT)Parameters.Find("Msg").To<quint64>();
            WPARAM wParam = (WPARAM)Parameters.Find("wParam").To<quint64>();
            LPARAM lParam = (LPARAM)Parameters.Find("lParam").To<quint64>();
            BOOLEAN post = Parameters.Find("Post").To<BOOLEAN>();

            LRESULT result;
            if (post)
                result = PostMessageW(hWnd, Msg, wParam, lParam);
            else
                result = SendMessageW(hWnd, Msg, wParam, lParam);

            Response = CVariant((quint64)result);
        }
        else
        {
            Response = CVariant((quint64)0);
        }
    }
    else if (Command == "FreeMemory")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            SYSTEM_MEMORY_LIST_COMMAND command = (SYSTEM_MEMORY_LIST_COMMAND)Parameters.Find("Command").To<sint32>();
            NTSTATUS status = NtSetSystemInformation(SystemMemoryListInformation, &command, sizeof(SYSTEM_MEMORY_LIST_COMMAND));
            Response = CVariant((sint32)status);
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "RunAsService")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            // Extract PH_RUNAS_SERVICE_PARAMETERS from CVariant
            PH_RUNAS_SERVICE_PARAMETERS params = { 0 };
            params.ProcessId = (ULONG)Parameters.Find("ProcessId").To<quint64>();

            // Convert strings to wide strings (need to keep storage alive)
            // IMPORTANT: Must convert empty strings to NULL pointers
            std::wstring userNameStr = Parameters.Find("UserName").ToWString();
            params.UserName = userNameStr.size() != 0 ? userNameStr.c_str() : NULL;

            std::wstring passwordStr = Parameters.Find("Password").ToWString();
            // If we have a username we also must have a password, even if its empty
            params.Password = userNameStr.size() != 0 ? passwordStr.c_str() : NULL;

            params.LogonType = (ULONG)Parameters.Find("LogonType").To<uint32>();
            params.SessionId = (ULONG)Parameters.Find("SessionId").To<uint32>();

            std::wstring currentDirStr = Parameters.Find("CurrentDirectory").ToWString();
            params.CurrentDirectory = currentDirStr.size() != 0 ? currentDirStr.c_str() : NULL;

            std::wstring commandLineStr = Parameters.Find("CommandLine").ToWString();
            params.CommandLine = commandLineStr.size() != 0 ? commandLineStr.c_str() : NULL;

            std::wstring fileNameStr = Parameters.Find("FileName").ToWString();
            params.FileName = fileNameStr.size() != 0 ? fileNameStr.c_str() : NULL;

            std::wstring desktopNameStr = Parameters.Find("DesktopName").ToWString();
            params.DesktopName = desktopNameStr.size() != 0 ? desktopNameStr.c_str() : NULL;

            params.UseLinkedToken = Parameters.Find("UseLinkedToken").To<BOOLEAN>();

            std::wstring serviceNameStr = Parameters.Find("ServiceName").ToWString();
            params.ServiceName = serviceNameStr.size() != 0 ? serviceNameStr.c_str() : NULL;

            params.CreateSuspendedProcess = Parameters.Find("CreateSuspendedProcess").To<BOOLEAN>();

            // Validate parameters before invoking
            NTSTATUS status = PhSvcpValidateRunAsServiceParameters(&params);
            if (NT_SUCCESS(status))
            {
                status = PhInvokeRunAsService(&params);
            }
            Response = CVariant((sint32)status);
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "WriteMiniDumpProcess")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            HANDLE localProcessHandle = (HANDLE)(ULONG_PTR)Parameters.Find("LocalProcessHandle").To<quint64>();
            HANDLE processId = (HANDLE)(ULONG_PTR)Parameters.Find("ProcessId").To<quint64>();
            HANDLE localFileHandle = (HANDLE)(ULONG_PTR)Parameters.Find("LocalFileHandle").To<quint64>();
            ULONG dumpType = (ULONG)Parameters.Find("DumpType").To<uint32>();

            HRESULT hr = PhWriteMiniDumpProcess(localProcessHandle, processId, localFileHandle, (MINIDUMP_TYPE)dumpType, NULL, NULL, NULL);
            if (hr != S_OK)
            {
                if (hr == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER))
                    Response = CVariant((uint32)STATUS_INVALID_PARAMETER);
                else
                    Response = CVariant((uint32)STATUS_UNSUCCESSFUL);
            }
            else
            {
                Response = CVariant((uint32)STATUS_SUCCESS);
            }
        }
        else
        {
            Response = CVariant((uint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "CreateProcessForKsi")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            NTSTATUS status;
            std::wstring commandLineStr = Parameters.Find("CommandLine").ToWString();
            ULONGLONG mitigationFlags0 = Parameters.Find("MitigationFlags0").To<uint64>();
            ULONGLONG mitigationFlags1 = Parameters.Find("MitigationFlags1").To<uint64>();

            PPROC_THREAD_ATTRIBUTE_LIST attributeList = NULL;
            STARTUPINFOEXW startupInfoEx;
            HANDLE processHandle = NULL;
            HANDLE tokenHandle = NULL;
            PVOID environment = NULL;
            PWSTR commandLineCopy = NULL;

            // Set up mitigation flags if specified
            if (mitigationFlags0 || mitigationFlags1)
            {
                ULONGLONG mitigationFlags[2] = { mitigationFlags0, mitigationFlags1 };

                status = PhInitializeProcThreadAttributeList(&attributeList, 1);

                if (!NT_SUCCESS(status))
                    goto CreateProcessForKsiCleanup;

                // Windows 10 22H2+ supports two ULONG64 values for mitigation policy
                ULONG mitigationSize = sizeof(ULONG64);
                if (mitigationFlags1)
                    mitigationSize = sizeof(ULONG64) * 2;

                status = PhUpdateProcThreadAttribute(
                    attributeList,
                    PROC_THREAD_ATTRIBUTE_MITIGATION_POLICY,
                    mitigationFlags,
                    mitigationSize
                );

                if (!NT_SUCCESS(status))
                    goto CreateProcessForKsiCleanup;
            }

            ZeroMemory(&startupInfoEx, sizeof(STARTUPINFOEXW));
            startupInfoEx.StartupInfo.cb = sizeof(STARTUPINFOEXW);
            startupInfoEx.lpAttributeList = attributeList;

            // Open the client process (pid is the calling process)
            status = PhOpenProcess(
                &processHandle,
                PROCESS_QUERY_LIMITED_INFORMATION,
                UlongToHandle(pid)
            );

            if (!NT_SUCCESS(status))
                goto CreateProcessForKsiCleanup;

            // Get the client's token
            status = PhOpenProcessToken(
                processHandle,
                TOKEN_ALL_ACCESS,
                &tokenHandle
            );

            if (!NT_SUCCESS(status))
                goto CreateProcessForKsiCleanup;

            // Create environment block from the token
            status = PhCreateEnvironmentBlock(&environment, tokenHandle, FALSE);

            if (!NT_SUCCESS(status))
                goto CreateProcessForKsiCleanup;

            // Make a writable copy of command line (CreateProcessAsUser requires it)
            if (!commandLineStr.empty())
            {
                size_t cmdLen = commandLineStr.length() + 1;
                commandLineCopy = (PWSTR)PhAllocate(cmdLen * sizeof(WCHAR));
                wcscpy_s(commandLineCopy, cmdLen, commandLineStr.c_str());
            }

            // Create the process with the client's token
            status = PhCreateProcessWin32Ex(
                NULL,
                commandLineCopy,
                environment,
                NULL,
                &startupInfoEx,
                (PH_CREATE_PROCESS_DEFAULT_ERROR_MODE |
                 PH_CREATE_PROCESS_EXTENDED_STARTUPINFO |
                 PH_CREATE_PROCESS_UNICODE_ENVIRONMENT),
                tokenHandle,
                NULL,
                NULL,
                NULL
            );

        CreateProcessForKsiCleanup:
            if (commandLineCopy)
                PhFree(commandLineCopy);

            if (environment)
                PhDestroyEnvironmentBlock(environment);

            if (tokenHandle)
                NtClose(tokenHandle);

            if (processHandle)
                NtClose(processHandle);

            if (attributeList)
                PhDeleteProcThreadAttributeList(attributeList);

            Response = CVariant((sint32)status);
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "CloseSocket")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            std::string localAddrStr = Parameters.Find("LocalAddress").ToString();
            quint16 localPort = Parameters.Find("LocalPort").To<quint16>();
            std::string remoteAddrStr = Parameters.Find("RemoteAddress").ToString();
            quint16 remotePort = Parameters.Find("RemotePort").To<quint16>();

            // Parse IP addresses (assuming IPv4 for now)
            ULONG localAddr = 0, remoteAddr = 0;
            if (inet_pton(AF_INET, localAddrStr.c_str(), &localAddr) != 1)
                localAddr = 0;
            if (inet_pton(AF_INET, remoteAddrStr.c_str(), &remoteAddr) != 1)
                remoteAddr = 0;

            MIB_TCPROW tcpRow = { 0 };
            tcpRow.dwState = MIB_TCP_STATE_DELETE_TCB;
            tcpRow.dwLocalAddr = htonl(localAddr);
            tcpRow.dwLocalPort = htons(localPort);
            tcpRow.dwRemoteAddr = htonl(remoteAddr);
            tcpRow.dwRemotePort = htons(remotePort);

            ULONG result = SetTcpEntry(&tcpRow);
            if (result == ERROR_MR_MID_NOT_FOUND)
                result = ERROR_ACCESS_DENIED;

            Response = CVariant((sint32)result);
        }
        else
        {
            Response = CVariant((sint32)ERROR_INVALID_PARAMETER);
        }
    }
    else if (Command == "RunAsTrustedInstaller")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            std::wstring commandLine = Parameters.Find("CommandLine").ToWString();

            if (commandLine.empty())
            {
                Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
            }
            else
            {
                NTSTATUS status = RunAsTrustedInstaller(commandLine.c_str());
                Response = CVariant((sint32)status);
            }
        }
        else
        {
            Response = CVariant((sint32)STATUS_INVALID_PARAMETER);
        }
    }
    else if (Command == "Refresh")
    {
        Response = CVariant(TRUE);
    }
    else if (Command == "Quit")
    {
        g_Running = FALSE;
        Response = CVariant(TRUE);
    }
    else
    {
        Response = CVariant("Unknown Command");
    }

    return Response;
}

// ============================================================================
// Utility Functions
// ============================================================================

ULONGLONG GetTickCount64Compat(VOID)
{
    return GetTickCount64();
}

NTSTATUS ExecTaskActionProcess(HANDLE ProcessId, PCSTR Action, PVOID Data, ULONG DataSize)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    HANDLE processHandle = NULL;

    if (strcmp(Action, "Terminate") == 0)
    {
        if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_TERMINATE, ProcessId)))
        {
            status = PhTerminateProcess(processHandle, 1);
        }
    }
    else if (strcmp(Action, "Suspend") == 0)
    {
        if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, ProcessId)))
        {
            status = NtSuspendProcess(processHandle);
        }
    }
    else if (strcmp(Action, "Resume") == 0)
    {
        if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, ProcessId)))
        {
            status = NtResumeProcess(processHandle);
        }
    }
    else if (strcmp(Action, "SetPriorityBoost") == 0)
    {
        if (DataSize >= sizeof(BOOLEAN))
        {
            if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, ProcessId)))
            {
                BOOLEAN disablePriorityBoost = *(PBOOLEAN)Data;
                status = PhSetProcessPriorityBoost(processHandle, disablePriorityBoost);
            }
        }
    }
    else if (strcmp(Action, "SetPriority") == 0)
    {
        if (DataSize >= sizeof(UCHAR))
        {
            if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, ProcessId)))
            {
                status = PhSetProcessPriorityClass(processHandle, *(PUCHAR)Data);
            }
        }
    }
    else if (strcmp(Action, "SetPagePriority") == 0)
    {
        if (DataSize >= sizeof(ULONG))
        {
            if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, ProcessId)))
            {
                status = PhSetProcessPagePriority(processHandle, *(PULONG)Data);
            }
        }
    }
    else if (strcmp(Action, "SetIOPriority") == 0)
    {
        if (DataSize >= sizeof(IO_PRIORITY_HINT))
        {
            if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, ProcessId)))
            {
                status = PhSetProcessIoPriority(processHandle, *(IO_PRIORITY_HINT*)Data);
            }
        }
    }
    else if (strcmp(Action, "SetAffinityMask") == 0)
    {
        if (DataSize >= sizeof(ULONGLONG))
        {
            if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, ProcessId)))
            {
                status = PhSetProcessAffinityMask(processHandle, *(PULONGLONG)Data);
            }
        }
    }

    if (processHandle)
        NtClose(processHandle);

    return status;
}

NTSTATUS ExecTaskActionThread(HANDLE ProcessId, HANDLE ThreadId, PCSTR Action, PVOID Data, ULONG DataSize)
{
    NTSTATUS status = STATUS_INVALID_PARAMETER;
    HANDLE threadHandle = NULL;

    UNREFERENCED_PARAMETER(ProcessId);

    if (strcmp(Action, "Terminate") == 0)
    {
        if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_TERMINATE, ThreadId)))
        {
            status = NtTerminateThread(threadHandle, STATUS_SUCCESS);
        }
    }
    else if (strcmp(Action, "Suspend") == 0)
    {
        if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, ThreadId)))
        {
            status = NtSuspendThread(threadHandle, NULL);
        }
    }
    else if (strcmp(Action, "Resume") == 0)
    {
        if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, ThreadId)))
        {
            status = NtResumeThread(threadHandle, NULL);
        }
    }
    else if (strcmp(Action, "SetPriorityBoost") == 0)
    {
        if (DataSize >= sizeof(BOOLEAN))
        {
            if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, ThreadId)))
            {
                BOOLEAN disablePriorityBoost = *(PBOOLEAN)Data;
                status = PhSetThreadPriorityBoost(threadHandle, disablePriorityBoost);
            }
        }
    }
    else if (strcmp(Action, "SetPriority") == 0)
    {
        if (DataSize >= sizeof(LONG))
        {
            if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, ThreadId)))
            {
                status = PhSetThreadBasePriority(threadHandle, *(PLONG)Data);
            }
        }
    }
    else if (strcmp(Action, "SetPagePriority") == 0)
    {
        if (DataSize >= sizeof(ULONG))
        {
            if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, ThreadId)))
            {
                status = PhSetThreadPagePriority(threadHandle, *(PULONG)Data);
            }
        }
    }
    else if (strcmp(Action, "SetIOPriority") == 0)
    {
        if (DataSize >= sizeof(IO_PRIORITY_HINT))
        {
            if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, ThreadId)))
            {
                status = PhSetThreadIoPriority(threadHandle, *(IO_PRIORITY_HINT*)Data);
            }
        }
    }
    else if (strcmp(Action, "SetAffinityMask") == 0)
    {
        if (DataSize >= sizeof(ULONGLONG))
        {
            if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, ThreadId)))
            {
                status = PhSetThreadAffinityMask(threadHandle, *(PULONGLONG)Data);
            }
        }
    }

    if (threadHandle)
        NtClose(threadHandle);

    return status;
}

NTSTATUS ExecServiceAction(PCWSTR ServiceName, PCSTR Action, PVOID Data, ULONG DataSize)
{
    NTSTATUS status = 0;
    SC_HANDLE serviceHandle = NULL;

    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(DataSize);

    if (strcmp(Action, "Start") == 0)
    {
        if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_START, (PWSTR)ServiceName)))
        {
            if (!StartService(serviceHandle, 0, NULL))
                status = PhGetLastWin32ErrorAsNtStatus();
        }
    }
    else if (strcmp(Action, "Pause") == 0)
    {
        if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (PWSTR)ServiceName)))
        {
            SERVICE_STATUS serviceStatus;
            if (!ControlService(serviceHandle, SERVICE_CONTROL_PAUSE, &serviceStatus))
                status = PhGetLastWin32ErrorAsNtStatus();
        }
    }
    else if (strcmp(Action, "Continue") == 0)
    {
        if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (PWSTR)ServiceName)))
        {
            SERVICE_STATUS serviceStatus;
            if (!ControlService(serviceHandle, SERVICE_CONTROL_CONTINUE, &serviceStatus))
                status = PhGetLastWin32ErrorAsNtStatus();
        }
    }
    else if (strcmp(Action, "Stop") == 0)
    {
        if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_STOP, (PWSTR)ServiceName)))
        {
            SERVICE_STATUS serviceStatus;
            if (!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus))
                status = PhGetLastWin32ErrorAsNtStatus();
        }
    }
    else if (strcmp(Action, "Delete") == 0)
    {
        if (NT_SUCCESS(PhOpenService(&serviceHandle, DELETE, (PWSTR)ServiceName)))
        {
            if (!DeleteService(serviceHandle))
                status = PhGetLastWin32ErrorAsNtStatus();
        }
    }
    else
    {
        status = STATUS_INVALID_PARAMETER;
    }

    if (serviceHandle)
        CloseServiceHandle(serviceHandle);

    return status;
}

// ============================================================================
// Windows Service Implementation
// ============================================================================

VOID WINAPI ServiceMain(DWORD argc, LPTSTR* argv)
{
    UNREFERENCED_PARAMETER(argc);
    UNREFERENCED_PARAMETER(argv);

    // Register service control handler
    g_ServiceStatusHandle = RegisterServiceCtrlHandlerW(g_ServiceName, ServiceCtrlHandler);
    if (!g_ServiceStatusHandle)
    {
        return;
    }

    // Initialize service status
    ZeroMemory(&g_ServiceStatus, sizeof(g_ServiceStatus));
    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
    g_ServiceStatus.dwWin32ExitCode = NO_ERROR;
    g_ServiceStatus.dwServiceSpecificExitCode = 0;
    g_ServiceStatus.dwCheckPoint = 0;
    g_ServiceStatus.dwWaitHint = 0;

    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    // Initialize ProcessHacker
    if (!InitializeProcessHacker())
    {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
        g_ServiceStatus.dwServiceSpecificExitCode = 1;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        return;
    }

    // Create stop event
    g_ServiceStopEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!g_ServiceStopEvent)
    {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        g_ServiceStatus.dwWin32ExitCode = GetLastError();
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
        return;
    }

    // Update service status to running
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

    // Run service worker
    ServiceWorkerThread(NULL);

    // Cleanup
    if (g_ServiceStopEvent)
    {
        CloseHandle(g_ServiceStopEvent);
        g_ServiceStopEvent = NULL;
    }

    // Set service status to stopped
    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);
}

VOID WINAPI ServiceCtrlHandler(DWORD ctrlCode)
{
    switch (ctrlCode)
    {
    case SERVICE_CONTROL_STOP:
        if (g_ServiceStatus.dwCurrentState != SERVICE_RUNNING)
            break;

        g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
        SetServiceStatus(g_ServiceStatusHandle, &g_ServiceStatus);

        // Signal stop event
        if (g_ServiceStopEvent)
            SetEvent(g_ServiceStopEvent);

        g_Running = FALSE;
        break;

    case SERVICE_CONTROL_INTERROGATE:
        break;

    default:
        break;
    }
}

VOID ServiceWorkerThread(PVOID Parameter)
{
    UNREFERENCED_PARAMETER(Parameter);

    WCHAR pipeName[256];
    wsprintfW(pipeName, L"\\\\.\\pipe\\%s", g_ServiceName);

    // Service mode: Run pipe server with stop event support
    RunPipeServer(pipeName, g_Timeout, g_ServiceStopEvent);
}

DWORD WINAPI ClientHandlerThread(LPVOID lpParam)
{
    HANDLE hPipe = (HANDLE)lpParam;
    ULONG pid = 0;
    g_PipeHandle = hPipe;
    
    GetNamedPipeClientProcessId(hPipe, &pid);

    // Process client requests
    while (TRUE)
    {
        // Read request CVariant
        CVariant request;
        if (!RecvCVariant(hPipe, request))
        {
            break;
        }

        g_LastActivity = GetTickCount64Compat();

        // Process the command
        CVariant response = ProcessCommand(request, pid);

        // Send response CVariant
        if (!SendCVariant(hPipe, response))
        {
            break;
        }

        // Check for quit command
        std::string cmdStr;
        if (request.GetType() == VAR_TYPE_ASCII || request.GetType() == VAR_TYPE_UTF8 || request.GetType() == VAR_TYPE_UNICODE)
        {
            cmdStr = request.ToString();
        }
        else if (request.GetType() == VAR_TYPE_MAP)
        {
            CVariant cmdVar = request.Find("Command");
            if (cmdVar.IsValid())
                cmdStr = cmdVar.ToString();
        }

        if (cmdStr == "Quit")
        {
            break;
        }
    }

    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
    g_PipeHandle = INVALID_HANDLE_VALUE;

    return 0;
}

BOOLEAN InstallAndRunService(PCWSTR ServiceName)
{
    WCHAR szPath[MAX_PATH];
    if (!GetModuleFileNameW(NULL, szPath, MAX_PATH))
    {
        return FALSE;
    }

    // Open service control manager
    SC_HANDLE scManager = OpenSCManagerW(NULL, NULL, SC_MANAGER_CREATE_SERVICE);
    if (!scManager)
    {
        return FALSE;
    }

    BOOLEAN success = FALSE;

    // Try to start existing service first
    SC_HANDLE service = OpenServiceW(scManager, ServiceName, SERVICE_QUERY_STATUS | SERVICE_START);
    if (service)
    {
        SERVICE_STATUS status;
        if (QueryServiceStatus(service, &status) && status.dwCurrentState == SERVICE_RUNNING)
        {
            success = TRUE;
        }
        else if (StartServiceW(service, 0, NULL))
        {
            success = TRUE;
        }

        CloseServiceHandle(service);
    }

    // If not running, create and start service
    if (!success)
    {
        WCHAR commandLine[512];
        wsprintfW(commandLine, L"\"%s\" -svc \"%s\" -timeout 5000", szPath, ServiceName);

        service = CreateServiceW(
            scManager,
            ServiceName,
            ServiceName,
            SERVICE_ALL_ACCESS,
            SERVICE_WIN32_OWN_PROCESS,
            SERVICE_DEMAND_START,
            SERVICE_ERROR_IGNORE,
            commandLine,
            NULL,
            NULL,
            NULL,
            L"LocalSystem",
            L""
        );

        if (service)
        {
            if (StartServiceW(service, 0, NULL))
            {
                success = TRUE;
            }

            // Delete service (temporary service)
            DeleteService(service);
            CloseServiceHandle(service);
        }
    }

    CloseServiceHandle(scManager);
    return success;
}
