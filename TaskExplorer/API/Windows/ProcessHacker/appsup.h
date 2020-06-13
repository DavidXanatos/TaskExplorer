#pragma once

#include "../ProcessHacker.h"

typedef enum _PH_KNOWN_PROCESS_TYPE
{
	UnknownProcessType,
	SystemProcessType, // ntoskrnl/ntkrnlpa/...
	SessionManagerProcessType, // smss
	WindowsSubsystemProcessType, // csrss
	WindowsStartupProcessType, // wininit
	ServiceControlManagerProcessType, // services
	LocalSecurityAuthorityProcessType, // lsass
	LocalSessionManagerProcessType, // lsm
	WindowsLogonProcessType, // winlogon
	ServiceHostProcessType, // svchost
	UmdfHostProcessType, // wudfhost
	WmiProviderHostType,
	WindowsOtherType,

	RunDllAsAppProcessType, // rundll32
	ComSurrogateProcessType, // dllhost
	TaskHostProcessType, // taskeng, taskhost, taskhostex
	ExplorerProcessType, // explorer
	EdgeProcessType, // Microsoft Edge

	MaximumProcessType,
	KnownProcessTypeMask = 0xffff,

	KnownProcessWow64 = 0x20000
} PH_KNOWN_PROCESS_TYPE;


PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(quint64 ProcessId, const QString& FileName);

typedef union _PH_KNOWN_PROCESS_COMMAND_LINE
{
    struct
    {
        PPH_STRING GroupName;
    } ServiceHost;
    struct
    {
        PPH_STRING FileName;
        PPH_STRING ProcedureName;
    } RunDllAsApp;
    struct
    {
        GUID Guid;
        PPH_STRING Name; // optional
        PPH_STRING FileName; // optional
    } ComSurrogate;
} PH_KNOWN_PROCESS_COMMAND_LINE, *PPH_KNOWN_PROCESS_COMMAND_LINE;

BOOLEAN PhaGetProcessKnownCommandLine(_In_ PPH_STRING CommandLine, _In_ PH_KNOWN_PROCESS_TYPE KnownProcessType, _Out_ PPH_KNOWN_PROCESS_COMMAND_LINE KnownCommandLine);

extern GUID XP_CONTEXT_GUID;
extern GUID VISTA_CONTEXT_GUID;
extern GUID WIN7_CONTEXT_GUID;
extern GUID WIN8_CONTEXT_GUID;
extern GUID WINBLUE_CONTEXT_GUID;
extern GUID WIN10_CONTEXT_GUID;

NTSTATUS PhGetProcessSwitchContext(_In_ HANDLE ProcessHandle, _Out_ PGUID Guid);

ULONG GetProcessDpiAwareness(HANDLE QueryHandle);

BOOLEAN PhShellOpenKey2(_In_ HWND hWnd, _In_ PPH_STRING KeyName);

VOID PhShellExecuteUserString(
    _In_ HWND hWnd,
    _In_ PWSTR Setting,
    _In_ PWSTR String,
    _In_ BOOLEAN UseShellExecute,
    _In_opt_ PWSTR ErrorMessage
);

////////////////////////////////////////////////////////////////////////////////////
// other

NTSTATUS PhSvcCallSendMessage(_In_opt_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam, _In_ BOOLEAN post = FALSE);

__inline NTSTATUS PhSvcCallPostMessage(_In_opt_ HWND hWnd, _In_ UINT Msg, _In_ WPARAM wParam, _In_ LPARAM lParam)
{
	return PhSvcCallSendMessage(hWnd, Msg, wParam, lParam, TRUE);
}