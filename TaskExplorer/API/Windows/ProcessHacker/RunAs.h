#pragma once

#include "../ProcessHacker.h"

BOOLEAN PhShellProcessHackerEx(
	_In_opt_ HWND hWnd,
	_In_opt_ PWSTR FileName,
	_In_opt_ PWSTR Parameters,
	_In_ ULONG ShowWindowType,
	_In_ ULONG Flags,
	_In_ ULONG AppFlags,
	_In_opt_ ULONG Timeout,
	_Out_opt_ PHANDLE ProcessHandle
);

NTSTATUS PhExecuteRunAsCommand3(
	_In_ HWND hWnd,
	_In_ PCWSTR Program,
	_In_opt_ PCWSTR UserName,
	_In_opt_ PCWSTR Password,
	_In_opt_ ULONG LogonType,
	_In_opt_ HANDLE ProcessIdWithToken,
	_In_ ULONG SessionId,
	_In_ PCWSTR DesktopName,
	_In_ BOOLEAN UseLinkedToken,
	_In_ BOOLEAN CreateSuspendedProcess
);

typedef struct _PH_RUNAS_SERVICE_PARAMETERS
{
    ULONG ProcessId;
    PWSTR UserName;
    PWSTR Password;
    ULONG LogonType;
    ULONG SessionId;
    PWSTR CurrentDirectory;
    PWSTR CommandLine;
    PWSTR FileName;
    PWSTR DesktopName;
    BOOLEAN UseLinkedToken;
    PWSTR ServiceName;
    BOOLEAN CreateSuspendedProcess;
} PH_RUNAS_SERVICE_PARAMETERS, *PPH_RUNAS_SERVICE_PARAMETERS;

VOID PhpSplitUserName(_In_ PWSTR UserName, _Out_opt_ PPH_STRING *DomainPart, _Out_opt_ PPH_STRING *UserPart);

NTSTATUS PhInvokeRunAsService(
	_In_ PPH_RUNAS_SERVICE_PARAMETERS Parameters
);

BOOLEAN PhInitializeNamespacePolicy(
	VOID
);

VOID PhSetDesktopWinStaAccess(VOID);

long SvcApiInvokeRunAsService(const QVariantMap& Parameters);

NTSTATUS RunAsLimitedUser(PWSTR CommandLine);
NTSTATUS RunAsTrustedInstaller(PWSTR CommandLine);

BOOLEAN PhMwpOnNotify(_In_ NMHDR *Header, _Out_ LRESULT *Result);

#define RUNAS_MODE_ADMIN 1
#define RUNAS_MODE_LIMITED 2
#define RUNAS_MODE_SYS 3

extern ULONG SelectedRunAsMode;
extern PHAPPAPI HWND PhMainWndHandle;

BOOLEAN IsServiceAccount(_In_ PPH_STRING UserName);
BOOLEAN IsCurrentUserAccount(_In_ PPH_STRING UserName);

void AddAccountsToComboBox(QComboBox* pComboBox);
void AddSessionsToComboBox(QComboBox* pComboBox);
void AddDesktopsToComboBox(QComboBox* pComboBox);

VOID SetDefaultSessionEntry(QComboBox* pComboBox);
VOID SetDefaultDesktopEntry(QComboBox* pComboBox);

void AddProgramsToComboBox(QComboBox* pComboBox);
VOID PhpAddRunMRUListEntry(_In_ PH_STRINGREF CommandLine);

// from guisup.h
PHLIBAPI
BOOLEAN
NTAPI
PhShowRunFileDialog(
    _In_ HWND WindowHandle,
    _In_opt_ HICON WindowIcon,
    _In_opt_ PWSTR WorkingDirectory,
    _In_opt_ PWSTR WindowTitle,
    _In_opt_ PWSTR WindowDescription,
    _In_ ULONG Flags
    );