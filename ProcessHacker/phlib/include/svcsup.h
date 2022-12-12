#ifndef _PH_SVCSUP_H
#define _PH_SVCSUP_H

#ifdef __cplusplus
extern "C" {
#endif

extern WCHAR *PhServiceTypeStrings[12];
extern WCHAR *PhServiceStartTypeStrings[5];
extern WCHAR *PhServiceErrorControlStrings[4];

_Success_(return != NULL)
PHLIBAPI
PVOID
NTAPI
PhEnumServices(
    _In_ SC_HANDLE ScManagerHandle,
    _In_opt_ ULONG Type,
    _In_opt_ ULONG State,
    _Out_ PULONG Count
    );

PHLIBAPI
SC_HANDLE
NTAPI
PhGetServiceManagerHandle(
    VOID
    );

PHLIBAPI
SC_HANDLE
NTAPI
PhOpenService(
    _In_ PWSTR ServiceName,
    _In_ ACCESS_MASK DesiredAccess
    );

PHLIBAPI
PVOID
NTAPI
PhGetServiceConfig(
    _In_ SC_HANDLE ServiceHandle
    );

PHLIBAPI
PVOID
NTAPI
PhQueryServiceVariableSize(
    _In_ SC_HANDLE ServiceHandle,
    _In_ ULONG InfoLevel
    );

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceDescription(
    _In_ SC_HANDLE ServiceHandle
    );

_Success_(return)
PHLIBAPI
BOOLEAN
NTAPI
PhGetServiceDelayedAutoStart(
    _In_ SC_HANDLE ServiceHandle,
    _Out_ PBOOLEAN DelayedAutoStart
    );

PHLIBAPI
BOOLEAN
NTAPI
PhSetServiceDelayedAutoStart(
    _In_ SC_HANDLE ServiceHandle,
    _In_ BOOLEAN DelayedAutoStart
    );

PHLIBAPI
PPH_STRINGREF
NTAPI
PhGetServiceStateString(
    _In_ ULONG ServiceState
    );

PHLIBAPI
PPH_STRINGREF
NTAPI
PhGetServiceTypeString(
    _In_ ULONG ServiceType
    );

PHLIBAPI
ULONG
NTAPI
PhGetServiceTypeInteger(
    _In_ PPH_STRINGREF ServiceType
    );

PHLIBAPI
PPH_STRINGREF
NTAPI
PhGetServiceStartTypeString(
    _In_ ULONG ServiceStartType
    );

PHLIBAPI
ULONG
NTAPI
PhGetServiceStartTypeInteger(
    _In_ PPH_STRINGREF ServiceStartType
    );

PHLIBAPI
PPH_STRINGREF
NTAPI
PhGetServiceErrorControlString(
    _In_ ULONG ServiceErrorControl
    );

PHLIBAPI
ULONG
NTAPI
PhGetServiceErrorControlInteger(
    _In_ PPH_STRINGREF ServiceErrorControl
    );

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceNameFromTag(
    _In_ HANDLE ProcessId,
    _In_ PVOID ServiceTag
    );

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceNameForModuleReference(
    _In_ HANDLE ProcessId,
    _In_ PWSTR ModuleName
    );

PHLIBAPI
NTSTATUS
NTAPI
PhGetThreadServiceTag(
    _In_ HANDLE ThreadHandle,
    _In_opt_ HANDLE ProcessHandle,
    _Out_ PVOID *ServiceTag
    );

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceKeyName(
    _In_ PPH_STRINGREF ServiceName
);

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceHandleFileName(
    _In_ SC_HANDLE ServiceHandle,
    _In_ PPH_STRINGREF ServiceName
    );

PHLIBAPI
PPH_STRING
NTAPI
PhGetServiceFileName(
    _In_ PPH_STRINGREF ServiceName
    );

PHLIBAPI
NTSTATUS
NTAPI
PhGetServiceDllParameter(
    _In_ ULONG ServiceType,
    _In_ PPH_STRINGREF ServiceName,
    _Out_ PPH_STRING *ServiceDll
    );

#ifdef __cplusplus
}
#endif

#endif
