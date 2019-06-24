#pragma once

#define _PHLIB_
#define _PHAPP_
#define _WINDOWS

#ifdef _DEBUG
#define DEBUG
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

// Missing phlib definitions
extern "C" {
	VERIFY_RESULT PhVerifyFileCached(_In_ PPH_STRING FileName, _In_opt_ PPH_STRING PackageFullName, _Out_opt_ PPH_STRING *SignerName, _In_ BOOLEAN CachedOnly);
}

// initialization call
int InitPH(bool bSvc = false);

void ClearPH();


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

// appsup.h
PH_KNOWN_PROCESS_TYPE PhGetProcessKnownTypeEx(_In_ HANDLE ProcessId, _In_ PPH_STRING FileName);

// appsup.h
extern GUID XP_CONTEXT_GUID;
extern GUID VISTA_CONTEXT_GUID;
extern GUID WIN7_CONTEXT_GUID;
extern GUID WIN8_CONTEXT_GUID;
extern GUID WINBLUE_CONTEXT_GUID;
extern GUID WIN10_CONTEXT_GUID;

// appsup.h
NTSTATUS PhGetProcessSwitchContext(_In_ HANDLE ProcessHandle, _Out_ PGUID Guid);


//netprv.h
#define PH_NETWORK_OWNER_INFO_SIZE 16

//netprv.c
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

//netprv.c
BOOLEAN PhGetNetworkConnections(_Out_ PPH_NETWORK_CONNECTION *Connections, _Out_ PULONG NumberOfConnections);


VOID PhpWorkaroundWindows10ServiceTypeBug(_Inout_ LPENUM_SERVICE_STATUS_PROCESS ServieEntry);

VOID WeInvertWindowBorder(_In_ HWND hWnd);

// procmtgn.h
typedef struct _PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION
{
    PVOID Pointers[MaxProcessMitigationPolicy];
    PROCESS_MITIGATION_DEP_POLICY DEPPolicy; // ProcessDEPPolicy
    PROCESS_MITIGATION_ASLR_POLICY ASLRPolicy; // ProcessASLRPolicy
    PROCESS_MITIGATION_DYNAMIC_CODE_POLICY DynamicCodePolicy; // ProcessDynamicCodePolicy
    PROCESS_MITIGATION_STRICT_HANDLE_CHECK_POLICY StrictHandleCheckPolicy; // ProcessStrictHandleCheckPolicy
    PROCESS_MITIGATION_SYSTEM_CALL_DISABLE_POLICY SystemCallDisablePolicy; // ProcessSystemCallDisablePolicy
    // ProcessMitigationOptionsMask
    PROCESS_MITIGATION_EXTENSION_POINT_DISABLE_POLICY ExtensionPointDisablePolicy; // ProcessExtensionPointDisablePolicy
    PROCESS_MITIGATION_CONTROL_FLOW_GUARD_POLICY ControlFlowGuardPolicy; // ProcessControlFlowGuardPolicy
    PROCESS_MITIGATION_BINARY_SIGNATURE_POLICY SignaturePolicy; // ProcessSignaturePolicy
    PROCESS_MITIGATION_FONT_DISABLE_POLICY FontDisablePolicy; // ProcessFontDisablePolicy
    PROCESS_MITIGATION_IMAGE_LOAD_POLICY ImageLoadPolicy; // ProcessImageLoadPolicy
    PROCESS_MITIGATION_SYSTEM_CALL_FILTER_POLICY SystemCallFilterPolicy; // ProcessSystemCallFilterPolicy
    PROCESS_MITIGATION_PAYLOAD_RESTRICTION_POLICY PayloadRestrictionPolicy; // ProcessPayloadRestrictionPolicy
    PROCESS_MITIGATION_CHILD_PROCESS_POLICY ChildProcessPolicy; // ProcessChildProcessPolicy
    PROCESS_MITIGATION_SIDE_CHANNEL_ISOLATION_POLICY SideChannelIsolationPolicy; // ProcessSideChannelIsolationPolicy
} PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION, *PPH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION;

// procmtgn.c
NTSTATUS PhGetProcessMitigationPolicy(_In_ HANDLE ProcessHandle, _Out_ PPH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION Information);

// procmtgn.c
BOOLEAN PhDescribeProcessMitigationPolicy(_In_ PROCESS_MITIGATION_POLICY Policy, _In_ PVOID Data, _Out_opt_ PPH_STRING *ShortDescription, _Out_opt_ PPH_STRING *LongDescription);

void PhShowAbout(QWidget* parent);


//syssccpu.c
VOID PhSipGetCpuBrandString(_Out_writes_(49) PWSTR BrandString);
BOOLEAN PhSipGetCpuFrequencyFromDistribution(_Out_ DOUBLE *Fraction);

NTSTATUS PhpOpenServiceControlManager(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context);
