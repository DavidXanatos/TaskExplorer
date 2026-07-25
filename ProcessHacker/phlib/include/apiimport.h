/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2015-2016
 *     dmex    2017-2024
 *
 */

#ifndef _PH_APIIMPORT_H
#define _PH_APIIMPORT_H

#include <devquery.h>
#include <sddl.h>
//#include <shlwapi.h>
#include <userenv.h>
#include <ntuser.h>
#include <xmllite.h>


// ntdll

typedef NTSTATUS (NTAPI *_NtQueryInformationEnlistment)(
    _In_ HANDLE EnlistmentHandle,
    _In_ ENLISTMENT_INFORMATION_CLASS EnlistmentInformationClass,
    _Out_writes_bytes_(EnlistmentInformationLength) PVOID EnlistmentInformation,
    _In_ ULONG EnlistmentInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

typedef NTSTATUS (NTAPI *_NtQueryInformationResourceManager)(
    _In_ HANDLE ResourceManagerHandle,
    _In_ RESOURCEMANAGER_INFORMATION_CLASS ResourceManagerInformationClass,
    _Out_writes_bytes_(ResourceManagerInformationLength) PVOID ResourceManagerInformation,
    _In_ ULONG ResourceManagerInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

typedef NTSTATUS (NTAPI *_NtQueryInformationTransaction)(
    _In_ HANDLE TransactionHandle,
    _In_ TRANSACTION_INFORMATION_CLASS TransactionInformationClass,
    _Out_writes_bytes_(TransactionInformationLength) PVOID TransactionInformation,
    _In_ ULONG TransactionInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

typedef NTSTATUS (NTAPI *_NtQueryInformationTransactionManager)(
    _In_ HANDLE TransactionManagerHandle,
    _In_ TRANSACTIONMANAGER_INFORMATION_CLASS TransactionManagerInformationClass,
    _Out_writes_bytes_(TransactionManagerInformationLength) PVOID TransactionManagerInformation,
    _In_ ULONG TransactionManagerInformationLength,
    _Out_opt_ PULONG ReturnLength
    );

typedef NTSTATUS (NTAPI* _NtSetInformationVirtualMemory)(
    _In_ HANDLE ProcessHandle,
    _In_ VIRTUAL_MEMORY_INFORMATION_CLASS VmInformationClass,
    _In_ ULONG_PTR NumberOfEntries,
    _In_reads_(NumberOfEntries) PMEMORY_RANGE_ENTRY VirtualAddresses,
    _In_reads_bytes_(VmInformationLength) PVOID VmInformation,
    _In_ ULONG VmInformationLength
    );

#if (PHNT_VERSION >= PHNT_THRESHOLD)
typedef NTSTATUS (NTAPI* _LdrControlFlowGuardEnforcedWithExportSuppression)(
    VOID
    );
#endif

typedef PS_SYSTEM_DLL_INIT_BLOCK* _LdrSystemDllInitBlock;

typedef NTSTATUS (NTAPI* _LdrResFindResource)(
    _In_ PVOID DllHandle,
    _In_ PCWSTR Type,
    _In_ PCWSTR Name,
    _In_ PCWSTR Language,
    _Out_opt_ PVOID* ResourceBuffer,
    _Out_opt_ PSIZE_T ResourceLength,
    _Out_writes_bytes_opt_(CultureNameLength) PVOID CultureName, // WCHAR buffer[6]
    _Out_opt_ PULONG CultureNameLength,
    _In_opt_ ULONG Flags
    );

typedef NTSTATUS (NTAPI* _NtCreateProcessStateChange)(
    _Out_ PHANDLE ProcessStateChangeHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ POBJECT_ATTRIBUTES ObjectAttributes,
    _In_ HANDLE ProcessHandle,
    _In_opt_ ULONG64 Reserved
    );

typedef NTSTATUS (NTAPI* _NtChangeProcessState)(
    _In_ HANDLE ProcessStateChangeHandle,
    _In_ HANDLE ProcessHandle,
    _In_ PROCESS_STATE_CHANGE_TYPE StateChangeType,
    _In_opt_ PVOID ExtendedInformation,
    _In_opt_ SIZE_T ExtendedInformationLength,
    _In_opt_ ULONG64 Reserved
    );

typedef NTSTATUS (NTAPI* _NtCreateThreadStateChange)(
    _Out_ PHANDLE ThreadStateChangeHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PCOBJECT_ATTRIBUTES ObjectAttributes,
    _In_ HANDLE ThreadHandle,
    _In_opt_ _Reserved_ ULONG Reserved
    );

typedef NTSTATUS (NTAPI* _NtChangeThreadState)(
    _In_ HANDLE ThreadStateChangeHandle,
    _In_ HANDLE ThreadHandle,
    _In_ THREAD_STATE_CHANGE_TYPE StateChangeType,
    _In_opt_ PVOID ExtendedInformation,
    _In_opt_ SIZE_T ExtendedInformationLength,
    _In_opt_ ULONG64 Reserved
);

typedef NTSTATUS (NTAPI* _NtCreateThreadStateChange)(
    _Out_ PHANDLE ThreadStateChangeHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_opt_ PCOBJECT_ATTRIBUTES ObjectAttributes,
    _In_ HANDLE ThreadHandle,
    _In_opt_ _Reserved_ ULONG Reserved
    );

#if (PHNT_VERSION >= PHNT_WIN11)
typedef NTSTATUS (NTAPI* _NtCopyFileChunk)(
    _In_ HANDLE SourceHandle,
    _In_ HANDLE DestinationHandle,
    _In_opt_ HANDLE EventHandle,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_ ULONG Length,
    _In_ PLARGE_INTEGER SourceOffset,
    _In_ PLARGE_INTEGER DestOffset,
    _In_opt_ PULONG SourceKey,
    _In_opt_ PULONG DestKey,
    _In_ ULONG Flags
);
#endif

#if (PHNT_VERSION >= PHNT_REDSTONE5)
typedef NTSTATUS (NTAPI* _NtAllocateVirtualMemoryEx)(
    _In_ HANDLE ProcessHandle,
    _Inout_ _At_(*BaseAddress, _Readable_bytes_(*RegionSize) _Writable_bytes_(*RegionSize) _Post_readable_byte_size_(*RegionSize)) PVOID *BaseAddress,
    _Inout_ PSIZE_T RegionSize,
    _In_ ULONG AllocationType,
    _In_ ULONG PageProtection,
    _Inout_updates_opt_(ExtendedParameterCount) PMEM_EXTENDED_PARAMETER ExtendedParameters,
    _In_ ULONG ExtendedParameterCount
);
#endif

#if (PHNT_VERSION >= PHNT_THRESHOLD)
typedef NTSTATUS (NTAPI* _NtCompareObjects)(
    _In_ HANDLE FirstObjectHandle,
    _In_ HANDLE SecondObjectHandle
);
#endif

typedef NTSTATUS (NTAPI* _RtlDefaultNpAcl)(
    _Out_ PACL* Acl
    );

typedef NTSTATUS (NTAPI* _RtlDelayExecution)(
    _In_ BOOLEAN Alertable,
    _In_opt_ PLARGE_INTEGER DelayInterval
    );
    
typedef NTSTATUS (NTAPI* _RtlDeriveCapabilitySidsFromName)(
    _Inout_ PUNICODE_STRING UnicodeString,
    _Out_ PSID CapabilityGroupSid,
    _Out_ PSID CapabilitySid
    );
    
#if (PHNT_VERSION >= PHNT_WINDOWS_10_RS3)
// rev
typedef NTSTATUS (NTAPI* _RtlDosLongPathNameToNtPathName_U_WithStatus)(
    _In_ PCWSTR DosFileName,
    _Out_ PUNICODE_STRING NtFileName,
    _Out_opt_ PWSTR *FilePart,
    _Out_opt_ PRTL_RELATIVE_NAME_U RelativeName
    );
#endif // PHNT_VERSION >= PHNT_WINDOWS_10_RS3

typedef NTSTATUS (NTAPI* _RtlGetTokenNamedObjectPath)(
    _In_ HANDLE Token,
    _In_opt_ PSID Sid,
    _Out_ PUNICODE_STRING ObjectPath
    );

typedef NTSTATUS (NTAPI* _RtlGetAppContainerNamedObjectPath)(
    _In_opt_ HANDLE Token,
    _In_opt_ PSID AppContainerSid,
    _In_ BOOLEAN RelativePath,
    _Out_ PUNICODE_STRING ObjectPath
    );

typedef NTSTATUS (NTAPI* _RtlGetAppContainerSidType)(
    _In_ PSID AppContainerSid,
    _Out_ PAPPCONTAINER_SID_TYPE AppContainerSidType
    );

typedef NTSTATUS (NTAPI* _RtlGetAppContainerParent)(
    _In_ PSID AppContainerSid,
    _Out_ PSID* AppContainerSidParent
    );

typedef NTSTATUS (NTAPI* _RtlDeriveCapabilitySidsFromName)(
    _Inout_ PUNICODE_STRING UnicodeString,
    _Out_ PSID CapabilityGroupSid,
    _Out_ PSID CapabilitySid
    );

typedef HRESULT (WINAPI* _GetAppContainerRegistryLocation)(
    _In_ REGSAM desiredAccess,
    _Outptr_ PHKEY phAppContainerKey
    );

typedef HRESULT (WINAPI* _GetAppContainerFolderPath)(
    _In_ PCWSTR pszAppContainerSid,
    _Out_ PWSTR* ppszPath
    );

typedef NTSTATUS (NTAPI* _ConsoleControl)(
    _In_ CONSOLECONTROL Command,
    _In_reads_bytes_(ConsoleInformationLength) PVOID ConsoleInformation,
    _In_ ULONG ConsoleInformationLength
);

typedef NTSTATUS (WINAPI* _PssNtCaptureSnapshot)(
    _Out_ PHANDLE SnapshotHandle,
    _In_ HANDLE ProcessHandle,
    _In_ PSSNT_CAPTURE_FLAGS CaptureFlags,
    _In_opt_ ULONG ThreadContextFlags
    );

typedef NTSTATUS (WINAPI* _PssNtFreeSnapshot)(
    _In_ HANDLE SnapshotHandle
    );

typedef NTSTATUS (WINAPI* _PssNtFreeRemoteSnapshot)(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE SnapshotHandle
    );

typedef NTSTATUS (WINAPI* _PssNtValidateDescriptor)(
    _In_ HANDLE SnapshotHandle,
    _In_opt_ PVOID ExceptionAddress //  _ReturnAddress()
    );


typedef NTSTATUS (WINAPI* _PssNtQuerySnapshot)(
    _In_ HANDLE SnapshotHandle,
    _In_ PSSNT_QUERY_INFORMATION_CLASS InformationClass,
    _Out_writes_bytes_(BufferLength) PVOID Buffer,
    _In_ ULONG BufferLength
    );

typedef NTSTATUS (WINAPI* _NtPssCaptureVaSpaceBulk)(
    _In_ HANDLE ProcessHandle,
    _In_opt_ PVOID BaseAddress,
    _In_ PNTPSS_MEMORY_BULK_INFORMATION BulkInformation,
    _In_ SIZE_T BulkInformationLength,
    _Out_opt_ PSIZE_T ReturnLength
    );

// Advapi32

typedef BOOL (WINAPI* _ConvertSecurityDescriptorToStringSecurityDescriptorW)(
    _In_ PSECURITY_DESCRIPTOR SecurityDescriptor,
    _In_ DWORD RequestedStringSDRevision,
    _In_ SECURITY_INFORMATION SecurityInformation,
    _Outptr_ LPWSTR* StringSecurityDescriptor,
    _Out_opt_ PULONG StringSecurityDescriptorLen
    );

typedef BOOL (WINAPI* _ConvertStringSecurityDescriptorToSecurityDescriptorW)(
    _In_ LPCWSTR StringSecurityDescriptor,
    _In_ DWORD StringSDRevision,
    _Outptr_ PSECURITY_DESCRIPTOR *SecurityDescriptor,
    _Out_opt_ PULONG SecurityDescriptorSize
    );

// Cfgmgr32

typedef HRESULT (WINAPI* _DevGetObjects)(
    _In_ DEV_OBJECT_TYPE ObjectType,
    _In_ ULONG QueryFlags,
    _In_ ULONG cRequestedProperties,
    _In_reads_opt_(cRequestedProperties) const DEVPROPCOMPKEY *pRequestedProperties,
    _In_ ULONG cFilterExpressionCount,
    _In_reads_opt_(cFilterExpressionCount) const DEVPROP_FILTER_EXPRESSION *pFilter,
    _Out_ PULONG pcObjectCount,
    _Outptr_result_buffer_maybenull_(*pcObjectCount) const DEV_OBJECT **ppObjects);
	
typedef VOID (WINAPI* _DevFreeObjects)(
    _In_ ULONG cObjectCount,
    _In_reads_(cObjectCount) const DEV_OBJECT *pObjects);

typedef HRESULT (WINAPI* _DevGetObjectProperties)(
    _In_ DEV_OBJECT_TYPE ObjectType,
    _In_ PCWSTR pszObjectId,
    _In_ ULONG QueryFlags,
    _In_ ULONG cRequestedProperties,
    _In_reads_(cRequestedProperties) const DEVPROPCOMPKEY *pRequestedProperties,
    _Out_ PULONG pcPropertyCount,
    _Outptr_result_buffer_(*pcPropertyCount) const DEVPROPERTY **ppProperties);
	
	
typedef VOID (WINAPI* _DevFreeObjectProperties)(
    _In_ ULONG cPropertyCount,
    _In_reads_(cPropertyCount) const DEVPROPERTY *pProperties);
	
typedef HRESULT (WINAPI* _DevCreateObjectQuery)(
    _In_ DEV_OBJECT_TYPE ObjectType,
    _In_ ULONG QueryFlags,
    _In_ ULONG cRequestedProperties,
    _In_reads_opt_(cRequestedProperties) const DEVPROPCOMPKEY *pRequestedProperties,
    _In_ ULONG cFilterExpressionCount,
    _In_reads_opt_(cFilterExpressionCount) const DEVPROP_FILTER_EXPRESSION *pFilter,
    _In_ PDEV_QUERY_RESULT_CALLBACK pCallback,
    _In_opt_ PVOID pContext,
    _Out_ PHDEVQUERY phDevQuery);
	
	
typedef VOID (WINAPI* _DevCloseObjectQuery)(
    _In_ HDEVQUERY hDevQuery);

// Shlwapi

typedef HRESULT (WINAPI* _SHAutoComplete)(
    _In_ HWND hwndEdit,
    _In_ ULONG Flags
    );

typedef HRESULT (WINAPI* _SHCreateStreamOnFileEx)(
    _In_ LPCWSTR pszFile,
    _In_ DWORD   grfMode,
    _In_ DWORD   dwAttributes,
    _In_ BOOL    fCreate,
    _In_opt_ IStream *pstmTemplate,
    _Out_ IStream **ppstm
);

typedef ULONG (WINAPI* _PssCaptureSnapshot)(
    _In_ HANDLE ProcessHandle,
    _In_ ULONG CaptureFlags,
    _In_opt_ ULONG ThreadContextFlags,
    _Out_ HANDLE* SnapshotHandle
    );

typedef ULONG (WINAPI* _PssFreeSnapshot)(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE SnapshotHandle
    );

typedef ULONG (WINAPI* _PssQuerySnapshot)(
    _In_ HANDLE SnapshotHandle,
    _In_ ULONG InformationClass,
    _Out_writes_bytes_(BufferLength) void* Buffer,
    _In_ DWORD BufferLength
    );

typedef LONG (WINAPI* _DnsQuery_W)(
    _In_ PCWSTR Name,
    _In_ USHORT Type,
    _In_ ULONG Options,
    _Inout_opt_ PVOID Extra,
    _Outptr_result_maybenull_ PVOID* DnsRecordList,
    _Outptr_opt_result_maybenull_ PVOID* Reserved
    );

typedef LONG (WINAPI* _DnsQueryEx)(
    _In_ PVOID pQueryRequest,
    _Inout_ PVOID pQueryResults,
    _Inout_opt_ PVOID pCancelHandle
    );

typedef LONG (WINAPI* _DnsCancelQuery)(
    _In_ PVOID pCancelHandle
    );

typedef struct _DNS_MESSAGE_BUFFER* PDNS_MESSAGE_BUFFER;

typedef LONG (WINAPI* _DnsExtractRecordsFromMessage_W)(
    _In_ PDNS_MESSAGE_BUFFER DnsBuffer,
    _In_ USHORT MessageLength,
    _Out_ PVOID* DnsRecordList
    );

typedef BOOL (WINAPI* _DnsWriteQuestionToBuffer_W)(
    _Inout_ PDNS_MESSAGE_BUFFER DnsBuffer,
    _Inout_ PULONG BufferSize,
    _In_ PCWSTR Name,
    _In_ USHORT Type,
    _In_ USHORT Xid,
    _In_ BOOL RecursionDesired
    );

typedef VOID (WINAPI* _DnsFree)(
    _Pre_opt_valid_ _Frees_ptr_opt_ PVOID Data,
    _In_ ULONG FreeType
    );

// Userenv

typedef BOOL (WINAPI* _CreateEnvironmentBlock)(
    _At_((PZZWSTR*)Environment, _Outptr_) PVOID* Environment,
    _In_opt_ HANDLE TokenHandle,
    _In_ BOOL Inherit
    );

typedef BOOL (WINAPI* _DestroyEnvironmentBlock)(
    _In_ PVOID Environment
    );

// User32

typedef BOOL (WINAPI* _SetWindowDisplayAffinity)(
    _In_ HWND WindowHandle,
    _In_ ULONG Affinity
    );

typedef ULONG (WINAPI *_NotifyServiceStatusChangeW)(
    _In_ SC_HANDLE hService,
    _In_ DWORD dwNotifyMask,
    _In_ PSERVICE_NOTIFYW pNotifyBuffer
    );

typedef ULONG (WINAPI* _SubscribeServiceChangeNotifications)(
    _In_ SC_HANDLE hService,
    _In_ SC_EVENT_TYPE eEventType,
    _In_ PSC_NOTIFICATION_CALLBACK pCallback,
    _In_opt_ PVOID pCallbackContext,
    _Out_ PSC_NOTIFICATION_REGISTRATION* pSubscription
    );

typedef VOID (WINAPI* _UnsubscribeServiceChangeNotifications)(
    _In_ PSC_NOTIFICATION_REGISTRATION pSubscription
    );

// Xmllite

typedef HRESULT (STDAPICALLTYPE* _CreateXmlReader)(_In_ REFIID riid,
    _Outptr_ void ** ppvObject,
    _In_opt_ IMalloc * pMalloc);
    
typedef HRESULT (STDAPICALLTYPE* _CreateXmlWriter)(_In_ REFIID riid,
    _Out_ void ** ppvObject,
    _In_opt_ IMalloc * pMalloc);

EXTERN_C_START

#ifndef IS_TE
#define PH_DECLARE_IMPORT(Name) typeof(&(Name)) Name##_Import(VOID)
#else
#define PH_DECLARE_IMPORT(Name) _##Name NTAPI Name##_Import(VOID)
#endif

// Ntdll

PH_DECLARE_IMPORT(NtQueryInformationEnlistment);
PH_DECLARE_IMPORT(NtQueryInformationResourceManager);
PH_DECLARE_IMPORT(NtQueryInformationTransaction);
PH_DECLARE_IMPORT(NtQueryInformationTransactionManager);
PH_DECLARE_IMPORT(NtCreateProcessStateChange);
PH_DECLARE_IMPORT(NtChangeProcessState);
PH_DECLARE_IMPORT(NtCreateThreadStateChange);
PH_DECLARE_IMPORT(NtChangeThreadState);
PH_DECLARE_IMPORT(NtCopyFileChunk);
PH_DECLARE_IMPORT(NtCompareObjects);
#ifndef IS_TE
PH_DECLARE_IMPORT(NtCreateTimer2);
PH_DECLARE_IMPORT(NtSetTimer2);
#endif

PH_DECLARE_IMPORT(NtSetInformationVirtualMemory);
PH_DECLARE_IMPORT(LdrSystemDllInitBlock);
PH_DECLARE_IMPORT(LdrResFindResource);
#ifndef IS_TE
PH_DECLARE_IMPORT(LdrResSearchResource);
#endif
PH_DECLARE_IMPORT(RtlDefaultNpAcl);
PH_DECLARE_IMPORT(RtlDelayExecution);
PH_DECLARE_IMPORT(RtlDeriveCapabilitySidsFromName);
PH_DECLARE_IMPORT(RtlDosLongPathNameToNtPathName_U_WithStatus);
PH_DECLARE_IMPORT(RtlGetTokenNamedObjectPath);
PH_DECLARE_IMPORT(RtlGetAppContainerNamedObjectPath);
PH_DECLARE_IMPORT(RtlGetAppContainerSidType);
PH_DECLARE_IMPORT(RtlGetAppContainerParent);

PH_DECLARE_IMPORT(PssNtCaptureSnapshot);
PH_DECLARE_IMPORT(PssNtQuerySnapshot);
PH_DECLARE_IMPORT(PssNtFreeSnapshot);
PH_DECLARE_IMPORT(PssNtFreeRemoteSnapshot);
PH_DECLARE_IMPORT(PssNtValidateDescriptor);
PH_DECLARE_IMPORT(NtPssCaptureVaSpaceBulk);
#ifndef IS_TE
PH_DECLARE_IMPORT(TpSetPoolThreadBasePriority);
#endif

// Advapi32

PH_DECLARE_IMPORT(ConvertSecurityDescriptorToStringSecurityDescriptorW);
PH_DECLARE_IMPORT(ConvertStringSecurityDescriptorToSecurityDescriptorW);

// Cfgmgr32

PH_DECLARE_IMPORT(DevGetObjects);
PH_DECLARE_IMPORT(DevFreeObjects);
PH_DECLARE_IMPORT(DevGetObjectProperties);
PH_DECLARE_IMPORT(DevFreeObjectProperties);
PH_DECLARE_IMPORT(DevCreateObjectQuery);
PH_DECLARE_IMPORT(DevCloseObjectQuery);

// Shlwapi

//PH_DECLARE_IMPORT(SHAutoComplete);
//PH_DECLARE_IMPORT(SHCreateStreamOnFileEx);

// Userenv

PH_DECLARE_IMPORT(CreateEnvironmentBlock);
PH_DECLARE_IMPORT(DestroyEnvironmentBlock);
PH_DECLARE_IMPORT(GetAppContainerRegistryLocation);
PH_DECLARE_IMPORT(GetAppContainerFolderPath);

// User32

PH_DECLARE_IMPORT(ConsoleControl);
#ifndef IS_TE
PH_DECLARE_IMPORT(GetCurrentInputMessageSource);
PH_DECLARE_IMPORT(GetCIMSSM);
PH_DECLARE_IMPORT(NtUserBuildHwndList);
#endif

// Xmllite

PH_DECLARE_IMPORT(CreateXmlReader);
PH_DECLARE_IMPORT(CreateXmlWriter);

EXTERN_C_END

#endif
