#include "stdafx.h"
#include "../MiniDump/MiniDumpFilter.h"
#include <shlobj.h>
#include <sddl.h>

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
#include <map>
#include <vector>

// CVariant includes (STL-only, no Qt)
#include "../MiscHelpers/Common/Types.h"
#include "../MiscHelpers/Common/VariantDefs.h"
//#include "../MiscHelpers/Common/Strings.h"
#include "../MiscHelpers/Common/Buffer.h"
#include "../MiscHelpers/Common/Variant.h"

// For the binary stdout the Wine bridge writes; see RunWineList.
#include <io.h>
#include <fcntl.h>

// ConvertSidToStringSidW, for the token the Wine bridge reports.
#include <sddl.h>
#pragma comment(lib, "advapi32.lib")

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

//
// ---- windows, on the desktops only a process standing here can see ----
//
// EnumWindows and FindWindowEx enumerate the *calling thread's* desktop and
// nothing else. That is not a permission problem and cannot be solved by
// privilege: a process in session 0 has no way to enumerate session 1's
// desktop, and neither does one user's process for another user's session.
//
// So somebody has to be standing there. That is what this is - a copy of this
// helper started inside the session, which walks every desktop of every window
// station it can open and reports what it found. The caller merges the answer
// with what it could see itself.
//
// One thread per desktop, because the desktop is a property of the *thread*:
// SetThreadDesktop only works on a thread with no windows or hooks of its own,
// so the enumerating thread has to be a fresh one every time.
//
struct SWndRecord
{
    ULONGLONG   hWnd;
    ULONGLONG   Parent;
    ULONG       ProcessId;
    ULONG       ThreadId;
    ULONG       ShowCommand;
    BOOLEAN     Visible;
    BOOLEAN     Enabled;
    BOOLEAN     OnTop;
    std::wstring Title;
    std::wstring Class;
    std::wstring Desktop;
};

struct SWndEnumContext
{
    std::vector<SWndRecord>* pList;
    const wchar_t*           Desktop;
};

static BOOL CALLBACK WndCollectProc(HWND hWnd, LPARAM lParam)
{
    SWndEnumContext* pContext = (SWndEnumContext*)lParam;

    SWndRecord Record = { 0 };
    Record.hWnd = (ULONGLONG)hWnd;
    Record.Parent = (ULONGLONG)GetParent(hWnd);
    Record.ThreadId = GetWindowThreadProcessId(hWnd, &Record.ProcessId);
    Record.Visible = !!IsWindowVisible(hWnd);
    Record.Enabled = (GetWindowLongW(hWnd, GWL_STYLE) & WS_DISABLED) == 0;
    Record.OnTop = (GetWindowLongW(hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
    Record.Desktop = pContext->Desktop ? pContext->Desktop : L"";

    WINDOWPLACEMENT Placement = { sizeof(Placement) };
    if (GetWindowPlacement(hWnd, &Placement))
        Record.ShowCommand = Placement.showCmd;

    WCHAR Text[512];
    if (GetWindowTextW(hWnd, Text, ARRAYSIZE(Text)) > 0)
        Record.Title = Text;

    WCHAR Class[128];
    if (GetClassNameW(hWnd, Class, ARRAYSIZE(Class)) > 0)
        Record.Class = Class;

    pContext->pList->push_back(Record);

    //
    // Children as well. The window list shows a tree, and a top-level window
    // with its children missing is a different picture from the real one.
    //
    EnumChildWindows(hWnd, WndCollectProc, lParam);
    return TRUE;
}

static BOOL CALLBACK DesktopNameProc(LPWSTR Name, LPARAM lParam)
{
    ((std::vector<std::wstring>*)lParam)->push_back(Name ? Name : L"");
    return TRUE;
}

//
// Every window on every desktop of this process's window station.
//
// EnumDesktopWindows takes the desktop as a handle, so nothing here has to move
// the calling thread onto it - and nothing should. SetThreadDesktop and
// SetProcessWindowStation change where *this whole process* is standing, and a
// helper that has quietly wandered onto the screen-saver desktop can no longer
// act on the windows it just reported. That is a bug with no symptom except
// actions that report success and do nothing.
//
// One station, deliberately: a session has one interactive window station and
// the others belong to services, which are not what this was put here to see.
// Reaching those would mean switching the process onto them, which is exactly
// what the paragraph above is about.
//
static std::vector<SWndRecord> EnumAllDesktopWindows()
{
    std::vector<SWndRecord> List;

    HWINSTA Station = GetProcessWindowStation();
    if (!Station)
        return List;

    std::vector<std::wstring> Desktops;
    EnumDesktopsW(Station, DesktopNameProc, (LPARAM)&Desktops);

    for (size_t i = 0; i < Desktops.size(); i++)
    {
        HDESK Desktop = OpenDesktopW((LPWSTR)Desktops[i].c_str(), 0, FALSE,
                                     DESKTOP_ENUMERATE | DESKTOP_READOBJECTS);
        if (!Desktop)
            continue;

        SWndEnumContext Context;
        Context.pList = &List;
        Context.Desktop = Desktops[i].c_str();

        EnumDesktopWindows(Desktop, WndCollectProc, (LPARAM)&Context);

        CloseDesktop(Desktop);
    }

    return List;
}

//
// One action on one window, performed here where the window can be reached.
//
// The numbering is CWndInfo's, not Win32's - see API/ApiDefs.h. A helper that
// invented its own would be a second table to keep in step.
//
static NTSTATUS DoWindowAction(HWND hWnd, ULONG Action, LONGLONG Value)
{
    if (!IsWindow(hWnd))
        return STATUS_INVALID_HANDLE;

    //
    // Which of these actually report success.
    //
    // ShowWindowAsync, EnableWindow and FlashWindow all return the window's
    // *previous* state rather than a result - so a window that was hidden
    // answers zero to being shown, and reading that as a failure reports one
    // for every action that had something to change. They are asked and
    // believed; only the calls that genuinely return a result are checked.
    //
    BOOL Ok = TRUE;
    switch (Action)
    {
    case 1: ShowWindowAsync(hWnd, Value ? SW_SHOW : SW_HIDE); break;
    case 2: EnableWindow(hWnd, Value ? TRUE : FALSE); break;
    case 3: Ok = SetWindowPos(hWnd, Value ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                              SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE); break;
    case 4:
        {
            //
            // Layered is set first, because SetLayeredWindowAttributes fails on
            // a window that is not - which is every window until it is asked to
            // be one.
            //
            SetWindowLongPtrW(hWnd, GWL_EXSTYLE, GetWindowLongPtrW(hWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            Ok = SetLayeredWindowAttributes(hWnd, 0, (BYTE)Value, LWA_ALPHA);
        }
        break;
    case 5: Ok = SetForegroundWindow(hWnd); break;
    case 6: FlashWindow(hWnd, TRUE); break;
    case 7: ShowWindowAsync(hWnd, SW_RESTORE); break;
    case 8: ShowWindowAsync(hWnd, SW_MINIMIZE); break;
    case 9: ShowWindowAsync(hWnd, SW_MAXIMIZE); break;
    case 10: Ok = PostMessageW(hWnd, WM_CLOSE, 0, 0); break;
    default:
        return STATUS_NOT_SUPPORTED;
    }

    return Ok ? STATUS_SUCCESS : STATUS_UNSUCCESSFUL;
}

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

    //
    // Duplicated as a *primary* token, which is what a token has to be to
    // become a process's own. The impersonation duplicate this used to make is
    // what a thread wears, not what a process runs as, and the call below
    // refuses it - with ERROR_ACCESS_DENIED, which reads like a permission
    // problem and is a type error.
    //
    SECURITY_ATTRIBUTES tokenAttributes;
    tokenAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    tokenAttributes.lpSecurityDescriptor = NULL;
    tokenAttributes.bInheritHandle = FALSE;

    if (!DuplicateTokenEx(tokenHandle, MAXIMUM_ALLOWED, &tokenAttributes,
        SecurityImpersonation, TokenPrimary, &dupTokenHandle))
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

    //
    // The two privileges a process needs to hand a token to a new process, put
    // on the *thread's* token rather than the process's.
    //
    // This is the whole knot. CreateProcessAsUser checks the caller for
    // SeAssignPrimaryToken and SeIncreaseQuota; an elevated administrator does
    // not have the first one at all - it is not in the token, so it cannot be
    // enabled - and the helper is an elevated administrator, not a service. But
    // it is impersonating SYSTEM by this point, SYSTEM has both, and the check
    // is made against the effective token. So they are enabled where they
    // exist: on the impersonation token this thread is wearing.
    //
    // Measured: without this the call returns ERROR_PRIVILEGE_NOT_HELD (1314),
    // with it the process is created.
    //
    {
        HANDLE threadToken = NULL;
        if (NT_SUCCESS(NtOpenThreadToken(NtCurrentThread(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, FALSE, &threadToken)))
        {
            PhSetTokenPrivilege2(threadToken, SE_ASSIGNPRIMARYTOKEN_PRIVILEGE, SE_PRIVILEGE_ENABLED);
            PhSetTokenPrivilege2(threadToken, SE_INCREASE_QUOTA_PRIVILEGE, SE_PRIVILEGE_ENABLED);
            PhSetTokenPrivilege2(threadToken, SE_TCB_PRIVILEGE, SE_PRIVILEGE_ENABLED);
            NtClose(threadToken);
        }
    }

    //
    // The session the caller is looking at, not the one TrustedInstaller runs
    // in.
    //
    // The service lives in session 0, and a token duplicated from it carries
    // that - so the process starts where nobody can see it. Which for "run this
    // as TrustedInstaller" is indistinguishable from not having run it. Setting
    // the session on the token needs SeTcb, which is why it is enabled above.
    //
    {
        ULONG sessionId = NtCurrentPeb()->SessionId;
        SetTokenInformation(dupTokenHandle, TokenSessionId, &sessionId, sizeof(ULONG));
    }

    //
    // Created here, with the token, rather than handed to the secondary logon
    // service.
    //
    // CreateProcessWithTokenW creates nothing itself: it asks seclogon to, and
    // seclogon opens *this* process to do the work. That is the step that fails
    // when the driver is protecting this helper - the protection strips the
    // access every other process gets to it, seclogon included, and the call
    // comes back ACCESS_DENIED however correct everything else was. It is also
    // what made the report look like a permission problem rather than a
    // structural one.
    //
    // Measured on a helper with no protection at all: the old route failed too,
    // with ERROR_ACCESS_DENIED, because the token it was handed was an
    // impersonation token. Both halves are gone now.
    //
    //
    // The environment belongs to the token, not to this process: a
    // TrustedInstaller process carrying the helper's environment would inherit
    // whatever started it.
    //
    PVOID environment = NULL;
    if (!NT_SUCCESS(PhCreateEnvironmentBlock(&environment, dupTokenHandle, FALSE)))
        environment = NULL;

    BOOL bCreated = CreateProcessAsUserW(dupTokenHandle, NULL, commandLineCopy, NULL, NULL, FALSE,
        CREATE_UNICODE_ENVIRONMENT | CREATE_NEW_CONSOLE, environment, NULL, &startupInfo, &processInfo);

    //
    // And if that is somehow refused - no SYSTEM token to impersonate, a policy
    // that removes the privileges - the old route is still worth one attempt.
    // It cannot work while this helper is protected, for the reason above, so it
    // is a fallback and not an alternative.
    //
    if (!bCreated)
    {
        bCreated = CreateProcessWithTokenW(dupTokenHandle, LOGON_WITH_PROFILE, NULL, commandLineCopy,
            CREATE_UNICODE_ENVIRONMENT, NULL, NULL, &startupInfo, &processInfo);
    }

    if (environment)
        PhDestroyEnvironmentBlock(environment);

    if (!bCreated)
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

//
// ---- the Wine bridge ----
//
// One shot: enumerate what Windows sees, write it to stdout, exit.
//
// This runs under Wine, started by the Linux daemon, and it exists because the
// two halves of a Wine process are visible to different observers. /proc knows
// the Linux pid, the memory map and the descriptors; only code running inside
// the prefix can ask wineserver what Windows thinks - and the first thing worth
// asking is which Windows process is which, because the two numbering schemes
// have nothing to do with each other. See NEXT.md 5.16.
//
// stdout rather than the named pipe the rest of this program speaks. A pipe is a
// wineserver object: it exists inside the prefix and a Linux process cannot
// connect to it. A standard output stream is a file descriptor the kernel owns,
// which both sides can hold at once, and it costs no port, no name and no
// authentication of its own - whoever started the process is the only one
// reading it.
//
static CVariant BuildWineProcessList();
static void BuildWineHandles(DWORD Pid, CVariant& Reply);

//
// Framing on the standard streams.
//
// The same shape the pipe server uses - a length and then the packet - but
// blocking and without the overlapped machinery, because these are not pipe
// handles from this program's point of view: they are whatever the parent
// handed us, and it wants a conversation, not a completion port.
//
static bool StdWrite(const CVariant& Variant)
{
    CBuffer Packet;
    Variant.ToPacket(&Packet);

    const uint32 Length = (uint32)Packet.GetSize();
    if (fwrite(&Length, 1, sizeof(Length), stdout) != sizeof(Length))
        return false;
    if (Packet.GetSize() && fwrite(Packet.GetBuffer(), 1, Packet.GetSize(), stdout) != Packet.GetSize())
        return false;

    //
    // Flushed every time. A buffered reply is a reply the other side waits for
    // until this program exits, which for a resident helper is never.
    //
    return fflush(stdout) == 0;
}

static bool StdRead(CVariant& Variant)
{
    uint32 Length = 0;
    if (fread(&Length, 1, sizeof(Length), stdin) != sizeof(Length))
        return false;	// closed: the parent has finished with us

    //
    // A ceiling, because the length arrives from outside this process and a
    // wrong one would otherwise be an allocation the size of whatever it said.
    //
    if (Length == 0 || Length > 0x02000000)
        return false;

    CBuffer Packet(Length);
    if (fread(Packet.GetBuffer(), 1, Length, stdin) != Length)
        return false;
    Packet.SetSize(Length);

    Variant.FromPacket(&Packet);
    return true;
}

//
// A SID as text, and the account it names where there is one.
//
static void WriteSid(CVariant& Entry, const char* KeyText, const char* KeyName, PSID Sid)
{
    if (!Sid || !IsValidSid(Sid))
        return;

    LPWSTR SidText = NULL;
    if (ConvertSidToStringSidW(Sid, &SidText))
    {
        Entry.Write(KeyText, SidText, wcslen(SidText), true);
        LocalFree(SidText);
    }

    if (!KeyName)
        return;

    //
    // The name is a lookup and can fail - an account from another machine, or a
    // well-known sid Wine does not carry. The text form above is always right,
    // so a failed lookup costs a nicety rather than the fact.
    //
    WCHAR Name[256] = L"", Domain[256] = L"";
    DWORD NameLen = 256, DomainLen = 256;
    SID_NAME_USE Use;
    if (LookupAccountSidW(NULL, Sid, Name, &NameLen, Domain, &DomainLen, &Use))
    {
        std::wstring Full = Domain[0] ? (std::wstring(Domain) + L"\\" + Name) : std::wstring(Name);
        Entry.Write(KeyName, Full.c_str(), Full.length(), true);
    }
}

//
// One token, as the program itself sees it.
//
// Under Wine much of this is Wine's own answer rather than a real Windows
// security context - it has no domain, no LSA and no real integrity policy. That
// is not a reason to leave it out: it is exactly what the program running there
// sees and acts on, which is what a task manager is for. It is a reason not to
// dress it up as something else.
//
static CVariant BuildWineToken(DWORD Pid)
{
    CVariant Token;
    Token.BeginMap();

    HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, Pid);
    if (!hProcess)
        hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, Pid);
    if (!hProcess)
    {
        Token.Finish();
        return Token;
    }

    HANDLE hToken = NULL;
    if (!OpenProcessToken(hProcess, TOKEN_QUERY, &hToken))
    {
        CloseHandle(hProcess);
        Token.Finish();
        return Token;
    }

    BYTE Buffer[4096];
    DWORD Length = 0;

    if (GetTokenInformation(hToken, TokenUser, Buffer, sizeof(Buffer), &Length))
        WriteSid(Token, "UserSid", "User", ((PTOKEN_USER)Buffer)->User.Sid);

    if (GetTokenInformation(hToken, TokenOwner, Buffer, sizeof(Buffer), &Length))
        WriteSid(Token, "OwnerSid", "Owner", ((PTOKEN_OWNER)Buffer)->Owner);

    if (GetTokenInformation(hToken, TokenPrimaryGroup, Buffer, sizeof(Buffer), &Length))
        WriteSid(Token, "GroupSid", "Group", ((PTOKEN_PRIMARY_GROUP)Buffer)->PrimaryGroup);

    DWORD Session = 0;
    if (GetTokenInformation(hToken, TokenSessionId, &Session, sizeof(Session), &Length))
        Token.Write("Session", (uint32)Session);

    TOKEN_ELEVATION Elevation = { 0 };
    if (GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof(Elevation), &Length))
        Token.Write("Elevated", Elevation.TokenIsElevated != 0);

    TOKEN_ELEVATION_TYPE ElevationType = TokenElevationTypeDefault;
    if (GetTokenInformation(hToken, TokenElevationType, &ElevationType, sizeof(ElevationType), &Length))
        Token.Write("ElevationType", (sint32)ElevationType);

    //
    // The integrity level is the rid of the last subauthority of its sid, which
    // is the number every Windows tool reports as "medium", "high" and so on.
    //
    if (GetTokenInformation(hToken, TokenIntegrityLevel, Buffer, sizeof(Buffer), &Length))
    {
        PSID Sid = ((PTOKEN_MANDATORY_LABEL)Buffer)->Label.Sid;
        if (Sid && IsValidSid(Sid))
        {
            const UCHAR Count = *GetSidSubAuthorityCount(Sid);
            if (Count)
                Token.Write("Integrity", (uint32)*GetSidSubAuthority(Sid, Count - 1));
        }
    }

    //
    // The groups and the privileges, which are the two lists the token view
    // spends most of its space on. Allocated rather than read into the stack
    // buffer above: a real token has more of both than fits.
    //
    Length = 0;
    GetTokenInformation(hToken, TokenGroups, NULL, 0, &Length);
    if (Length)
    {
        std::vector<BYTE> Groups(Length);
        if (GetTokenInformation(hToken, TokenGroups, Groups.data(), Length, &Length))
        {
            PTOKEN_GROUPS pGroups = (PTOKEN_GROUPS)Groups.data();
            CVariant List;
            List.BeginList();
            for (DWORD i = 0; i < pGroups->GroupCount; i++)
            {
                CVariant Entry;
                Entry.BeginMap();
                WriteSid(Entry, "Sid", "Name", pGroups->Groups[i].Sid);
                Entry.Write("Attributes", (uint32)pGroups->Groups[i].Attributes);
                Entry.Finish();
                List.WriteVariant(Entry);
            }
            List.Finish();
            Token.WriteVariant("Groups", List);
        }
    }

    Length = 0;
    GetTokenInformation(hToken, TokenPrivileges, NULL, 0, &Length);
    if (Length)
    {
        std::vector<BYTE> Privileges(Length);
        if (GetTokenInformation(hToken, TokenPrivileges, Privileges.data(), Length, &Length))
        {
            PTOKEN_PRIVILEGES pPrivileges = (PTOKEN_PRIVILEGES)Privileges.data();
            CVariant List;
            List.BeginList();
            for (DWORD i = 0; i < pPrivileges->PrivilegeCount; i++)
            {
                WCHAR Name[256] = L"";
                DWORD NameLen = 256;
                if (!LookupPrivilegeNameW(NULL, &pPrivileges->Privileges[i].Luid, Name, &NameLen))
                    continue;

                CVariant Entry;
                Entry.BeginMap();
                Entry.Write("Name", Name, wcslen(Name), true);
                Entry.Write("Attributes", (uint32)pPrivileges->Privileges[i].Attributes);
                Entry.Finish();
                List.WriteVariant(Entry);
            }
            List.Finish();
            Token.WriteVariant("Privileges", List);
        }
    }

    Token.Write("Ok", true);

    CloseHandle(hToken);
    CloseHandle(hProcess);

    Token.Finish();
    return Token;
}

//
// The object table of one process, as wineserver keeps it.
//
// None of this is visible from Linux. A Wine process's /proc/<pid>/fd lists the
// descriptors the kernel gave it - the socket to wineserver, the mapped files,
// the pipes - and says nothing about the events, mutexes, keys and sections the
// program itself opened, because those are wineserver objects and the kernel has
// never heard of them. This is the other half of the same process's handles.
//
// The shape is the one Windows uses: enumerate the system's handle table, keep
// the entries belonging to the process asked about, and duplicate each one here
// to ask what it is. There is no way to ask about somebody else's handle
// directly - a handle value only means anything in its own process.
//
// The object manager's type table, in the order it reports it.
//
// Wanted because the handle table carries a type index for every entry, so a
// process whose handles cannot be duplicated can still have its handles typed.
// Only the object *names* need a duplicate, and those are the part Wine will
// not give up for a WoW64 process.
//
// A list rather than a map from index to name, because the index a handle
// carries is not the position here and the two are related by an offset that
// has to be measured - see CalibrateWineTypeOffset.
//
// Empty when the class is not implemented, which is a fact about the host and
// not an error.
//
static std::vector<std::wstring> BuildWineTypeNames()
{
    std::vector<std::wstring> Names;

    ULONG Length = 0x4000;
    for (int Attempt = 0; Attempt < 6; Attempt++)
    {
        PVOID Buffer = malloc(Length);
        if (!Buffer)
            break;

        NTSTATUS Status = NtQueryObject(NULL, ObjectTypesInformation, Buffer, Length, &Length);
        if (NT_SUCCESS(Status))
        {
            const POBJECT_TYPES_INFORMATION pTypes = (POBJECT_TYPES_INFORMATION)Buffer;
            POBJECT_TYPE_INFORMATION pType = (POBJECT_TYPE_INFORMATION)((PUCHAR)pTypes + ALIGN_UP(sizeof(OBJECT_TYPES_INFORMATION), ULONG_PTR));

            for (ULONG i = 0; i < pTypes->NumberOfTypes; i++)
            {
                if (pType->TypeName.Buffer && pType->TypeName.Length)
                    Names.push_back(std::wstring(pType->TypeName.Buffer, pType->TypeName.Length / sizeof(WCHAR)));
                else
                    Names.push_back(std::wstring());

                pType = (POBJECT_TYPE_INFORMATION)((PUCHAR)pType + sizeof(OBJECT_TYPE_INFORMATION) + ALIGN_UP(pType->TypeName.MaximumLength, ULONG_PTR));
            }

            free(Buffer);
            break;
        }

        free(Buffer);
        if (Status != STATUS_INFO_LENGTH_MISMATCH)
            break;

        Length = Length ? Length * 2 : 0x4000;
    }

    return Names;
}

//
// How the handle table's type index relates to the position in that list.
//
// Measured rather than assumed, because the two conventions in use are not
// distinguishable by inspection and the wrong one is not detectably wrong.
// Windows numbers types from two and fills the TypeIndex field of the type
// table to say so. Wine fills that field exactly the same way but numbers its
// *handle* table from the position instead, so taking the field at its word
// names every object as the type two places along: index 13 is a WindowStation
// and the table calls it a Timer, 19 is a Key and the table calls it a File.
// Plausible words, all of them wrong, which is worse than no word at all. Nor
// can the host be asked directly - Wine 11 does not export wine_get_version
// from its PE ntdll, and a check that silently fails would pick the wrong
// convention and look right.
//
// So one of this process's own handles is looked up both ways - by the index
// the table gives it, and by the name the object manager gives it, which needs
// no right at all for a handle we hold ourselves - and the difference between
// the two is the offset. Returns false when no such handle could be matched, in
// which case nothing should be typed from the table at all.
//
static bool CalibrateWineTypeOffset(const std::vector<std::wstring>& Types, PVOID Buffer, bool bExtended, LONG& Offset)
{
    if (Types.empty())
        return false;

    const ULONG_PTR Mine = (ULONG_PTR)GetCurrentProcessId();
    const ULONG_PTR Count = bExtended
        ? (ULONG_PTR)((PSYSTEM_HANDLE_INFORMATION_EX)Buffer)->NumberOfHandles
        : (ULONG_PTR)((PSYSTEM_HANDLE_INFORMATION)Buffer)->NumberOfHandles;

    for (ULONG_PTR i = 0; i < Count; i++)
    {
        HANDLE Value;
        ULONG_PTR Owner;
        ULONG TypeIndex;

        if (bExtended)
        {
            const SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& Entry = ((PSYSTEM_HANDLE_INFORMATION_EX)Buffer)->Handles[i];
            Value = (HANDLE)Entry.HandleValue;
            Owner = (ULONG_PTR)Entry.UniqueProcessId;
            TypeIndex = Entry.ObjectTypeIndex;
        }
        else
        {
            const SYSTEM_HANDLE_TABLE_ENTRY_INFO& Entry = ((PSYSTEM_HANDLE_INFORMATION)Buffer)->Handles[i];
            Value = (HANDLE)(ULONG_PTR)Entry.HandleValue;
            Owner = (ULONG_PTR)Entry.UniqueProcessId;
            TypeIndex = Entry.ObjectTypeIndex;
        }

        if (Owner != Mine)
            continue;

        //
        // Ours, so it can be named without any right being needed: the type of
        // a handle in this very process is always available.
        //
        BYTE TypeBuffer[1024];
        ULONG Returned = 0;
        if (!NT_SUCCESS(NtQueryObject(Value, ObjectTypeInformation, TypeBuffer, sizeof(TypeBuffer), &Returned)))
            continue;

        const POBJECT_TYPE_INFORMATION pType = (POBJECT_TYPE_INFORMATION)TypeBuffer;
        if (!pType->TypeName.Buffer || !pType->TypeName.Length)
            continue;

        const std::wstring Name(pType->TypeName.Buffer, pType->TypeName.Length / sizeof(WCHAR));
        for (size_t k = 0; k < Types.size(); k++)
        {
            if (_wcsicmp(Types[k].c_str(), Name.c_str()) == 0)
            {
                Offset = (LONG)k - (LONG)TypeIndex;
                return true;
            }
        }
    }

    return false;
}

static void BuildWineHandles(DWORD Pid, CVariant& Reply)
{
    CVariant List;
    List.BeginList();

    //
    // What every type is called, read once. The table is compiled into
    // wineserver and does not change while a prefix is up.
    //
    static const std::vector<std::wstring> TypeNames = BuildWineTypeNames();

    //
    // A handle can be named only by duplicating it here and asking the object
    // manager, and duplicating needs this right on the process that holds it.
    //
    // Not fatal when it is refused. Wine will not grant PROCESS_DUP_HANDLE on a
    // 32-bit process - measured: every 64-bit process in the prefix grants it,
    // the one WoW64 process refuses it with ERROR_ACCESS_DENIED while granting
    // MAXIMUM_ALLOWED, and a 32-bit helper is refused just the same. So the
    // handles are still reported, typed from the table above and without their
    // names, rather than the process appearing to hold nothing at all.
    //
    HANDLE hProcess = Pid ? OpenProcess(PROCESS_DUP_HANDLE, FALSE, Pid) : NULL;
    const DWORD OpenError = (Pid && !hProcess) ? GetLastError() : 0;

    //
    // The extended class first: it reports the object address, the attributes
    // and the type index, and its pid field is pointer sized, which is what a
    // 64-bit prefix needs. The old class is the fallback for a Wine too old to
    // have implemented the extended one - its entry carries a USHORT pid, so a
    // process id above 65535 cannot be matched there and is skipped rather than
    // matched wrongly.
    //
    ULONG Length = 0x10000;
    PVOID Buffer = NULL;
    NTSTATUS Status = STATUS_INFO_LENGTH_MISMATCH;
    bool bExtended = true;

    for (int Attempt = 0; Attempt < 8; Attempt++)
    {
        Buffer = malloc(Length);
        if (!Buffer)
            break;

        Status = NtQuerySystemInformation(SystemExtendedHandleInformation, Buffer, Length, &Length);
        if (NT_SUCCESS(Status))
            break;

        free(Buffer);
        Buffer = NULL;

        if (Status != STATUS_INFO_LENGTH_MISMATCH)
            break;

        Length = Length ? Length * 2 : 0x10000;
    }

    if (!NT_SUCCESS(Status))
    {
        bExtended = false;
        Length = 0x10000;

        for (int Attempt = 0; Attempt < 8; Attempt++)
        {
            Buffer = malloc(Length);
            if (!Buffer)
                break;

            Status = NtQuerySystemInformation(SystemHandleInformation, Buffer, Length, &Length);
            if (NT_SUCCESS(Status))
                break;

            free(Buffer);
            Buffer = NULL;

            if (Status != STATUS_INFO_LENGTH_MISMATCH)
                break;

            Length = Length ? Length * 2 : 0x10000;
        }
    }

    if (!Buffer)
    {
        if (hProcess)
            CloseHandle(hProcess);
        List.Finish();
        Reply.WriteVariant("Handles", List);
        Reply.Write("QueryStatus", (uint32)Status);
        return;
    }

    //
    // And how its positions line up with the indexes the table above uses,
    // which is not the same on every host - see CalibrateWineTypeOffset. Once,
    // and only if it can be established: a guess here would name every object
    // as something it is not.
    //
    static LONG TypeOffset = 0;
    static bool bTypeOffsetKnown = false;
    if (!bTypeOffsetKnown && !TypeNames.empty())
        bTypeOffsetKnown = CalibrateWineTypeOffset(TypeNames, Buffer, bExtended, TypeOffset);

    //
    // Collected first, emitted after.
    //
    // The names cannot all be had in one pass: where the process would not be
    // opened, they have to be borrowed from other holders of the same objects,
    // and which objects those are is only known once the process's own entries
    // have been read.
    //
    struct SWineHandleEntry
    {
        HANDLE      Value;
        ULONG       Access;
        ULONG       Attributes;
        ULONG_PTR   Owner;
        ULONG_PTR   Object;
        ULONG       TypeIndex;
        std::wstring Type;
        std::wstring Name;
    };
    std::vector<SWineHandleEntry> Entries;

    const ULONG_PTR Count = bExtended
        ? (ULONG_PTR)((PSYSTEM_HANDLE_INFORMATION_EX)Buffer)->NumberOfHandles
        : (ULONG_PTR)((PSYSTEM_HANDLE_INFORMATION)Buffer)->NumberOfHandles;

    for (ULONG_PTR i = 0; i < Count; i++)
    {
        SWineHandleEntry E;

        if (bExtended)
        {
            const SYSTEM_HANDLE_TABLE_ENTRY_INFO_EX& Entry =
                ((PSYSTEM_HANDLE_INFORMATION_EX)Buffer)->Handles[i];
            E.Value = (HANDLE)Entry.HandleValue;
            E.Owner = (ULONG_PTR)Entry.UniqueProcessId;
            E.Access = Entry.GrantedAccess;
            E.Attributes = Entry.HandleAttributes;
            E.Object = (ULONG_PTR)Entry.Object;
            E.TypeIndex = Entry.ObjectTypeIndex;
        }
        else
        {
            const SYSTEM_HANDLE_TABLE_ENTRY_INFO& Entry =
                ((PSYSTEM_HANDLE_INFORMATION)Buffer)->Handles[i];
            E.Value = (HANDLE)(ULONG_PTR)Entry.HandleValue;
            E.Owner = (ULONG_PTR)Entry.UniqueProcessId;
            E.Access = Entry.GrantedAccess;
            E.Attributes = Entry.HandleAttributes;
            E.Object = (ULONG_PTR)Entry.Object;
            E.TypeIndex = Entry.ObjectTypeIndex;
        }

        //
        // Pid 0 is not a process here: it means every entry in the table, which
        // is what the -wine-handles diagnostic asks for when it wants to know
        // what a prefix reports at all. The daemon always names a process.
        //
        if (Pid && E.Owner != (ULONG_PTR)Pid)
            continue;

        //
        // The type as the object manager spells it - "Event", "Key", "Mutant" -
        // straight out of the type table, which needs no handle of our own.
        //
        const LONG TypePos = bTypeOffsetKnown ? ((LONG)E.TypeIndex + TypeOffset) : -1;
        if (TypePos >= 0 && TypePos < (LONG)TypeNames.size())
            E.Type = TypeNames[TypePos];

        Entries.push_back(E);
    }

    const ULONG Kept = (ULONG)Entries.size();

    if (hProcess)
    {
        for (size_t i = 0; i < Entries.size(); i++)
        {
            //
            // DUPLICATE_SAME_ACCESS rather than a mask of our own: a handle
            // opened without the rights we would have asked for still answers
            // what follows, and asking for more than the original had fails.
            //
            HANDLE hDup = NULL;
            if (!DuplicateHandle(hProcess, Entries[i].Value, GetCurrentProcess(), &hDup, 0, FALSE, DUPLICATE_SAME_ACCESS))
                continue;

            //
            // The type again, for a host whose type table could not be read or
            // could not be lined up with the handle table's numbering.
            //
            if (Entries[i].Type.empty())
            {
                BYTE TypeBuffer[1024];
                ULONG Returned = 0;
                if (NT_SUCCESS(NtQueryObject(hDup, ObjectTypeInformation, TypeBuffer, sizeof(TypeBuffer), &Returned)))
                {
                    const POBJECT_TYPE_INFORMATION pType = (POBJECT_TYPE_INFORMATION)TypeBuffer;
                    if (pType->TypeName.Buffer && pType->TypeName.Length)
                        Entries[i].Type = std::wstring(pType->TypeName.Buffer, pType->TypeName.Length / sizeof(WCHAR));
                }
            }

            //
            // And its name, which most objects do not have: an unnamed event is
            // the normal case rather than a failure. A file's name is its NT
            // path. This is the part that needs the duplicate.
            //
            BYTE NameBuffer[4096];
            ULONG Returned = 0;
            if (NT_SUCCESS(NtQueryObject(hDup, ObjectNameInformation, NameBuffer, sizeof(NameBuffer), &Returned)))
            {
                const POBJECT_NAME_INFORMATION pName = (POBJECT_NAME_INFORMATION)NameBuffer;
                if (pName->Name.Buffer && pName->Name.Length)
                    Entries[i].Name = std::wstring(pName->Name.Buffer, pName->Name.Length / sizeof(WCHAR));
            }

            CloseHandle(hDup);
        }
    }
    //
    // And where the process would not be opened, the handles stay nameless.
    //
    // The obvious way round it does not exist here. On Windows an object can be
    // recognised across processes by its address, which the handle table
    // reports, so a name could be borrowed from any other process holding the
    // same object - and the shared ones, the desktop, the window station, the
    // pipe to a service, are exactly the ones worth naming. Wine reports that
    // address as zero for every entry, which is a reasonable thing for it to do
    // - they are wineserver's own pointers and no business of a client - but it
    // means two handles to one object cannot be told from two handles to two.
    // Measured before the borrowing was written and again after.
    //
    // So a 32-bit program's objects arrive typed and nameless, and that is as
    // far as this can go without something new from wineserver.
    //

    for (size_t i = 0; i < Entries.size(); i++)
    {
        CVariant Handle;
        Handle.BeginMap();
        Handle.Write("Handle", (uint64)(ULONG_PTR)Entries[i].Value);
        Handle.Write("Owner", (uint64)Entries[i].Owner);
        Handle.Write("Access", (uint32)Entries[i].Access);
        Handle.Write("Attributes", (uint32)Entries[i].Attributes);
        Handle.Write("Object", (uint64)Entries[i].Object);
        Handle.Write("TypeIndex", (uint32)Entries[i].TypeIndex);
        if (!Entries[i].Type.empty())
            Handle.Write("Type", Entries[i].Type.c_str(), Entries[i].Type.length(), true);
        if (!Entries[i].Name.empty())
            Handle.Write("Name", Entries[i].Name.c_str(), Entries[i].Name.length(), true);
        Handle.Finish();
        List.WriteVariant(Handle);
    }

    free(Buffer);
    if (hProcess)
        CloseHandle(hProcess);

    List.Finish();
    Reply.WriteVariant("Handles", List);
    Reply.Write("Total", (uint32)Count);
    Reply.Write("Mine", (uint32)Kept);
    Reply.Write("Extended", bExtended);

    //
    // Said rather than left to be inferred: without it a caller cannot tell a
    // list with no names from a process whose objects are all unnamed.
    //
    if (OpenError)
        Reply.Write("OpenError", (uint32)OpenError);
}
//
// The windows one process has, as Wine's user32 sees them.
//
// A Wine program's top-level window is also an X11 window, so the Linux side is
// not blind to it - but what it can see is the X11 window: its geometry, and
// whatever the toolkit chose to advertise. The class name, the window styles,
// the owner, the HWND the program itself passes around: those are user32's, and
// only something running inside the prefix can ask for them.
//
// Top level only, as the tab shows. A window's children are its own business
// and there can be hundreds of them in one dialog.
//
struct SWineWindowEnum
{
    DWORD Pid;
    CVariant* pList;
};

static BOOL CALLBACK WineWindowProc(HWND hWnd, LPARAM Param)
{
    SWineWindowEnum* pEnum = (SWineWindowEnum*)Param;

    DWORD Pid = 0;
    const DWORD Tid = GetWindowThreadProcessId(hWnd, &Pid);
    if (Pid != pEnum->Pid)
        return TRUE;

    CVariant Window;
    Window.BeginMap();
    Window.Write("Wnd", (uint64)(ULONG_PTR)hWnd);
    Window.Write("Parent", (uint64)(ULONG_PTR)GetParent(hWnd));
    Window.Write("Thread", (uint32)Tid);

    WCHAR Class[256] = L"";
    if (GetClassNameW(hWnd, Class, 256))
        Window.Write("Class", Class, wcslen(Class), true);

    //
    // Read with a length first, because a window title can be long and because
    // GetWindowTextW on a window belonging to another process is a message send
    // - one that a wedged program never answers. Wine delivers it through
    // wineserver rather than blocking on the peer, so this cannot hang here the
    // way it can on Windows, but the length is worth having anyway.
    //
    const int Length = GetWindowTextLengthW(hWnd);
    if (Length > 0)
    {
        std::vector<WCHAR> Title(Length + 1, 0);
        const int Got = GetWindowTextW(hWnd, &Title[0], Length + 1);
        if (Got > 0)
            Window.Write("Title", &Title[0], (size_t)Got, true);
    }

    Window.Write("Visible", IsWindowVisible(hWnd) != FALSE);
    Window.Write("Enabled", IsWindowEnabled(hWnd) != FALSE);
    Window.Write("Minimized", IsIconic(hWnd) != FALSE);
    Window.Write("Maximized", IsZoomed(hWnd) != FALSE);

    Window.Write("Style", (uint32)GetWindowLongW(hWnd, GWL_STYLE));
    Window.Write("StyleEx", (uint32)GetWindowLongW(hWnd, GWL_EXSTYLE));

    RECT Rect;
    if (GetWindowRect(hWnd, &Rect))
    {
        Window.Write("Left", (sint32)Rect.left);
        Window.Write("Top", (sint32)Rect.top);
        Window.Write("Right", (sint32)Rect.right);
        Window.Write("Bottom", (sint32)Rect.bottom);
    }

    Window.Finish();
    pEnum->pList->WriteVariant(Window);
    return TRUE;
}

static void BuildWineWindows(DWORD Pid, CVariant& Reply)
{
    CVariant List;
    List.BeginList();

    if (Pid)
    {
        SWineWindowEnum Enum;
        Enum.Pid = Pid;
        Enum.pList = &List;
        EnumWindows(WineWindowProc, (LPARAM)&Enum);
    }

    List.Finish();
    Reply.WriteVariant("Windows", List);
}

//
// Acting on a window inside the prefix.
//
// All of it is a message or a call that only user32 can make, and user32 here
// lives in the prefix - so this is the only place any of it can happen. The
// action arrives as a number rather than a name for the same reason everything
// else on this wire does: the words belong to the viewer.
//
// The numbers are CWineWnd::EAction, mirrored rather than included because this
// program does not build against the API layer. Appended to, never reordered:
// a helper is deployed beside a daemon but not necessarily rebuilt with it.
//
enum EWineWindowAction
{
    eWineWndClose = 0,
    eWineWndQuit,
    eWineWndMinimize,
    eWineWndMaximize,
    eWineWndRestore,
    eWineWndBringToFront,
    eWineWndShow,
    eWineWndHide,
    eWineWndEnable,
    eWineWndDisable,
};

static bool DoWineWindowAction(HWND hWnd, uint32 Action)
{
    if (!IsWindow(hWnd))
        return false;

    switch (Action)
    {
    case eWineWndClose:
        //
        // Posted, not sent. A window that is not answering would hold this
        // helper for as long as it stayed that way, and the helper is the only
        // thing the daemon can ask about this prefix.
        //
        return PostMessageW(hWnd, WM_CLOSE, 0, 0) != FALSE;

    case eWineWndQuit:
        return PostMessageW(hWnd, WM_QUIT, 0, 0) != FALSE;

    //
    // Through the window's own thread, not by calling ShowWindow on it here.
    //
    // ShowWindow on another process's window is documented to work and does
    // nothing useful in practice - measured: the call reported success and the
    // window stayed exactly where it was. A system command is the way a window
    // is asked to change its own state from outside; the owning thread does the
    // work, which is the only thread allowed to.
    //
    case eWineWndMinimize:
        return PostMessageW(hWnd, WM_SYSCOMMAND, SC_MINIMIZE, 0) != FALSE;

    case eWineWndMaximize:
        return PostMessageW(hWnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0) != FALSE;

    case eWineWndRestore:
        return PostMessageW(hWnd, WM_SYSCOMMAND, SC_RESTORE, 0) != FALSE;

    //
    // Async, for the same reason: it queues the change on the owning thread
    // rather than waiting for it here.
    //
    case eWineWndShow:
        return ShowWindowAsync(hWnd, SW_SHOW) != FALSE;

    case eWineWndHide:
        return ShowWindowAsync(hWnd, SW_HIDE) != FALSE;

    case eWineWndEnable:
        return EnableWindow(hWnd, TRUE) || TRUE;

    case eWineWndDisable:
        return EnableWindow(hWnd, FALSE) || TRUE;

    case eWineWndBringToFront:
        //
        // Restored first if it is iconic, because a minimized window brought to
        // the front and left minimized is not what anyone meant by it.
        //
        if (IsIconic(hWnd))
            ShowWindow(hWnd, SW_RESTORE);
        return SetForegroundWindow(hWnd) != FALSE;
    }

    return false;
}

//
// Everything this helper can be asked, in one place.
//
static CVariant WineServeCommand(const CVariant& Request)
{
    CVariant Reply;
    Reply.BeginMap();

    CVariant Value;
    std::wstring Command;
    if (Request.Find("Cmd", Value))
        Command = Value.AsStr();

    if (Command == L"Ping")
    {
        Reply.Write("Ok", true);
    }
    else if (Command == L"ProcessList")
    {
        Reply.Write("Ok", true);
        Reply.WriteVariant("Processes", BuildWineProcessList());
    }
    else if (Command == L"Token")
    {
        uint32 Pid = 0;
        if (Request.Find("Pid", Value))
            Pid = Value.To<uint32>();

        Reply.Write("Ok", Pid != 0);
        if (Pid)
            Reply.WriteVariant("Token", BuildWineToken(Pid));
    }
    else if (Command == L"Handles")
    {
        uint32 Pid = 0;
        if (Request.Find("Pid", Value))
            Pid = Value.To<uint32>();

        Reply.Write("Ok", Pid != 0);
        if (Pid)
            BuildWineHandles(Pid, Reply);
    }
    else if (Command == L"Windows")
    {
        uint32 Pid = 0;
        if (Request.Find("Pid", Value))
            Pid = Value.To<uint32>();

        Reply.Write("Ok", Pid != 0);
        if (Pid)
            BuildWineWindows(Pid, Reply);
    }
    else if (Command == L"WindowAction")
    {
        uint64 Wnd = 0;
        uint32 Action = 0;
        if (Request.Find("Wnd", Value))
            Wnd = Value.To<uint64>();
        if (Request.Find("Action", Value))
            Action = Value.To<uint32>();

        const bool bOk = Wnd && DoWineWindowAction((HWND)(ULONG_PTR)Wnd, Action);
        Reply.Write("Ok", bOk);
        if (!bOk)
            Reply.Write("Error", (uint32)GetLastError());
    }
    else
    {
        //
        // Named rather than silent: a helper that is older than the daemon
        // talking to it should say so, not return an empty answer that reads
        // like "there is nothing there".
        //
        Reply.Write("Ok", false);
        Reply.Write("Error", "unknown command");
    }

    Reply.Finish();
    return Reply;
}

//
// The resident mode.
//
// Requests in on stdin, replies out on stdout, one packet each, until the
// parent closes the pipe or nothing arrives for long enough. See NEXT.md 5.16
// for why the standard streams and not the named pipe the rest of this program
// speaks: a Wine pipe is a wineserver object with no name outside the prefix,
// and a Linux process has nothing to connect to.
//
static int RunWineServe(ULONG IdleTimeout)
{
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);

    const HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
    ULONGLONG LastActivity = GetTickCount64Compat();

    for (;;)
    {
        //
        // Polled rather than blocked on, so that an orphan - a helper whose
        // daemon died without closing anything - does not sit in a prefix for
        // ever. Where the peek is not supported the read below simply blocks,
        // and the parent closing the pipe is what ends it.
        //
        DWORD Available = 0;
        if (PeekNamedPipe(hStdIn, NULL, 0, NULL, &Available, NULL))
        {
            if (!Available)
            {
                if (IdleTimeout && GetTickCount64Compat() - LastActivity > IdleTimeout)
                    break;

                Sleep(50);
                continue;
            }
        }

        CVariant Request;
        try {
            if (!StdRead(Request))
                break;	// the parent is done with us
        } catch (...) {
            break;		// or sent something that is not a packet
        }

        LastActivity = GetTickCount64Compat();

        try {
            if (!StdWrite(WineServeCommand(Request)))
                break;
        } catch (...) {
            break;
        }
    }

    return EXIT_SUCCESS;
}

//
// One window action, asked from a shell.
//
// The same shape as the other two one-shots, and for the same reason: when a
// menu item appears to do nothing, this says whether the prefix refused it or
// whether it never arrived.
//
static int RunWineWindowAction(HWND hWnd, uint32 Action)
{
    const bool bOk = DoWineWindowAction(hWnd, Action);
    wprintf(L"action %u on %08llx: %s (error %u)\n", (unsigned)Action,
        (unsigned long long)(ULONG_PTR)hWnd, bOk ? L"ok" : L"failed",
        bOk ? 0u : (unsigned)GetLastError());
    return EXIT_SUCCESS;
}

//
// The windows of one process, asked from a shell.
//
// The same shape as -wine-handles and for the same reason: when a list comes
// back looking wrong, the question is whether the prefix said that or whether
// something between here and the screen changed it.
//
static int RunWineWindows(DWORD Pid)
{
    CVariant Reply;
    Reply.BeginMap();
    BuildWineWindows(Pid, Reply);
    Reply.Finish();

    CVariant List;
    if (!Reply.Find("Windows", List))
    {
        wprintf(L"no reply\n");
        return EXIT_SUCCESS;
    }

    int Count = 0;
    try {
        List.ReadRawList([&](const CVariant& Entry) {
            CVariant Value;
            uint64 Wnd = Entry.Find("Wnd", Value) ? Value.To<uint64>() : 0;
            std::wstring Class = Entry.Find("Class", Value) ? Value.AsStr() : L"";
            std::wstring Title = Entry.Find("Title", Value) ? Value.AsStr() : L"";
            const bool bVisible = Entry.Find("Visible", Value) && Value.To<bool>();
            const bool bEnabled = Entry.Find("Enabled", Value) && Value.To<bool>();
            uint32 Style = Entry.Find("Style", Value) ? Value.To<uint32>() : 0;
            uint32 Thread = Entry.Find("Thread", Value) ? Value.To<uint32>() : 0;

            wprintf(L"%08llx vis=%d en=%d style=%08x thread=%u %-20s %s\n",
                (unsigned long long)Wnd, (int)bVisible, (int)bEnabled,
                (unsigned)Style, (unsigned)Thread, Class.c_str(), Title.c_str());
            Count++;
        });
    } catch (...) {}

    wprintf(L"%d windows for pid %u\n", Count, Pid);
    return EXIT_SUCCESS;
}

//
// The same question the resident mode answers, asked from a shell.
//
// Readable rather than packed, because the only reason to run it this way is to
// find out what a prefix actually reports - which is a thing one reads. Pid 0
// means every entry in the table rather than one process's.
//
static int RunWineHandles(DWORD Pid)
{
    CVariant Reply;
    Reply.BeginMap();
    BuildWineHandles(Pid, Reply);
    Reply.Finish();

    CVariant List;
    if (!Reply.Find("Handles", List))
    {
        wprintf(L"no reply\n");
        return EXIT_SUCCESS;
    }

    CVariant Value;
    wprintf(L"table=%u mine=%u openerror=%u status=%08x\n",
        Reply.Find("Total", Value) ? Value.To<uint32>() : 0,
        Reply.Find("Mine", Value) ? Value.To<uint32>() : 0,
        Reply.Find("OpenError", Value) ? Value.To<uint32>() : 0,
        Reply.Find("QueryStatus", Value) ? Value.To<uint32>() : 0);

    int Count = 0;
    try {
        List.ReadRawList([&](const CVariant& Entry) {
            CVariant Value;
            std::wstring Type = Entry.Find("Type", Value) ? Value.AsStr() : L"?";
            std::wstring Name = Entry.Find("Name", Value) ? Value.AsStr() : L"";
            uint64 Handle = Entry.Find("Handle", Value) ? Value.To<uint64>() : 0;
            uint64 Owner = Entry.Find("Owner", Value) ? Value.To<uint64>() : 0;
            uint32 TypeIx = Entry.Find("TypeIndex", Value) ? Value.To<uint32>() : 0;
            uint64 Obj = Entry.Find("Object", Value) ? Value.To<uint64>() : 0;
            wprintf(L"owner=%llu %08llx ti=%u obj=%llx %-16s %s\n", (unsigned long long)Owner,
                (unsigned long long)Handle, TypeIx, (unsigned long long)Obj, Type.c_str(), Name.c_str());
            Count++;
        });
    } catch (...) {}

    wprintf(L"%d handles for pid %u\n", Count, Pid);
    return EXIT_SUCCESS;
}

static int RunWineList()
{
    //
    // Binary, or the CRT turns every 0x0A in the packet into a CRLF and the
    // reader gets a corrupted variant with no clue why.
    //
    _setmode(_fileno(stdout), _O_BINARY);

    CBuffer Packet;
    BuildWineProcessList().ToPacket(&Packet);
    fwrite(Packet.GetBuffer(), 1, Packet.GetSize(), stdout);
    fflush(stdout);
    return EXIT_SUCCESS;
}

//
// The Windows process table, as Wine sees it.
//
static CVariant BuildWineProcessList()
{
    CVariant List;
    List.BeginList();

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot != INVALID_HANDLE_VALUE)
    {
        PROCESSENTRY32W processEntry;
        processEntry.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &processEntry))
        {
            do
            {
                CVariant Entry;
                Entry.BeginMap();
                Entry.Write("Pid", (uint32)processEntry.th32ProcessID);
                Entry.Write("ParentPid", (uint32)processEntry.th32ParentProcessID);

                //
                // The full path where it can be had, the bare file name
                // otherwise. The path is what the Linux side matches on - it
                // reads the same string out of /proc/<pid>/cmdline - so a
                // process that will not give it up simply goes unpaired rather
                // than being paired on a name that several processes share.
                //
                WCHAR imagePath[MAX_PATH * 2] = L"";
                DWORD imageChars = MAX_PATH * 2;
                HANDLE processHandle = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION,
                    FALSE, processEntry.th32ProcessID);
                if (processHandle)
                {
                    if (!QueryFullProcessImageNameW(processHandle, 0, imagePath, &imageChars))
                        imagePath[0] = L'\0';
                    CloseHandle(processHandle);
                }

                //
                // utf-8 on the way out. The reader is a Qt program on Linux,
                // where wchar_t is four bytes and here it is two - the same trap
                // the network wire fell into once already.
                //
                const WCHAR* pImage = imagePath[0] ? imagePath : processEntry.szExeFile;
                Entry.Write("Image", pImage, wcslen(pImage), true);
                Entry.Write("Name", processEntry.szExeFile, wcslen(processEntry.szExeFile), true);
                Entry.Finish();

                List.WriteVariant(Entry);
            } while (Process32NextW(snapshot, &processEntry));
        }
        CloseHandle(snapshot);
    }

    List.Finish();
    return List;
}

//
// ---- crash dumps ----
//
// The helper is the one part of this that runs as two different things: a short
// lived worker started by a viewer, and a service running as LocalSystem. Where
// a dump belongs differs accordingly, and getting it wrong means writing a crash
// report into a directory nobody will ever open.
//
// No Qt here - this program has none, which is the point of it - so the same
// rule the viewer's CSettings follows is spelled out with the platform's own
// calls. It is a rule about where the *configuration* is, because that is where
// everything else of ours already goes looking.
//

static BOOLEAN IsLocalSystem(void)
{
    HANDLE hToken = NULL;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
        return FALSE;

    BYTE Buffer[256] = { 0 };
    DWORD Size = 0;
    BOOLEAN bSystem = FALSE;

    if (GetTokenInformation(hToken, TokenUser, Buffer, sizeof(Buffer), &Size))
    {
        //
        // Compared against the well known LocalSystem sid rather than a name:
        // names are localised and can be renamed, S-1-5-18 cannot.
        //
        BYTE SystemSid[SECURITY_MAX_SID_SIZE] = { 0 };
        DWORD SidSize = sizeof(SystemSid);
        if (CreateWellKnownSid(WinLocalSystemSid, NULL, SystemSid, &SidSize))
            bSystem = EqualSid(((PTOKEN_USER)Buffer)->User.Sid, SystemSid) ? TRUE : FALSE;
    }

    CloseHandle(hToken);
    return bSystem;
}

static void GetCrashDumpDir(WCHAR* pPath, DWORD Chars)
{
    pPath[0] = L'\0';

    //
    // A copy that carries its own configuration keeps everything together,
    // whoever is running it. Same rule as the viewer's, and the same reason:
    // a portable copy should leave nothing behind anywhere else.
    //
    WCHAR Beside[MAX_PATH] = { 0 };
    if (GetModuleFileNameW(NULL, Beside, MAX_PATH))
    {
        WCHAR* pSlash = wcsrchr(Beside, L'\\');
        if (pSlash)
        {
            *pSlash = L'\0';

            WCHAR Ini[MAX_PATH] = { 0 };
            wcscpy_s(Ini, MAX_PATH, Beside);
            wcscat_s(Ini, MAX_PATH, L"\\TaskExplorer.ini");
            if (GetFileAttributesW(Ini) != INVALID_FILE_ATTRIBUTES)
            {
                wcscpy_s(pPath, Chars, Beside);
                wcscat_s(pPath, Chars, L"\\MiniDump");
                return;
            }
        }
    }

    //
    // Otherwise it depends on who this is.
    //
    // As LocalSystem, the machine-wide directory - which is where the daemon's
    // configuration and its own dumps are, and where whoever administers the
    // machine already looks. What LOCALAPPDATA means for SYSTEM is a directory
    // under config\systemprofile that nobody will ever open.
    //
    // As a person, that person's own, beside the viewer's settings.
    //
    const DWORD Folder = IsLocalSystem() ? CSIDL_COMMON_APPDATA : CSIDL_LOCAL_APPDATA;

    WCHAR Base[MAX_PATH] = { 0 };
    if (SUCCEEDED(SHGetFolderPathW(NULL, Folder, NULL, 0, Base)))
    {
        wcscpy_s(pPath, Chars, Base);
        wcscat_s(pPath, Chars, L"\\Xanasoft\\TaskExplorer\\MiniDump");
    }
}

int main(int argc, char* argv[])
{
    //
    // Before anything else, and not under a debugger - there the exception
    // belongs to whoever attached. MiniDump.exe is looked for beside this
    // program, which is where the build and the installer both put it.
    //
    if (!IsDebuggerPresent())
    {
        WCHAR DumpDir[MAX_PATH] = { 0 };
        GetCrashDumpDir(DumpDir, MAX_PATH);

        MiniDumpFilter_Init(NULL, L"TaskHelper", MDF_TYPE_TRIAGE, NULL,
            DumpDir[0] ? DumpDir : NULL);
    }

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
        else if (strcmp(argv[i], "-dump-dir") == 0)
        {
            //
            // Where a crash from this process would be written, and stop.
            //
            // It depends on who is running - a person or LocalSystem - and on
            // whether this copy carries its own configuration, which is three
            // answers that are easy to reason about wrongly. Asking the program
            // is cheaper than reasoning.
            //
            WCHAR DumpDir[MAX_PATH] = { 0 };
            GetCrashDumpDir(DumpDir, MAX_PATH);
            wprintf(L"account:  %s\n", IsLocalSystem() ? L"LocalSystem" : L"a user");
            wprintf(L"dumps:    %s\n", DumpDir[0] ? DumpDir : L"(the temp folder)");
            return 0;
        }
        else if (strcmp(argv[i], "-dbg_wait") == 0)
        {
            debugWait = TRUE;
        }
        else if (strcmp(argv[i], "-wine-serve") == 0)
        {
            //
            // Resident. Answered here for the same reason as -wine-list: this
            // mode owns the standard streams and must not share them with a
            // pipe server or a service.
            //
            ULONG idle = 120000;
            if (i + 1 < argc && argv[i + 1][0] >= '0' && argv[i + 1][0] <= '9')
                idle = atoi(argv[++i]);
            return RunWineServe(idle);
        }
        else if (strcmp(argv[i], "-wine-list") == 0)
        {
            //
            // Answered before anything else is set up: this mode talks to
            // nobody, needs no pipe and no service, and must not leave a
            // listener behind when it is done.
            //
            return RunWineList();
        }
        else if (strcmp(argv[i], "-wine-window-action") == 0)
        {
            const ULONG_PTR Wnd = (i + 1 < argc) ? (ULONG_PTR)strtoull(argv[++i], NULL, 16) : 0;
            const uint32 Action = (i + 1 < argc) ? (uint32)atoi(argv[++i]) : 0;
            return RunWineWindowAction((HWND)Wnd, Action);
        }
        else if (strcmp(argv[i], "-wine-windows") == 0)
        {
            return RunWineWindows((i + 1 < argc) ? (DWORD)atoi(argv[++i]) : 0);
        }
        else if (strcmp(argv[i], "-wine-handles") == 0)
        {
            //
            // Answered here for the same reason as -wine-list, and in a function
            // of its own for one more: everything it does that can throw has to
            // stay out of main. Wine cannot read the unwind information this
            // compiler emits for a function that has a handler in it, and an
            // exception thrown anywhere below such a function overflows the
            // stack while trying to unwind through it - which took every mode of
            // this program down, not only this one.
            //
            return RunWineHandles((i + 1 < argc) ? (DWORD)atoi(argv[++i]) : 0);
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
    else if (Command == "EnumWindows")
    {
        //
        // Everything this helper can see, as records rather than as handles:
        // the caller is in another session and cannot ask a single question
        // about any of these windows itself.
        //
        std::vector<SWndRecord> Windows = EnumAllDesktopWindows();

        CVariant List;
        List.BeginList();
        for (size_t i = 0; i < Windows.size(); i++)
        {
            const SWndRecord& W = Windows[i];

            //
            // Named rather than numbered, because this pipe's protocol is named
            // throughout - "Command", "Parameters", "hWnd" - and an index map
            // comes out the other side keyed by the raw bytes of the index,
            // which is not something a reader should have to know.
            //
            CVariant Entry;
            Entry.BeginMap();
            Entry.Write("hWnd", (uint64)W.hWnd);
            Entry.Write("Parent", (uint64)W.Parent);
            Entry.Write("Pid", (uint64)W.ProcessId);
            Entry.Write("Tid", (uint64)W.ThreadId);
            Entry.Write("Show", (uint32)W.ShowCommand);
            Entry.Write("Visible", (uint32)(W.Visible ? 1 : 0));
            Entry.Write("Enabled", (uint32)(W.Enabled ? 1 : 0));
            Entry.Write("OnTop", (uint32)(W.OnTop ? 1 : 0));
            Entry.Write("Title", W.Title, true);
            Entry.Write("Class", W.Class, true);
            Entry.Write("Desktop", W.Desktop, true);
            Entry.Finish();

            List.WriteVariant(Entry);
        }
        List.Finish();

        Response = List;
    }
    else if (Command == "WndAction")
    {
        if (Parameters.IsValid() && Parameters.GetType() == VAR_TYPE_MAP)
        {
            HWND hWnd = (HWND)(ULONG_PTR)Parameters.Find("hWnd").To<quint64>();
            ULONG Action = (ULONG)Parameters.Find("Action").To<quint64>();
            LONGLONG Value = (LONGLONG)Parameters.Find("Value").To<sint64>();

            Response = CVariant((sint32)DoWindowAction(hWnd, Action, Value));
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
