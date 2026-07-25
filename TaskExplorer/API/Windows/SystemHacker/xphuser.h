#ifndef _PH_XPHUSER_H
#define _PH_XPHUSER_H

#include "../../../KSystemHacker/include/kphapi.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _KPH_PARAMETERS
{
    KPH_SECURITY_LEVEL SecurityLevel;
    BOOLEAN CreateDynamicConfiguration;
} KPH_PARAMETERS, *PKPH_PARAMETERS;


NTSTATUS
NTAPI
XphConnect(
    _In_opt_ PWSTR DeviceName
    );


NTSTATUS
NTAPI
XphConnect2(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    );


NTSTATUS
NTAPI
XphConnect2Ex(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKPH_PARAMETERS Parameters
    );


NTSTATUS
NTAPI
XphDisconnect(
    VOID
    );


BOOLEAN
NTAPI
XphIsConnected(
    VOID
    );


BOOLEAN
NTAPI
XphIsVerified(
    VOID
    );


NTSTATUS
NTAPI
XphSetParameters(
    _In_opt_ PWSTR DeviceName,
    _In_ PKPH_PARAMETERS Parameters
    );


NTSTATUS
NTAPI
XphResetParameters(
    _In_opt_ PWSTR DeviceName
    );


VOID
NTAPI
XphSetServiceSecurity(
    _In_ SC_HANDLE ServiceHandle
    );


NTSTATUS
NTAPI
XphInstall(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName
    );


NTSTATUS
NTAPI
XphInstallEx(
    _In_opt_ PWSTR DeviceName,
    _In_ PWSTR FileName,
    _In_opt_ PKPH_PARAMETERS Parameters
    );


NTSTATUS
NTAPI
XphUninstall(
    _In_opt_ PWSTR DeviceName
    );


NTSTATUS
NTAPI
XphGetFeatures(
    _Inout_ PULONG Features
    );


NTSTATUS
NTAPI
XphVerifyClient(
    _In_reads_bytes_(SignatureSize) PUCHAR Signature,
    _In_ ULONG SignatureSize
    );


NTSTATUS
NTAPI
XphOpenProcess(
    _Inout_ PHANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    );


NTSTATUS
NTAPI
XphOpenProcessToken(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE TokenHandle
    );


NTSTATUS
NTAPI
XphOpenProcessJob(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE JobHandle
    );


NTSTATUS
NTAPI
XphTerminateProcess(
    _In_ HANDLE ProcessHandle,
    _In_ NTSTATUS ExitStatus
    );


NTSTATUS
NTAPI
XphReadVirtualMemoryUnsafe(
    _In_opt_ HANDLE ProcessHandle,
    _In_ PVOID BaseAddress,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferSize,
    _Inout_opt_ PSIZE_T NumberOfBytesRead
    );


NTSTATUS
NTAPI
XphQueryInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _Out_writes_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength,
    _Inout_opt_ PULONG ReturnLength
    );


NTSTATUS
NTAPI
XphSetInformationProcess(
    _In_ HANDLE ProcessHandle,
    _In_ KPH_PROCESS_INFORMATION_CLASS ProcessInformationClass,
    _In_reads_bytes_(ProcessInformationLength) PVOID ProcessInformation,
    _In_ ULONG ProcessInformationLength
    );


NTSTATUS
NTAPI
XphOpenThread(
    _Inout_ PHANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ PCLIENT_ID ClientId
    );


NTSTATUS
NTAPI
XphOpenThreadProcess(
    _In_ HANDLE ThreadHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _Inout_ PHANDLE ProcessHandle
    );


NTSTATUS
NTAPI
XphCaptureStackBackTraceThread(
    _In_ HANDLE ThreadHandle,
    _In_ ULONG FramesToSkip,
    _In_ ULONG FramesToCapture,
    _Out_writes_(FramesToCapture) PVOID *BackTrace,
    _Inout_opt_ PULONG CapturedFrames,
    _Inout_opt_ PULONG BackTraceHash
    );


NTSTATUS
NTAPI
XphQueryInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _Out_writes_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength,
    _Inout_opt_ PULONG ReturnLength
    );


NTSTATUS
NTAPI
XphSetInformationThread(
    _In_ HANDLE ThreadHandle,
    _In_ KPH_THREAD_INFORMATION_CLASS ThreadInformationClass,
    _In_reads_bytes_(ThreadInformationLength) PVOID ThreadInformation,
    _In_ ULONG ThreadInformationLength
    );


NTSTATUS
NTAPI
XphEnumerateProcessHandles(
    _In_ HANDLE ProcessHandle,
    _Out_writes_bytes_(BufferLength) PVOID Buffer,
    _In_opt_ ULONG BufferLength,
    _Inout_opt_ PULONG ReturnLength
    );


NTSTATUS
NTAPI
XphEnumerateProcessHandles2(
    _In_ HANDLE ProcessHandle,
    _Out_ PKPH_PROCESS_HANDLE_INFORMATION *Handles
    );


NTSTATUS
NTAPI
XphQueryInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _Out_writes_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength,
    _Inout_opt_ PULONG ReturnLength
    );


NTSTATUS
NTAPI
XphSetInformationObject(
    _In_ HANDLE ProcessHandle,
    _In_ HANDLE Handle,
    _In_ KPH_OBJECT_INFORMATION_CLASS ObjectInformationClass,
    _In_reads_bytes_(ObjectInformationLength) PVOID ObjectInformation,
    _In_ ULONG ObjectInformationLength
    );


NTSTATUS
NTAPI
XphOpenDriver(
    _Inout_ PHANDLE DriverHandle,
    _In_ ACCESS_MASK DesiredAccess,
    _In_ POBJECT_ATTRIBUTES ObjectAttributes
    );


NTSTATUS
NTAPI
XphQueryInformationDriver(
    _In_ HANDLE DriverHandle,
    _In_ DRIVER_INFORMATION_CLASS DriverInformationClass,
    _Out_writes_bytes_(DriverInformationLength) PVOID DriverInformation,
    _In_ ULONG DriverInformationLength,
    _Inout_opt_ PULONG ReturnLength
    );

// kphdata


#ifdef XPH_USE_DYN_DATA
NTSTATUS
NTAPI
XphInitializeDynamicPackage(
    _Out_ PKPH_DYN_PACKAGE Package
    );
#endif

#ifdef __cplusplus
}
#endif

#endif
