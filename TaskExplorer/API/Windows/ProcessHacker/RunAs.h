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
	_In_ PWSTR Program,
	_In_opt_ PWSTR UserName,
	_In_opt_ PWSTR Password,
	_In_opt_ ULONG LogonType,
	_In_opt_ HANDLE ProcessIdWithToken,
	_In_ ULONG SessionId,
	_In_ PWSTR DesktopName,
	_In_ BOOLEAN UseLinkedToken,
	_In_ BOOLEAN CreateSuspendedProcess
);

VOID PhpSplitUserName(_In_ PWSTR UserName, _Out_opt_ PPH_STRING *DomainPart, _Out_opt_ PPH_STRING *UserPart);

VOID PhSetDesktopWinStaAccess(VOID);

quint32 PhSvcApiInvokeRunAsService(const QVariantMap& Parameters);

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
VOID PhpAddRunMRUListEntry(_In_ PPH_STRING CommandLine);