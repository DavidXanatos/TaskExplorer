#pragma once

#define _PHLIB_
#define _PHAPP_
#define _WINDOWS

#ifdef _DEBUG
#define DEBUG
#endif

#ifdef WIN64
#ifndef _WIN64
#deinfe _WIN64
#endif
#endif

#if !defined(_PHAPP_)
#define PHAPPAPI __declspec(dllimport)
#else
#define PHAPPAPI __declspec(dllexport)
#endif

#include <ph.h>
#include <guisup.h>
#include <provider.h>
#include <filestream.h>
#include <fastlock.h>
#include <treenew.h>
#include <graph.h>
#include <circbuf.h>
#include <dltmgr.h>
#include <phnet.h>
#include <kphuser.h>
#include <appresolver.h>
#include <hndlinfo.h>
#include <verify.h>
#include <ref.h>
#include <subprocesstag.h>
#include <secedit.h>
#include <symprv.h>
#include <svcsup.h>
#include <mapimg.h>
#include <combaseapi.h>
#include <lsasup.h>
#include <phnative.h>

// begin_phapppub
#define DPCS_PROCESS_ID ((HANDLE)(LONG_PTR)-2)
#define INTERRUPTS_PROCESS_ID ((HANDLE)(LONG_PTR)-3)

// DPCs, Interrupts and System Idle Process are not real.
// Non-"real" processes can never be opened.
#define PH_IS_REAL_PROCESS_ID(ProcessId) ((LONG_PTR)(ProcessId) > 0)

// DPCs and Interrupts are fake, but System Idle Process is not.
#define PH_IS_FAKE_PROCESS_ID(ProcessId) ((LONG_PTR)(ProcessId) < 0)

// The process item has been removed.
#define PH_PROCESS_ITEM_REMOVED 0x1
// end_phapppub

#define PH_INTEGRITY_STR_LEN 10
#define PH_INTEGRITY_STR_LEN_1 (PH_INTEGRITY_STR_LEN + 1)


QString CastPhString(PPH_STRING phString, bool bDeRef = true);
PPH_STRING CastQString(const QString& qString);

quint64 FILETIME2ms(quint64 fileTime);
time_t FILETIME2time(quint64 fileTime);

// Missing phlib definitions
extern "C" {
	VERIFY_RESULT NTAPI PhVerifyFileCached(_In_ PPH_STRING FileName, _In_opt_ PPH_STRING PackageFullName, _Out_opt_ PPH_STRING *SignerName, _In_ BOOLEAN CachedOnly);
}

// initialization call
int InitPH(bool bSvc = false);

void PhShowAbout(QWidget* parent);

////////////////////////////////////////////////////////////////////////////////////
// netprv.h

#define PH_NETWORK_OWNER_INFO_SIZE 16

typedef struct _PH_NETWORK_CONNECTION
{
    ULONG ProtocolType;
    PH_IP_ENDPOINT LocalEndpoint;
    PH_IP_ENDPOINT RemoteEndpoint;
    ULONG State;
    HANDLE ProcessId;
    LARGE_INTEGER CreateTime;
    ULONGLONG OwnerInfo[PH_NETWORK_OWNER_INFO_SIZE];
    ULONG LocalScopeId; // Ipv6
    ULONG RemoteScopeId; // Ipv6
} PH_NETWORK_CONNECTION, *PPH_NETWORK_CONNECTION;

BOOLEAN PhGetNetworkConnections(_Out_ PPH_NETWORK_CONNECTION *Connections, _Out_ PULONG NumberOfConnections);

////////////////////////////////////////////////////////////////////////////////////
// appsup.h

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
	RunDllAsAppProcessType, // rundll32
	ComSurrogateProcessType, // dllhost
	TaskHostProcessType, // taskeng, taskhost, taskhostex
	ExplorerProcessType, // explorer
	UmdfHostProcessType, // wudfhost
	EdgeProcessType, // Microsoft Edge
	WmiProviderHostType,
	MaximumProcessType,
	KnownProcessTypeMask = 0xffff,

	KnownProcessWow64 = 0x20000
} PH_KNOWN_PROCESS_TYPE;


PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(_In_ HANDLE ProcessId, _In_ PPH_STRING FileName);

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

NTSTATUS PhSvcCallSendMessage(
	_In_opt_ HWND hWnd,
	_In_ UINT Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam,
	_In_ BOOLEAN post = FALSE
);

__inline NTSTATUS PhSvcCallPostMessage(
	_In_opt_ HWND hWnd,
	_In_ UINT Msg,
	_In_ WPARAM wParam,
	_In_ LPARAM lParam
)
{
	return PhSvcCallSendMessage(hWnd, Msg, wParam, lParam, TRUE);
}

VOID PhpWorkaroundWindows10ServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServieEntry);
VOID WeInvertWindowBorder(_In_ HWND hWnd);


////////////////////////////////////////////////////////////////////////////////////
// syssccpu.c

VOID PhSipGetCpuBrandString(_Out_writes_(49) PWSTR BrandString);
BOOLEAN PhSipGetCpuFrequencyFromDistribution(_Out_ DOUBLE *Fraction);

