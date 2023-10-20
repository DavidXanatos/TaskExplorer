/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022
 *     dmex    2022-2023
 *
 */

#include <ph.h>
#include <kphcomms.h>
#include <kphuser.h>
#include <mapldr.h>

#include <fltuser.h>

typedef struct _KPH_UMESSAGE
{
    FILTER_MESSAGE_HEADER MessageHeader;
    KPH_MESSAGE Message;
    OVERLAPPED Overlapped;
} KPH_UMESSAGE, *PKPH_UMESSAGE;

typedef struct _KPH_UREPLY
{
    FILTER_REPLY_HEADER ReplyHeader;
    KPH_MESSAGE Message;
} KPH_UREPLY, *PKPH_UREPLY;

PKPH_COMMS_CALLBACK KphpCommsRegisteredCallback = NULL;
HANDLE KphpCommsFltPortHandle = NULL;
BOOLEAN KphpCommsPortDisconnected = TRUE;
PTP_POOL KphpCommsThreadPool = NULL;
TP_CALLBACK_ENVIRON KphpCommsThreadPoolEnv;
PTP_IO KphpCommsThreadPoolIo = NULL;
PKPH_UMESSAGE KphpCommsMessages = NULL;
ULONG KphpCommsMessageCount = 0;
PH_RUNDOWN_PROTECT KphpCommsRundown;
PH_FREE_LIST KphpCommsReplyFreeList;

#define KPH_COMMS_MIN_THREADS   2
#define KPH_COMMS_MESSAGE_SCALE 2
#define KPH_COMMS_THREAD_SCALE  2
#define KPH_COMMS_MAX_MESSAGES  1024

// rev
typedef struct _FILTER_PORT_EA
{
    PUNICODE_STRING PortName;
    PUNICODE_STRING64 PortName64;
    USHORT SizeOfContext;
    BYTE Padding[6]; // not-used (uninitialized heap bytes)
    BYTE ConnectionContext[ANYSIZE_ARRAY];
} FILTER_PORT_EA, *PFILTER_PORT_EA;

#define FLT_PORT_CONTEXT_MAX 0xFFE8

// FILE_FULL_EA_INFORMATION (symbols)
typedef struct _FILTER_PORT_FULL_EA
{
    ULONG NextEntryOffset; // 0
    UCHAR Flags;           // 0
    UCHAR EaNameLength;    // sizeof(FLT_PORT_EA_NAME) - sizeof(ANSI_NULL)
    USHORT EaValueLength;  // RTL_SIZEOF_THROUGH_FIELD(FILTER_PORT_EA, Padding) + SizeOfContext
    CHAR EaName[8];        // FLTPORT\0
    FILTER_PORT_EA EaValue;
} FILTER_PORT_FULL_EA, *PFILTER_PORT_FULL_EA;

#define FLT_PORT_EA_NAME "FLTPORT"

#define FILTER_PORT_EA_SIZE \
    (sizeof(FILE_FULL_EA_INFORMATION) + (sizeof(FLT_PORT_EA_NAME) - sizeof(ANSI_NULL)))
#define FILTER_PORT_EA_VALUE_SIZE \
    RTL_SIZEOF_THROUGH_FIELD(FILTER_PORT_EA, Padding)
//#define FILTER_PORT_EA_VALUE_OFFSET \
//    (FIELD_OFFSET(FILE_FULL_EA_INFORMATION, EaName) + sizeof(FLT_PORT_EA_NAME))
//#define FILTER_PORT_EA_VALUE_SIZE \
//    (FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.ConnectionContext) - FILTER_PORT_EA_VALUE_OFFSET)

#ifdef _WIN64
C_ASSERT(FILTER_PORT_EA_SIZE == 19); // 0x13
C_ASSERT(FILTER_PORT_EA_VALUE_SIZE == 24); // 0x18
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.PortName) == 16); // 0x10
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.PortName64) == 24); // 0x18
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.SizeOfContext) == 32); // 0x20
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.ConnectionContext) == 40); // 0x28
#else
C_ASSERT(FILTER_PORT_EA_SIZE == 19); // 0x13
C_ASSERT(FILTER_PORT_EA_VALUE_SIZE == 16); // 0x18
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.PortName) == 16); // 0x10
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.PortName64) == 20); // 0x14
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.SizeOfContext) == 24); // 0x18
C_ASSERT(FIELD_OFFSET(FILTER_PORT_FULL_EA, EaValue.ConnectionContext) == 32); // 0x20
#endif

typedef struct _FILTER_LOADUNLOAD
{
    USHORT Length;
    WCHAR Name[ANYSIZE_ARRAY];
} FILTER_LOADUNLOAD, *PFILTER_LOADUNLOAD;

#define FLT_CTL_LOAD                CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 1, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_UNLOAD              CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 2, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_CREATE              CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 3, METHOD_BUFFERED, FILE_READ_ACCESS)
#define FLT_CTL_ATTACH              CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 4, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_DETATCH             CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 5, METHOD_BUFFERED, FILE_WRITE_ACCESS)
#define FLT_CTL_SEND_MESSAGE        CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 6, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define FLT_CTL_GET_MESSAGE         CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 7, METHOD_NEITHER, FILE_READ_ACCESS)
#define FLT_CTL_REPLY_MESSAGE       CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 8, METHOD_NEITHER, FILE_WRITE_ACCESS)
#define FLT_CTL_FIND_FIRST          CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 9, METHOD_BUFFERED, FILE_READ_ACCESS)
#define FLT_CTL_FIND_NEXT           CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 10, METHOD_BUFFERED, FILE_READ_ACCESS)
#define FLT_CTL_QUERY_INFORMATION   CTL_CODE(FILE_DEVICE_DISK_FILE_SYSTEM, 11, METHOD_BUFFERED, FILE_READ_ACCESS)

/**
 * \brief Wrapper which is essentially FilterpDeviceIoControl.
 *
 * \param[in] Handle Filter port handle.
 * \param[in] IoControlCode Filter I/O control code
 * \param[in] InBuffer Input buffer.
 * \param[in] InBufferSize Input buffer size.
 * \param[out] OutputBuffer Output Buffer.
 * \param[in] OutputBufferSize Output buffer size.
 * \param[out] BytesReturned Optionally set to the number of bytes returned.
 * \param[in,out] Overlapped Optional overlapped structure.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphpFilterDeviceIoControl(
    _In_ HANDLE Handle,
    _In_ ULONG IoControlCode,
    _In_reads_bytes_(InBufferSize) PVOID InBuffer,
    _In_ ULONG InBufferSize,
    _Out_writes_bytes_to_opt_(OutputBufferSize, *BytesReturned) PVOID OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_opt_ PULONG BytesReturned,
    _Inout_opt_ LPOVERLAPPED Overlapped
    )
{
    NTSTATUS status;

    if (BytesReturned)
    {
        *BytesReturned = 0;
    }

    if (Overlapped)
    {
        Overlapped->Internal = STATUS_PENDING;

        if (DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM)
        {
            status = NtFsControlFile(Handle,
                                     Overlapped->hEvent,
                                     NULL,
                                     Overlapped,
                                     (PIO_STATUS_BLOCK)Overlapped,
                                     IoControlCode,
                                     InBuffer,
                                     InBufferSize,
                                     OutputBuffer,
                                     OutputBufferSize);
        }
        else
        {
            status = NtDeviceIoControlFile(Handle,
                                           Overlapped->hEvent,
                                           NULL,
                                           Overlapped,
                                           (PIO_STATUS_BLOCK)Overlapped,
                                           IoControlCode,
                                           InBuffer,
                                           InBufferSize,
                                           OutputBuffer,
                                           OutputBufferSize);
        }

        if (NT_INFORMATION(status) && BytesReturned)
        {
            *BytesReturned = (ULONG)Overlapped->InternalHigh;
        }
    }
    else
    {
        IO_STATUS_BLOCK ioStatusBlock;

        if (DEVICE_TYPE_FROM_CTL_CODE(IoControlCode) == FILE_DEVICE_FILE_SYSTEM)
        {
            status = NtFsControlFile(Handle,
                                     NULL,
                                     NULL,
                                     NULL,
                                     &ioStatusBlock,
                                     IoControlCode,
                                     InBuffer,
                                     InBufferSize,
                                     OutputBuffer,
                                     OutputBufferSize);
        }
        else
        {
            status = NtDeviceIoControlFile(Handle,
                                           NULL,
                                           NULL,
                                           NULL,
                                           &ioStatusBlock,
                                           IoControlCode,
                                           InBuffer,
                                           InBufferSize,
                                           OutputBuffer,
                                           OutputBufferSize);
        }

        if (status == STATUS_PENDING)
        {
            status = NtWaitForSingleObject(Handle, FALSE, NULL);
            if (NT_SUCCESS(status))
            {
                status = ioStatusBlock.Status;
            }
        }

        if (BytesReturned)
        {
            *BytesReturned = (ULONG)ioStatusBlock.Information;
        }
    }

    return status;
}

// rev from FilterLoad/FilterUnload (fltlib) (dmex)
/**
 * \brief An application with minifilter support can dynamically load and unload the minifilter.
 *
 * \param[in] ServiceName The service name from HKLM\\System\\CurrentControlSet\\Services\\<ServiceName>.
 * \param[in] LoadDriver TRUE to load the kernel driver, FALSE to unload the kernel driver.
 *
 * \remarks The caller must have SE_LOAD_DRIVER_PRIVILEGE.
 * \remarks This ioctl is a kernel wrapper around NtLoadDriver and NtUnloadDriver.
 *
 * \return Successful or errant status.
 */
NTSTATUS KphFilterLoadUnload(
    _In_ PPH_STRINGREF ServiceName,
    _In_ BOOLEAN LoadDriver
    )
{
    NTSTATUS status;
    HANDLE fileHandle;
    UNICODE_STRING objectName;
    OBJECT_ATTRIBUTES objectAttributes;
    IO_STATUS_BLOCK ioStatusBlock;
    ULONG filterNameBufferLength;
    PFILTER_LOADUNLOAD filterNameBuffer;
    SECURITY_QUALITY_OF_SERVICE filterSecurityQos =
    {
        sizeof(SECURITY_QUALITY_OF_SERVICE),
        SecurityImpersonation,
        SECURITY_DYNAMIC_TRACKING,
        TRUE
    };

    RtlInitUnicodeString(&objectName, L"\\FileSystem\\Filters\\FltMgr");
    InitializeObjectAttributes(
        &objectAttributes,
        &objectName,
        OBJ_CASE_INSENSITIVE | (WindowsVersion < WINDOWS_10 ? 0 : OBJ_DONT_REPARSE),
        NULL,
        NULL
        );
    objectAttributes.SecurityQualityOfService = &filterSecurityQos;

    status = NtCreateFile(
        &fileHandle,
        FILE_READ_ATTRIBUTES | GENERIC_WRITE | SYNCHRONIZE,
        &objectAttributes,
        &ioStatusBlock,
        NULL,
        FILE_ATTRIBUTE_NORMAL,
        FILE_SHARE_WRITE,
        FILE_OPEN,
        FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
        NULL,
        0
        );

    if (!NT_SUCCESS(status))
        return status;

    filterNameBufferLength = UFIELD_OFFSET(FILTER_LOADUNLOAD, Name[ServiceName->Length]) + sizeof(UNICODE_NULL);
    filterNameBuffer = PhAllocateZero(filterNameBufferLength);
    filterNameBuffer->Length = (USHORT)ServiceName->Length;
    RtlCopyMemory(filterNameBuffer->Name, ServiceName->Buffer, ServiceName->Length);

    status = NtDeviceIoControlFile(
        fileHandle,
        NULL,
        NULL,
        NULL,
        &ioStatusBlock,
        LoadDriver ? FLT_CTL_LOAD : FLT_CTL_UNLOAD,
        filterNameBuffer,
        filterNameBufferLength,
        NULL,
        0
        );

    NtClose(fileHandle);
    PhFree(filterNameBuffer);

    return status;
}

/**
 * \brief Wrapper which is essentially FilterSendMessage.
 *
 * \param[in] Port Filter port handle.
 * \param[in] InBuffer Input buffer.
 * \param[in] InBufferSize Input buffer size.
 * \param[out] OutputBuffer Output Buffer.
 * \param[in] OutputBufferSize Output buffer size.
 * \param[out] BytesReturned Set to the number of bytes returned.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphpFilterSendMessage(
    _In_ HANDLE Port,
    _In_reads_bytes_(InBufferSize) PVOID InBuffer,
    _In_ ULONG InBufferSize,
    _Out_writes_bytes_to_opt_(OutputBufferSize, *BytesReturned) PVOID OutputBuffer,
    _In_ ULONG OutputBufferSize,
    _Out_ PULONG BytesReturned
    )
{
    return KphpFilterDeviceIoControl(Port,
                                     FLT_CTL_SEND_MESSAGE,
                                     InBuffer,
                                     InBufferSize,
                                     OutputBuffer,
                                     OutputBufferSize,
                                     BytesReturned,
                                     NULL);
}

/**
 * \brief Wrapper which is essentially FilterGetMessage.
 *
 * \param[in] Port Filter port handle.
 * \param[out] MessageBuffer Message buffer.
 * \param[in] MessageBufferSize Message buffer size.
 * \param[in] Overlapped Th overlapped structure.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphpFilterGetMessage(
    _In_ HANDLE Port,
    _Out_writes_bytes_(MessageBufferSize) PFILTER_MESSAGE_HEADER MessageBuffer,
    _In_ ULONG MessageBufferSize,
    _Inout_ LPOVERLAPPED Overlapped
    )
{
    return KphpFilterDeviceIoControl(Port,
                                     FLT_CTL_GET_MESSAGE,
                                     NULL,
                                     0,
                                     MessageBuffer,
                                     MessageBufferSize,
                                     NULL,
                                     Overlapped);
}

/**
 * \brief Wrapper which is essentially FilterReplyMessage.
 *
 * \param[in] Port Filter port handle.
 * \param[in] ReplyBuffer Reply buffer.
 * \param[in] ReplyBufferSize Reply buffer size.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphpFilterReplyMessage(
    _In_ HANDLE Port,
    _In_reads_bytes_(ReplyBufferSize) PFILTER_REPLY_HEADER ReplyBuffer,
    _In_ ULONG ReplyBufferSize
    )
{
    return KphpFilterDeviceIoControl(Port,
                                     FLT_CTL_REPLY_MESSAGE,
                                     ReplyBuffer,
                                     ReplyBufferSize,
                                     NULL,
                                     0,
                                     NULL,
                                     NULL);
}

/**
 * \brief Wrapper which is essentially FilterConnectCommunicationPort.
 *
 * \param[in] PortName Name of filter port to connect to.
 * \param[in] Options Filter port options.
 * \param[in] ConnectionContext Connection context.
 * \param[in] SizeOfContext Size of connection context.
 * \param[in] SecurityAttributes Security attributes for handle.
 * \param[out] Port Set to filter port handle on success, null on failure.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphpFilterConnectCommunicationPort(
    _In_ PPH_STRINGREF PortName,
    _In_ ULONG Options,
    _In_reads_bytes_opt_(SizeOfContext) PVOID ConnectionContext,
    _In_ USHORT SizeOfContext,
    _In_opt_ PSECURITY_ATTRIBUTES SecurityAttributes,
    _Outptr_ HANDLE *Port
    )
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objectAttributes;
    UNICODE_STRING objectName;
    UNICODE_STRING portName;
    UNICODE_STRING64 portName64;
    ULONG eaLength;
    PFILE_FULL_EA_INFORMATION ea;
    PFILTER_PORT_EA eaValue;
    IO_STATUS_BLOCK isb;

    *Port = NULL;

    if ((SizeOfContext > 0 && !ConnectionContext) ||
        (SizeOfContext == 0 && ConnectionContext))
    {
        return STATUS_INVALID_PARAMETER;
    }

    if (SizeOfContext >= FLT_PORT_CONTEXT_MAX)
        return STATUS_INTEGER_OVERFLOW;

    if (!PhStringRefToUnicodeString(PortName, &portName))
        return STATUS_NAME_TOO_LONG;

    portName64.Buffer = (ULONGLONG)portName.Buffer;
    portName64.Length = portName.Length;
    portName64.MaximumLength = portName.MaximumLength;

    //
    // Build the filter EA, this contains the port name and the context.
    //

    eaLength = FILTER_PORT_EA_SIZE
             + FILTER_PORT_EA_VALUE_SIZE
             + SizeOfContext;

    ea = PhAllocateZeroSafe(eaLength);
    if (!ea)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    ea->Flags = 0;
    ea->EaNameLength = sizeof(FLT_PORT_EA_NAME) - sizeof(ANSI_NULL);
    ea->EaValueLength = FILTER_PORT_EA_VALUE_SIZE + SizeOfContext;
    RtlCopyMemory(ea->EaName, FLT_PORT_EA_NAME, sizeof(FLT_PORT_EA_NAME));
    eaValue = PTR_ADD_OFFSET(ea->EaName, sizeof(FLT_PORT_EA_NAME));
    eaValue->PortName = &portName;
    eaValue->PortName64 = &portName64;
    eaValue->SizeOfContext = SizeOfContext;

    if (SizeOfContext > 0)
    {
        RtlCopyMemory(eaValue->ConnectionContext,
                      ConnectionContext,
                      SizeOfContext);
    }

    RtlInitUnicodeString(&objectName, L"\\FileSystem\\Filters\\FltMgrMsg");
    InitializeObjectAttributes(&objectAttributes,
                               &objectName,
                               OBJ_CASE_INSENSITIVE | (WindowsVersion < WINDOWS_10 ? 0 : OBJ_DONT_REPARSE),
                               NULL,
                               NULL);
    if (SecurityAttributes)
    {
        if (SecurityAttributes->bInheritHandle)
        {
            objectAttributes.Attributes |= OBJ_INHERIT;
        }
        objectAttributes.SecurityDescriptor = SecurityAttributes->lpSecurityDescriptor;
    }

    status = NtCreateFile(Port,
                          FILE_READ_ACCESS | FILE_WRITE_ACCESS | SYNCHRONIZE,
                          &objectAttributes,
                          &isb,
                          NULL,
                          0,
                          0,
                          FILE_OPEN_IF,
                          Options & FLT_PORT_FLAG_SYNC_HANDLE ? FILE_SYNCHRONOUS_IO_NONALERT : 0,
                          ea,
                          eaLength);

    PhFree(ea);

    return status;
}

/**
 * \brief No-operation communications callback handling only the minimum
 *  necessary. Used when no callback is provided when connecting. Or when the
 *  callback does not handle the message.
 *
 * \details Synchronous messages expecting a reply must always be handled, this
 * no-operation default callback will handle them as necessary.
 *
 * \param[in] ReplyToken - Token used to reply, when possible.
 * \param[in] Message - Message from KPH to handle.
 */
VOID KphpCommsCallbackUnhandled(
    _In_ ULONG_PTR ReplyToken,
    _In_ PCKPH_MESSAGE Message
    )
{
    PPH_FREE_LIST freelist;
    PKPH_MESSAGE msg;

    if (Message->Header.MessageId != KphMsgProcessCreate)
    {
        return;
    }

    freelist = KphGetMessageFreeList();

    msg = PhAllocateFromFreeList(freelist);
    KphMsgInit(msg, KphMsgProcessCreate);
    msg->Reply.ProcessCreate.CreationStatus = STATUS_SUCCESS;
    KphCommsReplyMessage(ReplyToken, msg);

    PhFreeToFreeList(freelist, msg);
}

/**
 * \brief I/O thread pool callback for filter port.
 *
 * \param[in,out] Instance Unused
 * \param[in,out] Context Unused
 * \param[in,out] ApcContext Pointer to overlapped I/O that was completed.
 * \param[in] IoSB Result of the asynchronous I/O operation.
 * \param[in,out] Io Unused
 */
VOID WINAPI KphpCommsIoCallback(
    _Inout_ PTP_CALLBACK_INSTANCE Instance,
    _Inout_opt_ PVOID Context,
    _In_ PVOID ApcContext,
    _In_ PIO_STATUS_BLOCK IoSB,
    _In_ PTP_IO Io
    )
{
    NTSTATUS status;
    PKPH_UMESSAGE msg;
    BOOLEAN handled;

    if (!PhAcquireRundownProtection(&KphpCommsRundown))
    {
        return;
    }

    msg = CONTAINING_RECORD(ApcContext, KPH_UMESSAGE, Overlapped);

    if (IoSB->Status != STATUS_SUCCESS)
    {
        goto Exit;
    }

    if (IoSB->Information < KPH_MESSAGE_MIN_SIZE)
    {
        assert(FALSE);
        goto Exit;
    }

    status = KphMsgValidate(&msg->Message);
    if (!NT_SUCCESS(status))
    {
        assert(FALSE);
        goto Exit;
    }

    if (KphpCommsRegisteredCallback)
    {
        handled = KphpCommsRegisteredCallback((ULONG_PTR)&msg->MessageHeader,
                                              &msg->Message);
    }
    else
    {
        handled = FALSE;
    }

    if (!handled)
    {
        KphpCommsCallbackUnhandled((ULONG_PTR)&msg->MessageHeader,
                                   &msg->Message);
    }

Exit:

    RtlZeroMemory(&msg->Overlapped, FIELD_OFFSET(OVERLAPPED, hEvent));

    TpStartAsyncIoOperation(KphpCommsThreadPoolIo);

    status = KphpFilterGetMessage(KphpCommsFltPortHandle,
                                  &msg->MessageHeader,
                                  FIELD_OFFSET(KPH_UMESSAGE, Overlapped),
                                  &msg->Overlapped);

    if (status != STATUS_PENDING)
    {
        if (status == STATUS_PORT_DISCONNECTED)
        {
            //
            // Mark the port disconnected so KphCommsIsConnected returns false.
            // This can happen if the driver goes away before the client.
            //
            KphpCommsPortDisconnected = TRUE;
        }
        else
        {
            assert(FALSE);
        }

        TpCancelAsyncIoOperation(KphpCommsThreadPoolIo);
    }

    PhReleaseRundownProtection(&KphpCommsRundown);
}

static VOID KphpTpSetPoolThreadBasePriority(
    _Inout_ PTP_POOL Pool,
    _In_ ULONG BasePriority
    )
{
    static PH_INITONCE initOnce = PH_INITONCE_INIT;
    static NTSTATUS (NTAPI* TpSetPoolThreadBasePriority_I)(
        _Inout_ PTP_POOL Pool,
        _In_ ULONG BasePriority
        ) = NULL;

    if (PhBeginInitOnce(&initOnce))
    {
        PVOID baseAddress;

        if (baseAddress = PhGetLoaderEntryDllBaseZ(L"ntdll.dll"))
        {
            TpSetPoolThreadBasePriority_I = PhGetDllBaseProcedureAddress(baseAddress, "TpSetPoolThreadBasePriority", 0);
        }

        PhEndInitOnce(&initOnce);
    }

    if (TpSetPoolThreadBasePriority_I)
    {
        TpSetPoolThreadBasePriority_I(Pool, BasePriority);
    }
}

/**
 * \brief Starts the communications infrastructure.
 *
 * \param[in] PortName Communication port name.
 * \param[in] Callback Communication callback for receiving (and replying to)
 * messages from the driver.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphCommsStart(
    _In_ PPH_STRINGREF PortName,
    _In_opt_ PKPH_COMMS_CALLBACK Callback
    )
{
    NTSTATUS status;
    ULONG numberOfThreads;

    if (KphpCommsFltPortHandle)
    {
        status = STATUS_ALREADY_INITIALIZED;
        goto Exit;
    }

    status = KphpFilterConnectCommunicationPort(PortName,
                                                0,
                                                NULL,
                                                0,
                                                NULL,
                                                &KphpCommsFltPortHandle);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    KphpCommsPortDisconnected = FALSE;
    KphpCommsRegisteredCallback = Callback;

    PhInitializeRundownProtection(&KphpCommsRundown);
    PhInitializeFreeList(&KphpCommsReplyFreeList, sizeof(KPH_UREPLY), 16);

    if (PhSystemProcessorInformation.NumberOfProcessors >= KPH_COMMS_MIN_THREADS)
        numberOfThreads = PhSystemProcessorInformation.NumberOfProcessors * KPH_COMMS_THREAD_SCALE;
    else
        numberOfThreads = KPH_COMMS_MIN_THREADS;

    KphpCommsMessageCount = numberOfThreads * KPH_COMMS_MESSAGE_SCALE;
    if (KphpCommsMessageCount > KPH_COMMS_MAX_MESSAGES)
        KphpCommsMessageCount = KPH_COMMS_MAX_MESSAGES;

    status = TpAllocPool(&KphpCommsThreadPool, NULL);
    if (!NT_SUCCESS(status))
        goto Exit;

    TpSetPoolMinThreads(KphpCommsThreadPool, KPH_COMMS_MIN_THREADS);
    TpSetPoolMaxThreads(KphpCommsThreadPool, numberOfThreads);
    KphpTpSetPoolThreadBasePriority(KphpCommsThreadPool, THREAD_PRIORITY_HIGHEST);

    TpInitializeCallbackEnviron(&KphpCommsThreadPoolEnv);
    TpSetCallbackNoActivationContext(&KphpCommsThreadPoolEnv);
    TpSetCallbackPriority(&KphpCommsThreadPoolEnv, TP_CALLBACK_PRIORITY_HIGH);
    TpSetCallbackThreadpool(&KphpCommsThreadPoolEnv, KphpCommsThreadPool);
    //TpSetCallbackLongFunction(&KphpCommsThreadPoolEnv);

    PhSetFileCompletionNotificationMode(KphpCommsFltPortHandle,
                                        FILE_SKIP_SET_EVENT_ON_HANDLE);

    status = TpAllocIoCompletion(&KphpCommsThreadPoolIo,
                                 KphpCommsFltPortHandle,
                                 KphpCommsIoCallback,
                                 NULL,
                                 &KphpCommsThreadPoolEnv
                                 );
    if (!NT_SUCCESS(status))
        goto Exit;

    KphpCommsMessages = PhAllocateZeroSafe(sizeof(KPH_UMESSAGE) * KphpCommsMessageCount);
    if (!KphpCommsMessages)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    for (ULONG i = 0; i < KphpCommsMessageCount; i++)
    {
        status = NtCreateEvent(&KphpCommsMessages[i].Overlapped.hEvent,
                               EVENT_ALL_ACCESS,
                               NULL,
                               NotificationEvent,
                               FALSE);

        if (!NT_SUCCESS(status))
            goto Exit;

        RtlZeroMemory(&KphpCommsMessages[i].Overlapped,
            FIELD_OFFSET(OVERLAPPED, hEvent));

        TpStartAsyncIoOperation(KphpCommsThreadPoolIo);

        status = KphpFilterGetMessage(KphpCommsFltPortHandle,
                                      &KphpCommsMessages[i].MessageHeader,
                                      FIELD_OFFSET(KPH_UMESSAGE, Overlapped),
                                      &KphpCommsMessages[i].Overlapped);
        if (status == STATUS_PENDING)
        {
            status = STATUS_SUCCESS;
        }
        else
        {
            status = STATUS_FLT_INTERNAL_ERROR;
            goto Exit;
        }
    }

    status = STATUS_SUCCESS;

Exit:
    if (!NT_SUCCESS(status))
    {
        KphCommsStop();
    }

    return status;
}

/**
 * \brief Stops the communications infrastructure.
 */
VOID KphCommsStop(
    VOID
    )
{
    if (!KphpCommsFltPortHandle)
    {
        return;
    }

    PhWaitForRundownProtection(&KphpCommsRundown);

    if (KphpCommsThreadPoolIo)
    {
        TpWaitForIoCompletion(KphpCommsThreadPoolIo, TRUE);
        TpReleaseIoCompletion(KphpCommsThreadPoolIo);
        KphpCommsThreadPoolIo = NULL;
    }

    TpDestroyCallbackEnviron(&KphpCommsThreadPoolEnv);

    if (KphpCommsThreadPool)
    {
        TpReleasePool(KphpCommsThreadPool);
        KphpCommsThreadPool = NULL;
    }

    KphpCommsRegisteredCallback = NULL;
    KphpCommsPortDisconnected = TRUE;

    if (KphpCommsFltPortHandle)
    {
        NtClose(KphpCommsFltPortHandle);
        KphpCommsFltPortHandle = NULL;
    }

    if (KphpCommsMessages)
    {
        for (ULONG i = 0; i < KphpCommsMessageCount; i++)
        {
            if (KphpCommsMessages[i].Overlapped.hEvent)
            {
                NtClose(KphpCommsMessages[i].Overlapped.hEvent);
                KphpCommsMessages[i].Overlapped.hEvent = NULL;
            }
        }

        KphpCommsMessageCount = 0;

        PhFree(KphpCommsMessages);
        KphpCommsMessages = NULL;
    }

    PhDeleteFreeList(&KphpCommsReplyFreeList);
}

/**
 * \brief Checks if communications is connected to the driver.
 *
 * @return TRUE if connected, FALSE otherwise.
 */
BOOLEAN KphCommsIsConnected(
    VOID
    )
{
    return (KphpCommsFltPortHandle && !KphpCommsPortDisconnected);
}

/**
 * \brief Replies to a message send to the communications callback.
 *
 * \param[in] ReplyToken Reply token from the communications callback.
 * \param[in] Message The message to reply with.
 *
 * \return Successful or errant status.
 */
NTSTATUS KphCommsReplyMessage(
    _In_ ULONG_PTR ReplyToken,
    _In_ PKPH_MESSAGE Message
    )
{
    NTSTATUS status;
    PKPH_UREPLY reply;
    PFILTER_MESSAGE_HEADER header;

    reply = NULL;
    header = (PFILTER_MESSAGE_HEADER)ReplyToken;

    if (!KphpCommsFltPortHandle)
    {
        status = STATUS_FLT_NOT_INITIALIZED;
        goto Exit;
    }

    if (!header || (header->ReplyLength == 0))
    {
        //
        // Kernel is not expecting a reply.
        //
        status = STATUS_INVALID_PARAMETER_1;
        goto Exit;
    }

    status = KphMsgValidate(Message);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    reply = PhAllocateFromFreeList(&KphpCommsReplyFreeList);
    if (!reply)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    RtlZeroMemory(reply, sizeof(KPH_UREPLY));
    RtlCopyMemory(&reply->ReplyHeader, header, sizeof(reply->ReplyHeader));
    RtlCopyMemory(&reply->Message, Message, Message->Header.Size);

    status = KphpFilterReplyMessage(
                           KphpCommsFltPortHandle,
                           &reply->ReplyHeader,
                           sizeof(reply->ReplyHeader) + Message->Header.Size);

    if (status == STATUS_PORT_DISCONNECTED)
    {
        //
        // Mark the port disconnected so KphCommsIsConnected returns false.
        // This can happen if the driver goes away before the client.
        //
        KphpCommsPortDisconnected = TRUE;
    }

Exit:

    if (reply)
    {
        PhFreeToFreeList(&KphpCommsReplyFreeList, reply);
    }

    return status;
}

/**
 * \brief Sends a message to the driver (and may receive output if applicable).
 *
 * \param[in,out] Message The message to send to driver, on success this message
 * contains the output from the driver.
 *
 * \return Successful or errant status.
 */
_Must_inspect_result_
NTSTATUS KphCommsSendMessage(
    _Inout_ PKPH_MESSAGE Message
    )
{
    NTSTATUS status;
    ULONG bytesReturned;

    if (!KphpCommsFltPortHandle)
    {
        return STATUS_FLT_NOT_INITIALIZED;
    }

    status = KphMsgValidate(Message);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = KphpFilterSendMessage(KphpCommsFltPortHandle,
                                   Message,
                                   sizeof(KPH_MESSAGE),
                                   NULL,
                                   0,
                                   &bytesReturned);

    if (status == STATUS_PORT_DISCONNECTED)
    {
        //
        // Mark the port disconnected so KphCommsIsConnected returns false.
        // This can happen if the driver goes away before the client.
        //
        KphpCommsPortDisconnected = TRUE;
    }

    return status;
}
