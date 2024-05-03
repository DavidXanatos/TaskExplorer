/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2016
 *     jxy-s   2022-2023
 *
 */

#include <kph.h>

#include <trace.h>

KPH_PROTECTED_DATA_SECTION_RO_PUSH();
static const UNICODE_STRING KphpLsaPortName = RTL_CONSTANT_STRING(L"\\SeLsaCommandPort");
static const ANSI_STRING KphpUrlSchemeSeparator = RTL_CONSTANT_STRING("://");
static const ANSI_STRING KphpUrlPathSeparator = RTL_CONSTANT_STRING("/");
static const ANSI_STRING KphpUrlParametersSeparator = RTL_CONSTANT_STRING("?");
static const ANSI_STRING KphpUrlAnchorSeparator = RTL_CONSTANT_STRING("#");
static const ANSI_STRING KphpUrlPortSeparator = RTL_CONSTANT_STRING(":");
KPH_PROTECTED_DATA_SECTION_RO_POP();

/**
 * \brief Compares two blocks of memory.
 *
 * \param[in] Buffer1 Pointer to block of memory.
 * \param[in] Buffer2 Pointer to block of memory.
 * \param[in] Length Number of bytes to compare.
 *
 * \return 0 if the buffers are equal, less than 0 if the byte that does not
 * match in the first buffer is less than the second, greater than 0 if the
 * byte that does not match in the first buffer is greater than the second.
 */
_Must_inspect_result_
INT KphCompareMemory(
    _In_reads_bytes_(Length) PVOID Buffer1,
    _In_reads_bytes_(Length) PVOID Buffer2,
    _In_ SIZE_T Length
    )
{
    //
    // Optimization for length that fits into a register.
    //
#define KPH_COMPARE_MEMORY_SIZED(type)                                        \
    case sizeof(type):                                                        \
    {                                                                         \
        if (*(type*)Buffer1 == *(type*)Buffer2)                               \
        {                                                                     \
            return 0;                                                         \
        }                                                                     \
        break;                                                                \
    }

    switch (Length)
    {
        KPH_COMPARE_MEMORY_SIZED(UCHAR)
        KPH_COMPARE_MEMORY_SIZED(USHORT)
        KPH_COMPARE_MEMORY_SIZED(ULONG)
        KPH_COMPARE_MEMORY_SIZED(ULONG64)
        default:
        {
            break;
        }
    }

#pragma warning(suppress: 4995) // suppress deprecation warning
    return memcmp(Buffer1, Buffer2, Length);
}

/**
 * \brief Compares two blocks of memory for equality.
 *
 * \param[in] Buffer1 Pointer to block of memory.
 * \param[in] Buffer2 Pointer to block of memory.
 * \param[in] Length Number of bytes to compare.
 *
 * \return TRUE if the contents of the buffers are equal, FALSE otherwise.
 */
_Must_inspect_result_
BOOLEAN KphEqualMemory(
    _In_reads_bytes_(Length) PVOID Buffer1,
    _In_reads_bytes_(Length) PVOID Buffer2,
    _In_ SIZE_T Length
    )
{
    //
    // Optimization for length that fits into a register.
    //
#define KPH_EQUAL_MEMORY_SIZED(type)                                          \
    case sizeof(type):                                                        \
    {                                                                         \
        return (*(type*)Buffer1 == *(type*)Buffer2);                          \
    }

    switch (Length)
    {
        KPH_EQUAL_MEMORY_SIZED(UCHAR)
        KPH_EQUAL_MEMORY_SIZED(USHORT)
        KPH_EQUAL_MEMORY_SIZED(ULONG)
        KPH_EQUAL_MEMORY_SIZED(ULONG64)
        default:
        {
            break;
        }
    }

#pragma warning(suppress: 4995) // suppress deprecation warning
    return (memcmp(Buffer1, Buffer2, Length) == 0);
}

/**
 * \brief Searches memory for a given pattern.
 *
 * \param[in] Buffer The memory to search.
 * \param[in] BufferLength The length of the memory to search.
 * \param[in] Pattern The pattern to search for.
 * \param[in] PatternLength The length of the pattern to search for.
 *
 * \return Pointer to the beginning of the first found pattern, NULL if the
 * pattern is not found.
 */
_Must_inspect_result_
PVOID KphSearchMemory(
    _In_reads_bytes_(BufferLength) PVOID Buffer,
    _In_ ULONG BufferLength,
    _In_reads_bytes_(PatternLength) PVOID Pattern,
    _In_ ULONG PatternLength
    )
{
    PBYTE buffer;
    PBYTE end;

    if (!BufferLength || !PatternLength)
    {
        return NULL;
    }

    if (PatternLength > BufferLength)
    {
        return NULL;
    }

    buffer = Buffer;
    end = Add2Ptr(Buffer, BufferLength - PatternLength);

    //
    // Move the loop into the switch to ensure optimal code generation.
    //
#define KPH_SEARCH_MEMORY_FOR for (; buffer <= end; buffer++)

    //
    // Optimization for a pattern size that fits into a register.
    //
#define KPH_SEARCH_MEMORY_SIZED(type)                                         \
    case sizeof(type):                                                        \
    {                                                                         \
        KPH_SEARCH_MEMORY_FOR                                                 \
        {                                                                     \
            if (*(type*)buffer == *(type*)Pattern)                            \
            {                                                                 \
                return buffer;                                                \
            }                                                                 \
        }                                                                     \
        break;                                                                \
    }

    switch (PatternLength)
    {
        KPH_SEARCH_MEMORY_SIZED(UCHAR)
        KPH_SEARCH_MEMORY_SIZED(USHORT)
        KPH_SEARCH_MEMORY_SIZED(ULONG)
        KPH_SEARCH_MEMORY_SIZED(ULONG64)
        default:
        {
            KPH_SEARCH_MEMORY_FOR
            {
#pragma warning(suppress: 4995) // suppress deprecation warning
                if (memcmp(buffer, Pattern, PatternLength) == 0)
                {
                    return buffer;
                }
            }
            break;
        }
    }

    return NULL;
}

/**
 * \brief Acquires rundown. On successful return the caller should release
 * the rundown using KphReleaseRundown.
 *
 * \param[in,out] Rundown The rundown object to acquire.
 *
 * \return TRUE if rundown is acquired, FALSE if object is already ran down.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
_Must_inspect_result_
BOOLEAN KphAcquireRundown(
    _Inout_ PKPH_RUNDOWN Rundown
    )
{
    NPAGED_CODE_DISPATCH_MAX();

    return ExAcquireRundownProtection(Rundown);
}

/**
 * \brief Releases rundown previously acquired by KphAcquireRundown.
 *
 * \param[in,out] Rundown The rundown object to release.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
VOID KphReleaseRundown(
    _Inout_ PKPH_RUNDOWN Rundown
    )
{
    NPAGED_CODE_DISPATCH_MAX();

    ExReleaseRundownProtection(Rundown);
}

/**
 * \brief Retrieves the process sequence number for a given process.
 *
 * \param[in] Process The process to get the sequence number of.
 *
 * \return The sequence number key.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG64 KphGetProcessSequenceNumber(
    _In_ PEPROCESS Process
    )
{
    ULONG64 sequence;
    PKPH_PROCESS_CONTEXT process;

    NPAGED_CODE_DISPATCH_MAX();

    if (KphDynPsGetProcessSequenceNumber)
    {
        return KphDynPsGetProcessSequenceNumber(Process);
    }

    process = KphGetEProcessContext(Process);
    if (!process)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "Failed to get process sequence number for PID %lu",
                      HandleToULong(PsGetProcessId(Process)));

        return 0;
    }

    sequence = process->SequenceNumber;

    KphDereferenceObject(process);

    return sequence;
}

/**
 * \brief Retrieves the process start key for a given process.
 *
 * \param[in] Process The process to get the start key of.
 *
 * \return The process start key.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
ULONG64 KphGetProcessStartKey(
    _In_ PEPROCESS Process
    )
{
    ULONG64 key;
    PKPH_PROCESS_CONTEXT process;

    NPAGED_CODE_DISPATCH_MAX();

    if (KphDynPsGetProcessStartKey)
    {
        return KphDynPsGetProcessStartKey(Process);
    }

    process = KphGetEProcessContext(Process);
    if (!process)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "Failed to get process start key for PID %lu",
                      HandleToULong(PsGetProcessId(Process)));

        return 0;
    }

    key = (process->SequenceNumber | ((ULONG64)SharedUserData->BootId << 48));

    KphDereferenceObject(process);

    return key;
}

/**
 * \brief Retrieves the current thread's sub-process tag.
 *
 * \return The current thread's sub-process tag.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
PVOID KphGetCurrentThreadSubProcessTag(
    VOID
    )
{
    PKPH_THREAD_CONTEXT thread;
    PVOID subProcessTag;
    PTEB teb;

    NPAGED_CODE_DISPATCH_MAX();

    if (PsIsSystemThread(PsGetCurrentThread()))
    {
        return NULL;
    }

    //
    // We support lookups at dispatch. To achieve this we cache the last lookup
    // in the thread context. If we're at dispatch use the cache. Otherwise go
    // do the lookup and cache the result in the thread context.
    //

    if (KeGetCurrentIrql() > APC_LEVEL)
    {
        subProcessTag = NULL;

        thread = KphGetCurrentThreadContext();
        if (thread)
        {
            subProcessTag = thread->SubProcessTag;

            KphDereferenceObject(thread);
        }

        return subProcessTag;
    }

    teb = PsGetCurrentThreadTeb();
    if (!teb)
    {
        return NULL;
    }

    __try
    {
        subProcessTag = teb->SubProcessTag;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }

    thread = KphGetCurrentThreadContext();
    if (thread)
    {
        thread->SubProcessTag = subProcessTag;

        KphDereferenceObject(thread);
    }

    return subProcessTag;
}

/**
 * \brief Retrieves a thread's sub-process tag.
 *
 * \param[in] Thread The thread to get the sub-process tag of.
 * \param[in] CacheOnly If TRUE, only the cached value is returned. This is
 * useful when the caller knows that touching the TEB is not safe.
 *
 * \return The thread's sub-process tag.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
PVOID KphGetThreadSubProcessTagEx(
    _In_ PETHREAD Thread,
    _In_ BOOLEAN CacheOnly
    )
{
    PKPH_THREAD_CONTEXT thread;
    PVOID subProcessTag;
    PTEB teb;

    NPAGED_CODE_DISPATCH_MAX();

    if (PsIsSystemThread(Thread))
    {
        return NULL;
    }

    //
    // We support lookups at dispatch and across process boundaries. To achieve
    // this we cache the last lookup in the thread context. If we're at dispatch
    // or across process boundaries use the cache. Otherwise go do the lookup
    // and cache the result in the thread context. We choose not to attach to
    // a process to retrieve the information to avoid performance penalties.
    //

    if (CacheOnly ||
        (KeGetCurrentIrql() > APC_LEVEL) ||
        (PsGetThreadProcess(Thread) != PsGetCurrentProcess()))
    {
        subProcessTag = NULL;

        thread = KphGetEThreadContext(Thread);
        if (thread)
        {
            subProcessTag = thread->SubProcessTag;

            KphDereferenceObject(thread);
        }

        return subProcessTag;
    }

    teb = PsGetThreadTeb(Thread);
    if (!teb)
    {
        return NULL;
    }

    __try
    {
        subProcessTag = teb->SubProcessTag;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        return NULL;
    }

    thread = KphGetEThreadContext(Thread);
    if (thread)
    {
        thread->SubProcessTag = subProcessTag;

        KphDereferenceObject(thread);
    }

    return subProcessTag;
}

/**
 * \brief Retrieves a thread's sub-process tag.
 *
 * \param[in] Thread The thread to get the sub-process tag of.
 *
 * \return The thread's sub-process tag.
 */
_IRQL_requires_max_(DISPATCH_LEVEL)
PVOID KphGetThreadSubProcessTag(
    _In_ PETHREAD Thread
    )
{
    NPAGED_CODE_DISPATCH_MAX();

    return KphGetThreadSubProcessTagEx(Thread, FALSE);
}

/**
 * \brief Initializes rundown object.
 *
 * \param[out] Rundown The rundown object to initialize.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphInitializeRundown(
    _Out_ PKPH_RUNDOWN Rundown
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    ExInitializeRundownProtection(Rundown);
}

/**
 * \brief Marks rundown active and waits for rundown release. Subsequent
 * attempts to acquire rundown will fail.
 *
 * \param[in,out] Rundown The rundown object to activate and wait for.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphWaitForRundown(
    _Inout_ PKPH_RUNDOWN Rundown
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    ExWaitForRundownProtectionRelease(Rundown);
}

/**
 * \brief Initializes a readers-writer lock.
 *
 * \param[in] Lock The readers-writer lock to initialize.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphInitializeRWLock(
    _Out_ PKPH_RWLOCK Lock
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    FltInitializePushLock(Lock);
}

/**
 * \brief Deletes a readers-writer lock.
 *
 * \param[in] Lock The readers-writer lock to delete.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphDeleteRWLock(
    _In_ PKPH_RWLOCK Lock
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    FltDeletePushLock(Lock);
}

/**
 * \brief Acquires a readers-writer lock exclusive.
 *
 * \param[in,out] Lock The readers-writer lock to acquire exclusively.
 */
_IRQL_requires_max_(APC_LEVEL)
_Acquires_lock_(_Global_critical_region_)
VOID KphAcquireRWLockExclusive(
    _Inout_ _Requires_lock_not_held_(*_Curr_) _Acquires_lock_(*_Curr_) PKPH_RWLOCK Lock
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    FltAcquirePushLockExclusive(Lock);
}

/**
 * \brief Acquires readers-writer lock shared.
 *
 * \param[in,out] Lock The readers-writer lock to acquire shared.
 */
_IRQL_requires_max_(APC_LEVEL)
_Acquires_lock_(_Global_critical_region_)
VOID KphAcquireRWLockShared(
    _Inout_ _Requires_lock_not_held_(*_Curr_) _Acquires_lock_(*_Curr_) PKPH_RWLOCK Lock
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    FltAcquirePushLockShared(Lock);
}

/**
 * \brief Releases a readers-writer lock.
 *
 * \param[in,out] Lock The readers-writer lock to release.
 */
_IRQL_requires_max_(APC_LEVEL)
_Releases_lock_(_Global_critical_region_)
VOID KphReleaseRWLock(
    _Inout_ _Requires_lock_held_(*_Curr_) _Releases_lock_(*_Curr_) PKPH_RWLOCK Lock
    )
{
    NPAGED_CODE_APC_MAX_FOR_PAGING_IO();

    FltReleasePushLock(Lock);
}

PAGED_FILE();

/**
 * \brief Acquires a reference to a reference object.
 *
 * \details KPH_REFERENCE provides underflow and overflow guarantees. For this
 * reason, KPH_REFERENCE may not be suitable for all applications since it is
 * more expensive than a simple reference count.
 *
 * \param[in,out] Reference The reference object to acquire.
 * \param[out] PreviousCount Optionally set to the previous reference count.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphAcquireReference(
    _Inout_ PKPH_REFERENCE Reference,
    _Out_opt_ PLONG PreviousCount
    )
{
    PAGED_CODE();

    for (;;)
    {
        LONG count;

        count = Reference->Count;
        MemoryBarrier();

        if (count == LONG_MAX)
        {
            if (PreviousCount)
            {
                *PreviousCount = LONG_MAX;
            }

            return STATUS_INTEGER_OVERFLOW;
        }

        if (InterlockedCompareExchange(&Reference->Count,
                                       count + 1,
                                       count) != count)
        {
            continue;
        }

        if (PreviousCount)
        {
            *PreviousCount = count;
        }

        return STATUS_SUCCESS;
    }
}

/**
 * \brief Releases a reference to a reference object.
 *
 * \details KPH_REFERENCE provides underflow and overflow guarantees. For this
 * reason, KPH_REFERENCE may not be suitable for all applications since it is
 * more expensive than a simple reference count.
 *
 * \param[in,out] Reference The reference object to release.
 * \param[out] PreviousCount Optionally set to the previous reference count.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphReleaseReference(
    _Inout_ PKPH_REFERENCE Reference,
    _Out_opt_ PLONG PreviousCount
    )
{
    PAGED_CODE();

    for (;;)
    {
        LONG count;

        count = Reference->Count;
        MemoryBarrier();

        if (count == 0)
        {
            if (PreviousCount)
            {
                *PreviousCount = 0;
            }

            return STATUS_INTEGER_OVERFLOW;
        }

        if (InterlockedCompareExchange(&Reference->Count,
                                       count - 1,
                                       count) != count)
        {
            continue;
        }

        if (PreviousCount)
        {
            *PreviousCount = count;
        }

        return STATUS_SUCCESS;
    }
}

/**
 * \brief Checks if an address range lies within a kernel module.
 *
 * \param[in] Address The beginning of the address range.
 * \param[in] Length The number of bytes in the address range.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphValidateAddressForSystemModules(
    _In_ PVOID Address,
    _In_ SIZE_T Length
    )
{
    BOOLEAN valid;
    PVOID endAddress;

    PAGED_CODE();

    if (Add2Ptr(Address, Length) < Address)
    {
        return STATUS_BUFFER_OVERFLOW;
    }

    endAddress = Add2Ptr(Address, Length);

    KeEnterCriticalRegion();
    if (!ExAcquireResourceSharedLite(PsLoadedModuleResource, TRUE))
    {
        KeLeaveCriticalRegion();

        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "Failed to acquire PsLoadedModuleResource");

        return STATUS_RESOURCE_NOT_OWNED;
    }

    valid = FALSE;

    for (PLIST_ENTRY link = PsLoadedModuleList->Flink;
         link != PsLoadedModuleList;
         link = link->Flink)
    {
        PKLDR_DATA_TABLE_ENTRY entry;
        PVOID endOfImage;

        entry = CONTAINING_RECORD(link, KLDR_DATA_TABLE_ENTRY, InLoadOrderLinks);

        endOfImage = Add2Ptr(entry->DllBase, entry->SizeOfImage);

        if ((Address >= entry->DllBase) && (endAddress <= endOfImage))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Validated address in %wZ",
                          &entry->FullDllName);
            valid = TRUE;
            break;
        }
    }

    ExReleaseResourceLite(PsLoadedModuleResource);
    KeLeaveCriticalRegion();

    return (valid ? STATUS_SUCCESS : STATUS_ACCESS_VIOLATION);
}

/**
 * \brief Queries the registry key for a string value.
 *
 * \param[in] KeyHandle Handle to key to query.
 * \param[in] ValueName Name of value to query.
 * \param[out] String Populated with the queried registry string. The string is
 * guaranteed to be null terminated and the MaximumLength will reflect this
 * when compared to the Length. This string should be freed using
 * KphFreeRegistryString.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphQueryRegistryString(
    _In_ HANDLE KeyHandle,
    _In_ PCUNICODE_STRING ValueName,
    _Outptr_allocatesMem_ PUNICODE_STRING* String
    )
{
    NTSTATUS status;
    ULONG resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION info;
    PUNICODE_STRING string;
    ULONG length;

    PAGED_CODE_PASSIVE();

    *String = NULL;
    info = NULL;

    status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)ValueName,
                             KeyValuePartialInformation,
                             NULL,
                             0,
                             &resultLength);
    if ((status != STATUS_BUFFER_OVERFLOW) &&
        (status != STATUS_BUFFER_TOO_SMALL))
    {
        if (NT_SUCCESS(status))
        {
            status = STATUS_UNSUCCESSFUL;
        }

        goto Exit;
    }

    info = KphAllocatePaged(resultLength, KPH_TAG_REG_STRING);
    if (!info)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)ValueName,
                             KeyValuePartialInformation,
                             info,
                             resultLength,
                             &resultLength);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    status = RtlULongAdd(info->DataLength, sizeof(WCHAR), &length);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    if ((info->Type != REG_SZ) ||
        (length > UNICODE_STRING_MAX_BYTES) ||
        ((info->DataLength % 2) != 0))
    {
        status = STATUS_OBJECT_TYPE_MISMATCH;
        goto Exit;
    }

    string = KphAllocatePaged((sizeof(UNICODE_STRING) + length),
                              KPH_TAG_REG_STRING);
    if (!string)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    string->Buffer = Add2Ptr(string, sizeof(UNICODE_STRING));
    RtlZeroMemory(string->Buffer, length);

    NT_ASSERT(info->DataLength < UNICODE_STRING_MAX_BYTES);
    NT_ASSERT(length < UNICODE_STRING_MAX_BYTES);

    string->Length = (USHORT)info->DataLength;
    string->MaximumLength = (USHORT)length;

    if (string->Length)
    {
        PWCHAR sz;

        sz = (PWCHAR)info->Data;

        NT_ASSERT(info->DataLength >= sizeof(WCHAR));

        if (sz[(info->DataLength / sizeof(WCHAR)) - 1] == L'\0')
        {
            string->Length -= sizeof(WCHAR);
        }

        RtlCopyMemory(string->Buffer, info->Data, string->Length);
    }

    *String = string;
    status = STATUS_SUCCESS;

Exit:

    if (info)
    {
        KphFree(info, KPH_TAG_REG_STRING);
    }

    return status;
}

/**
 * \brief Frees a string retrieved by KphQueryRegistryString.
 *
 * \param[in] String The string to free.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphFreeRegistryString(
    _In_freesMem_ PUNICODE_STRING String
    )
{
    PAGED_CODE();

    KphFree(String, KPH_TAG_REG_STRING);
}

/**
 * \brief Queries the registry key for a string value.
 *
 * \param[in] KeyHandle Handle to key to query.
 * \param[in] ValueName Name of value to query.
 * \param[out] Buffer Registry binary buffer on success, should be freed using
 * KphFreeRegistryBinary.
 * \param[out] Length Set to the length of the buffer.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphQueryRegistryBinary(
    _In_ HANDLE KeyHandle,
    _In_ PCUNICODE_STRING ValueName,
    _Outptr_allocatesMem_ PBYTE* Buffer,
    _Out_ PULONG Length
    )
{
    NTSTATUS status;
    PBYTE buffer;
    ULONG resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION info;

    PAGED_CODE_PASSIVE();

    *Buffer = NULL;
    *Length = 0;
    buffer = NULL;

    status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)ValueName,
                             KeyValuePartialInformation,
                             NULL,
                             0,
                             &resultLength);
    if ((status != STATUS_BUFFER_OVERFLOW) &&
        (status != STATUS_BUFFER_TOO_SMALL))
    {
        if (NT_SUCCESS(status))
        {
            status = STATUS_UNSUCCESSFUL;
        }

        goto Exit;
    }

    buffer = KphAllocatePaged(resultLength, KPH_TAG_REG_BINARY);
    if (!buffer)
    {
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)ValueName,
                             KeyValuePartialInformation,
                             buffer,
                             resultLength,
                             &resultLength);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    info = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    if (info->Type != REG_BINARY)
    {
        status = STATUS_OBJECT_TYPE_MISMATCH;
        goto Exit;
    }

    *Length = info->DataLength;
    *Buffer = info->Data;
    buffer = NULL;
    status = STATUS_SUCCESS;

Exit:

    if (buffer)
    {
        KphFree(buffer, KPH_TAG_REG_BINARY);
    }

    return status;
}

/**
 * \brief Frees a buffer retrieved by KphQueryRegistryBinary.
 *
 * \param[in] String String to free.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphFreeRegistryBinary(
    _In_freesMem_ PBYTE Buffer
    )
{
    PAGED_CODE();

    KphFree(CONTAINING_RECORD(Buffer, KEY_VALUE_PARTIAL_INFORMATION, Data),
            KPH_TAG_REG_BINARY);
}

/**
 * \brief Queries the registry key for a string value.
 *
 * \param[in] KeyHandle Handle to key to query.
 * \param[in] ValueName Name of value to query.
 * \param[out] Value Registry unsigned long.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphQueryRegistryULong(
    _In_ HANDLE KeyHandle,
    _In_ PCUNICODE_STRING ValueName,
    _Out_ PULONG Value
    )
{
    NTSTATUS status;
    BYTE buffer[sizeof(KEY_VALUE_PARTIAL_INFORMATION) + sizeof(ULONGLONG)];
    ULONG resultLength;
    PKEY_VALUE_PARTIAL_INFORMATION info;

    PAGED_CODE_PASSIVE();

    *Value = 0;

    status = ZwQueryValueKey(KeyHandle,
                             (PUNICODE_STRING)ValueName,
                             KeyValuePartialInformation,
                             buffer,
                             ARRAYSIZE(buffer),
                             &resultLength);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    info = (PKEY_VALUE_PARTIAL_INFORMATION)buffer;

    if (info->Type != REG_DWORD)
    {
        return STATUS_OBJECT_TYPE_MISMATCH;
    }

    if (info->DataLength != sizeof(ULONG))
    {
        return STATUS_INFO_LENGTH_MISMATCH;
    }

    *Value = *(PULONG)info->Data;

    return STATUS_SUCCESS;
}

/**
 * \brief Maps view into the system address space. The caller should unmap the
 * view by calling KphUnmapViewInSystem.
 *
 * \param[in] FileHandle Handle to file to map.
 * \param[in] Flags Options for the mapping (see: KPH_MAP..).
 * \param[out] MappedBase Base address of the mapping.
 * \param[out] ViewSize Size of the mapped view. If not an image mapping and
 * if the view exceeds the file size, it is clamped to the file size.
 *
 * \return Successful or errant status.
 */
_IRQL_always_function_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphMapViewInSystem(
    _In_ HANDLE FileHandle,
    _In_ ULONG Flags,
    _Outptr_result_bytebuffer_(*ViewSize) PVOID *MappedBase,
    _Inout_ PSIZE_T ViewSize
    )
{
    NTSTATUS status;
    HANDLE sectionHandle;
    PVOID sectionObject;
    OBJECT_ATTRIBUTES objectAttributes;
    ULONG allocationAttributes;
    SIZE_T fileSize;
    LARGE_INTEGER sectionOffset;

    PAGED_CODE_PASSIVE();

    sectionHandle = NULL;
    sectionObject = NULL;
    *MappedBase = NULL;

    if (BooleanFlagOn(Flags, KPH_MAP_IMAGE))
    {
        allocationAttributes = SEC_IMAGE_NO_EXECUTE;
        fileSize = SIZE_T_MAX;
    }
    else
    {
        IO_STATUS_BLOCK ioStatusBlock;
        FILE_STANDARD_INFORMATION fileInfo;

        allocationAttributes = SEC_COMMIT;

        status = ZwQueryInformationFile(FileHandle,
                                        &ioStatusBlock,
                                        &fileInfo,
                                        sizeof(fileInfo),
                                        FileStandardInformation);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        if (fileInfo.EndOfFile.QuadPart < 0)
        {
            status = STATUS_FILE_TOO_LARGE;
            goto Exit;
        }

#ifdef _WIN64
        fileSize = (SIZE_T)fileInfo.EndOfFile.QuadPart;
#else
        if (fileInfo.EndOfFile.HighPart != 0)
        {
            status = STATUS_FILE_TOO_LARGE;
            goto Exit;
        }
        fileSize = fileInfo.EndOfFile.LowPart;
#endif
    }

    InitializeObjectAttributes(&objectAttributes,
                               NULL,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = ZwCreateSection(&sectionHandle,
                             SECTION_MAP_READ,
                             &objectAttributes,
                             NULL,
                             PAGE_READONLY,
                             allocationAttributes,
                             FileHandle);
    if (!NT_SUCCESS(status))
    {
        sectionHandle = NULL;
        goto Exit;
    }

    status = ObReferenceObjectByHandle(sectionHandle,
                                       SECTION_MAP_READ,
                                       *MmSectionObjectType,
                                       KernelMode,
                                       &sectionObject,
                                       NULL);
    if (!NT_SUCCESS(status))
    {
        sectionObject = NULL;
        goto Exit;
    }

    sectionOffset.QuadPart = 0;
    status = MmMapViewInSystemSpaceEx(sectionObject,
                                      MappedBase,
                                      ViewSize,
                                      &sectionOffset,
                                      MM_SYSTEM_VIEW_EXCEPTIONS_FOR_INPAGE_ERRORS);
    if (!NT_SUCCESS(status))
    {
        *MappedBase = NULL;
        *ViewSize = 0;
        goto Exit;
    }

    if (*ViewSize > fileSize)
    {
        *ViewSize = fileSize;
    }

    status = STATUS_SUCCESS;

Exit:

    if (sectionObject)
    {
        ObDereferenceObject(sectionObject);
    }

    if (sectionHandle)
    {
        ObCloseHandle(sectionHandle, KernelMode);
    }

    return status;
}

/**
 * \brief Unmaps view from the system process address space.
 *
 * \param[in] MappedBase Base address of the mapping.
 */
_IRQL_always_function_max_(PASSIVE_LEVEL)
VOID KphUnmapViewInSystem(
    _In_ PVOID MappedBase
    )
{
    PAGED_CODE_PASSIVE();

    MmUnmapViewInSystemSpace(MappedBase);
}

/**
 * \brief Gets the file name from a file object.
 *
 * \param[in] FileObject The file object to get the name from.
 * \param[out] FileName Set to a point to the file name on success, caller
 * should free this using KphFreeFileNameObject.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetNameFileObject(
    _In_ PFILE_OBJECT FileObject,
    _Outptr_allocatesMem_ PUNICODE_STRING* FileName
    )
{
    NTSTATUS status;
    ULONG returnLength;
    POBJECT_NAME_INFORMATION nameInfo;

    PAGED_CODE_PASSIVE();

    *FileName = NULL;

    returnLength = (sizeof(OBJECT_NAME_INFORMATION) + MAX_PATH);
    nameInfo = KphAllocatePaged(returnLength, KPH_TAG_FILE_OBJECT_NAME);
    if (!nameInfo)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "Failed to allocate for file object name.");

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = KphQueryNameFileObject(FileObject,
                                    nameInfo,
                                    returnLength,
                                    &returnLength);
    if (status == STATUS_BUFFER_TOO_SMALL)
    {
        KphFree(nameInfo, KPH_TAG_FILE_OBJECT_NAME);
        nameInfo = KphAllocatePaged(returnLength, KPH_TAG_FILE_OBJECT_NAME);
        if (!nameInfo)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Failed to allocate for file object name.");

            status = STATUS_INSUFFICIENT_RESOURCES;
            goto Exit;
        }

        status = KphQueryNameFileObject(FileObject,
                                        nameInfo,
                                        returnLength,
                                        &returnLength);
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "KphQueryNameFileObject failed: %!STATUS!",
                      status);

        goto Exit;
    }

    *FileName = &nameInfo->Name;
    nameInfo = NULL;
    status = STATUS_SUCCESS;

Exit:

    if (nameInfo)
    {
        KphFree(nameInfo, KPH_TAG_FILE_OBJECT_NAME);
    }

    return status;
}

/**
 * \brief Frees a file name previously retrieved by KphGetFileNameObject.
 *
 * \param[out] FileName The file name to free.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphFreeNameFileObject(
    _In_freesMem_ PUNICODE_STRING FileName
    )
{
    PAGED_CODE();

    KphFree(CONTAINING_RECORD(FileName, OBJECT_NAME_INFORMATION, Name),
            KPH_TAG_FILE_OBJECT_NAME);
}

/**
 * \brief Perform a single privilege check on the supplied subject context.
 *
 * \param[in] PrivilegeValue The privilege value to check.
 * \param[in] SubjectSecurityContext The subject context to check.
 * \param[in] AccessMode The access mode used for the access check.
 *
 * \return TRUE if the subject has the desired privilege, FALSE otherwise.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN KphSinglePrivilegeCheckEx(
    _In_ LUID PrivilegeValue,
    _In_ PSECURITY_SUBJECT_CONTEXT SubjectSecurityContext,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PRIVILEGE_SET requiredPrivileges;

    PAGED_CODE_PASSIVE();

    requiredPrivileges.PrivilegeCount = 1;
    requiredPrivileges.Control = PRIVILEGE_SET_ALL_NECESSARY;
    requiredPrivileges.Privilege[0].Luid = PrivilegeValue;
    requiredPrivileges.Privilege[0].Attributes = 0;

    return SePrivilegeCheck(&requiredPrivileges,
                            SubjectSecurityContext,
                            AccessMode);
}

/**
 * \brief Perform a single privilege check on the current subject context.
 *
 * \param[in] PrivilegeValue The privilege value to check.
 * \param[in] AccessMode The access mode used for the access check.
 *
 * \return TRUE if the subject has the desired privilege, FALSE otherwise.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN KphSinglePrivilegeCheck(
    _In_ LUID PrivilegeValue,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    BOOLEAN accessGranted;
    SECURITY_SUBJECT_CONTEXT subjectContext;

    PAGED_CODE_PASSIVE();

    SeCaptureSubjectContext(&subjectContext);

    accessGranted = KphSinglePrivilegeCheckEx(PrivilegeValue,
                                              &subjectContext,
                                              AccessMode);

    SeReleaseSubjectContext(&subjectContext);

    return accessGranted;
}

/**
 * \brief Retrieves the process ID of lsass.
 *
 * \param[out] ProcessId Set to the process ID of lsass.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS KphpGetLsassProcessId(
    _Out_ PHANDLE ProcessId
    )
{
    NTSTATUS status;
    PKPH_DYN dyn;
    HANDLE portHandle;
    KAPC_STATE apcState;
    KPH_ALPC_COMMUNICATION_INFORMATION info;

    PAGED_CODE_PASSIVE();

    *ProcessId = NULL;

    //
    // N.B. This is an optimization. In order to query the process ID of lsass
    // through the LSA port, we need the dynamic data. Rather than doing the
    // work to attach to the system process and open the port, if we know we
    // do not have the dynamic data, exit early.
    //
    dyn = KphReferenceDynData();
    if (!dyn)
    {
        return STATUS_NOINTERFACE;
    }

    //
    // Attach to system to ensure we get a kernel handle from the following
    // ZwAlpcConnectPort call. Opening the handle here does not ask for the
    // object attributes. To keep our imports simple we choose to use this
    // over the Ex version (might change in the future). Pattern is adopted
    // from msrpc.sys.
    //

    KeStackAttachProcess(PsInitialSystemProcess, &apcState);

    status = ZwAlpcConnectPort(&portHandle,
                               (PUNICODE_STRING)&KphpLsaPortName,
                               NULL,
                               NULL,
                               0,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL,
                               NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "ZwAlpcConnectPort failed: %!STATUS!",
                      status);

        portHandle = NULL;
        goto Exit;
    }

    status = KphAlpcQueryInformation(ZwCurrentProcess(),
                                     portHandle,
                                     KphAlpcCommunicationInformation,
                                     &info,
                                     sizeof(info),
                                     NULL,
                                     KernelMode);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "KphAlpcQueryInformation failed: %!STATUS!",
                      status);
        goto Exit;
    }

    *ProcessId = info.ConnectionPort.OwnerProcessId;

Exit:

    if (portHandle)
    {
        ObCloseHandle(portHandle, KernelMode);
    }

    KeUnstackDetachProcess(&apcState);

    KphDereferenceObject(dyn);

    return status;
}

/**
 * \brief Checks if a given process is lsass.
 *
 * \param[in] Process The process to check.
 * \param[out] IsLsass TRUE if the process is lsass, FALSE otherwise.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphProcessIsLsass(
    _In_ PEPROCESS Process,
    _Out_ PBOOLEAN IsLsass
    )
{
    NTSTATUS status;
    HANDLE processId;
    SECURITY_SUBJECT_CONTEXT subjectContext;

    PAGED_CODE_PASSIVE();

    *IsLsass = FALSE;

    status = KphpGetLsassProcessId(&processId);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    if (processId != PsGetProcessId(Process))
    {
        return STATUS_SUCCESS;
    }

    SeCaptureSubjectContextEx(NULL, Process, &subjectContext);

    *IsLsass = KphSinglePrivilegeCheckEx(SeCreateTokenPrivilege,
                                         &subjectContext,
                                         UserMode);

    SeReleaseSubjectContext(&subjectContext);

    return STATUS_SUCCESS;
}

/**
 * \brief Retrieves the kernel file name.
 *
 * \param[out] FileName On success set to the file name for the kernel. Must be
 * freed using RtlFreeUnicodeString.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphpGetKernelFileName(
    _Out_ _At_(FileName->Buffer, __drv_allocatesMem(Mem))
    PUNICODE_STRING FileName
    )
{
    NTSTATUS status;
    SYSTEM_SINGLE_MODULE_INFORMATION info;
    ANSI_STRING fullPathName;

    PAGED_CODE_PASSIVE();

    RtlZeroMemory(FileName, sizeof(UNICODE_STRING));

    RtlZeroMemory(&info, sizeof(info));

    info.TargetModuleAddress = (PVOID)ObCloseHandle;
    status = ZwQuerySystemInformation(SystemSingleModuleInformation,
                                      &info,
                                      sizeof(info),
                                      NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "ZwQuerySystemInformation failed: %!STATUS!",
                      status);

        return status;
    }

    status = RtlInitAnsiStringEx(&fullPathName,
                                 (PCSZ)info.ExInfo.BaseInfo.FullPathName);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    return RtlAnsiStringToUnicodeString(FileName, &fullPathName, TRUE);
}

/**
 * \brief Retrieves the version of the kernel.
 *
 * \param[out] Version Set to the kernel build version.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetKernelVersion(
    _Out_ PKPH_FILE_VERSION Version
    )
{
    NTSTATUS status;
    UNICODE_STRING kernelFileName;

    PAGED_CODE_PASSIVE();

    status = KphpGetKernelFileName(&kernelFileName);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "KphGetKernelFileName failed: %!STATUS!",
                      status);

        RtlZeroMemory(Version, sizeof(KPH_FILE_VERSION));
        goto Exit;
    }

    KphTracePrint(TRACE_LEVEL_INFORMATION,
                  UTIL,
                  "Kernel file name: \"%wZ\"",
                  &kernelFileName);

    status = KphGetFileVersion(&kernelFileName, Version);

Exit:

    RtlFreeUnicodeString(&kernelFileName);

    return status;
}

/**
 * \brief Retrieves the file version from a file.
 *
 * \param[in] FileName The name of the file to get the version from.
 * \param[out] Version Set to the file version.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetFileVersion(
    _In_ PCUNICODE_STRING FileName,
    _Out_ PKPH_FILE_VERSION Version
    )
{
    NTSTATUS status;
    OBJECT_ATTRIBUTES objectAttributes;
    HANDLE fileHandle;
    IO_STATUS_BLOCK ioStatusBlock;
    PVOID imageBase;
    SIZE_T imageSize;
    PVOID imageEnd;
    LDR_RESOURCE_INFO resourceInfo;
    PIMAGE_RESOURCE_DATA_ENTRY resourceData;
    PVOID resourceBuffer;
    ULONG resourceLength;
    PVS_VERSION_INFO_STRUCT versionInfo;
    UNICODE_STRING keyName;
    PVS_FIXEDFILEINFO fileInfo;

    PAGED_CODE_PASSIVE();

    RtlZeroMemory(Version, sizeof(KPH_FILE_VERSION));

    imageBase = NULL;
    fileHandle = NULL;

    InitializeObjectAttributes(&objectAttributes,
                               (PUNICODE_STRING)FileName,
                               OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = KphCreateFile(&fileHandle,
                           FILE_READ_ACCESS | SYNCHRONIZE,
                           &objectAttributes,
                           &ioStatusBlock,
                           NULL,
                           FILE_ATTRIBUTE_NORMAL,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           FILE_OPEN,
                           FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT,
                           NULL,
                           0,
                           IO_IGNORE_SHARE_ACCESS_CHECK,
                           KernelMode);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "KphCreateFile failed: %!STATUS!",
                      status);

        fileHandle = NULL;
        goto Exit;
    }

    imageSize = 0;
    status = KphMapViewInSystem(fileHandle,
                                KPH_MAP_IMAGE,
                                &imageBase,
                                &imageSize);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      UTIL,
                      "KphMapViewInSystem failed: %!STATUS!",
                      status);

        imageBase = NULL;
        goto Exit;
    }

    imageEnd = Add2Ptr(imageBase, imageSize);

    resourceInfo.Type = (ULONG_PTR)VS_FILE_INFO;
    resourceInfo.Name = (ULONG_PTR)MAKEINTRESOURCEW(VS_VERSION_INFO);
    resourceInfo.Language = MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL);

    __try
    {
        status = LdrFindResource_U(imageBase,
                                   &resourceInfo,
                                   RESOURCE_DATA_LEVEL,
                                   &resourceData);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "LdrFindResource_U failed: %!STATUS!",
                          status);

            goto Exit;
        }

        status = LdrAccessResource(imageBase,
                                   resourceData,
                                   &resourceBuffer,
                                   &resourceLength);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "LdrAccessResource failed: %!STATUS!",
                          status);

            goto Exit;
        }

        if (Add2Ptr(resourceBuffer, resourceLength) >= imageEnd)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Resource buffer overflows mapping");

            status = STATUS_BUFFER_OVERFLOW;
            goto Exit;
        }

        if (resourceLength < sizeof(VS_VERSION_INFO_STRUCT))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Resource length insufficient");

            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        versionInfo = resourceBuffer;

        if (Add2Ptr(resourceBuffer, versionInfo->Length) >= imageEnd)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Version info overflows mapping");

            status = STATUS_BUFFER_OVERFLOW;
            goto Exit;
        }

        if (versionInfo->ValueLength < sizeof(VS_FIXEDFILEINFO))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Value length insufficient");

            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        status = RtlInitUnicodeStringEx(&keyName, versionInfo->Key);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "RtlInitUnicodeStringEx failed: %!STATUS!",
                          status);

            goto Exit;
        }

        fileInfo = Add2Ptr(versionInfo, RTL_SIZEOF_THROUGH_FIELD(VS_VERSION_INFO_STRUCT, Type));
        fileInfo = (PVS_FIXEDFILEINFO)ALIGN_UP(Add2Ptr(fileInfo, keyName.MaximumLength), ULONG);

        if (Add2Ptr(fileInfo, sizeof(VS_FIXEDFILEINFO)) >= imageEnd)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "File version info overflows mapping");

            status = STATUS_BUFFER_TOO_SMALL;
            goto Exit;
        }

        if (fileInfo->dwSignature != VS_FFI_SIGNATURE)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Invalid file version information signature (0x%08x)",
                          fileInfo->dwSignature);

            status = STATUS_INVALID_SIGNATURE;
            goto Exit;
        }

        if (fileInfo->dwStrucVersion != 0x10000)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          UTIL,
                          "Unknown file version information structure (0x%08x)",
                          fileInfo->dwStrucVersion);

            status = STATUS_REVISION_MISMATCH;
            goto Exit;
        }

        Version->MajorVersion = HIWORD(fileInfo->dwFileVersionMS);
        Version->MinorVersion = LOWORD(fileInfo->dwFileVersionMS);
        Version->BuildNumber = HIWORD(fileInfo->dwFileVersionLS);
        Version->Revision = LOWORD(fileInfo->dwFileVersionLS);
        status = STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        status = GetExceptionCode();
    }

Exit:

    if (imageBase)
    {
        KphUnmapViewInSystem(imageBase);
    }

    if (fileHandle)
    {
        ObCloseHandle(fileHandle, KernelMode);
    }

    return status;
}

/**
 * \brief Sets CFG call target information for a process.
 *
 * \param[in] ProcessHandle Handle to the process to configure CFG for.
 * \param[in] VirtualAddress Virtual address to configure CFG for.
 * \param[in] Flags CFG call target flags (CFG_CALL_TARGET_).
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphpSetCfgCallTargetInformation(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID VirtualAddress,
    _In_ ULONG Flags
    )
{
    MEMORY_RANGE_ENTRY memoryRange;
    CFG_CALL_TARGET_INFO targetInfo;
    CFG_CALL_TARGET_LIST_INFORMATION targetListInfo;
    ULONG numberOfEntriesProcessed;

    PAGED_CODE_PASSIVE();

    memoryRange.VirtualAddress = PAGE_ALIGN(VirtualAddress);
    memoryRange.NumberOfBytes = PAGE_SIZE;

    targetInfo.Offset = BYTE_OFFSET(VirtualAddress);
    targetInfo.Flags = Flags;

    numberOfEntriesProcessed = 0;

    RtlZeroMemory(&targetListInfo, sizeof(targetListInfo));
    targetListInfo.NumberOfEntries = 1;
    targetListInfo.NumberOfEntriesProcessed = &numberOfEntriesProcessed;
    targetListInfo.CallTargetInfo = &targetInfo;

    return ZwSetInformationVirtualMemory(ProcessHandle,
                                         VmCfgCallTargetInformation,
                                         1,
                                         &memoryRange,
                                         &targetListInfo,
                                         sizeof(targetListInfo));
}

/**
 * \brief Grants a suppressed call access to a target.
 *
 * \param[in] ProcessHandle Handle to the process to configure CFG for.
 * \param[in] VirtualAddress Virtual address of the target.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGuardGrantSuppressedCallAccess(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID VirtualAddress
    )
{
    ULONG flags;

    PAGED_CODE_PASSIVE();

    flags = CFG_CALL_TARGET_CONVERT_EXPORT_SUPPRESSED_TO_VALID;

    return KphpSetCfgCallTargetInformation(ProcessHandle,
                                           VirtualAddress,
                                           flags);
}

/**
 * \brief Disables XFG on a target.
 *
 * \param[in] ProcessHandle Handle to the process to configure CFG for.
 * \param[in] VirtualAddress Virtual address of the target.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphDisableXfgOnTarget(
    _In_ HANDLE ProcessHandle,
    _In_ PVOID VirtualAddress
    )
{
    ULONG flags;

    PAGED_CODE_PASSIVE();

    flags = CFG_CALL_TARGET_CONVERT_XFG_TO_CFG;

    return KphpSetCfgCallTargetInformation(ProcessHandle,
                                           VirtualAddress,
                                           flags);
}

/**
 * \brief Gets the final component of a file name.
 *
 * \details The final component is the last part of the file name, e.g. for
 * "\SystemRoot\System32\ntoskrnl.exe" it would be "ntoskrnl.exe". Returns an
 * error if the final component is not found.
 *
 * \param[in] FileName File name to get the final component of.
 * \param[out] FinalComponent Final component of the file name, references
 * input string, this string should not be freed and the input string must
 * remain valid as long as this is in use.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetFileNameFinalComponent(
    _In_ PCUNICODE_STRING FileName,
    _Out_ PUNICODE_STRING FinalComponent
    )
{
    PAGED_CODE();

    for (USHORT i = (FileName->Length / sizeof(WCHAR)); i > 0; i--)
    {
        if (FileName->Buffer[i - 1] != L'\\')
        {
            continue;
        }

        FinalComponent->Buffer = &FileName->Buffer[i];
        FinalComponent->Length = FileName->Length - (i * sizeof(WCHAR));
        FinalComponent->MaximumLength = FinalComponent->Length;

        return STATUS_SUCCESS;
    }

    RtlZeroMemory(FinalComponent, sizeof(*FinalComponent));

    return STATUS_NOT_FOUND;
}

/**
 * \brief Gets the image name from a process.
 *
 * \param[in] Process The process to get the image name from.
 * \param[out] ImageName Populated with the image name of the process, this
 * string should be freed using KphFreeProcessImageName.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetProcessImageName(
    _In_ PEPROCESS Process,
    _Out_allocatesMem_ PUNICODE_STRING ImageName
    )
{
    NTSTATUS status;
    PUCHAR fileName;
    SIZE_T len;

    PAGED_CODE_PASSIVE();

    fileName = PsGetProcessImageFileName(Process);

    status = RtlStringCbLengthA((STRSAFE_PCNZCH)fileName, 15, &len);
    if (NT_SUCCESS(status))
    {
        ANSI_STRING string;

        string.Buffer = (PCHAR)fileName;
        string.Length = (USHORT)len;
        string.MaximumLength = string.Length;

        status = RtlAnsiStringToUnicodeString(ImageName, &string, TRUE);
        if (NT_SUCCESS(status))
        {
            return status;
        }
    }

    RtlZeroMemory(ImageName, sizeof(*ImageName));

    return status;
}

/**
 * \brief Frees a process image file name from KphGetProcessImageName.
 *
 * \param[in] ImageName The image name to free.
 */
_IRQL_requires_max_(APC_LEVEL)
VOID KphFreeProcessImageName(
    _In_freesMem_ PUNICODE_STRING ImageName
    )
{
    PAGED_CODE();

    RtlFreeUnicodeString(ImageName);
}

/**
 * \brief Opens the driver parameters key.
 *
 * \param[in] RegistryPath Registry path from the entry point.
 * \param[out] KeyHandle Handle to parameters key on success, null on failure.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphOpenParametersKey(
    _In_ PCUNICODE_STRING RegistryPath,
    _Out_ PHANDLE KeyHandle
    )
{
    NTSTATUS status;
    WCHAR buffer[MAX_PATH];
    UNICODE_STRING parametersKeyName;
    OBJECT_ATTRIBUTES objectAttributes;

    PAGED_CODE_PASSIVE();

    *KeyHandle = NULL;

    parametersKeyName.Buffer = buffer;
    parametersKeyName.Length = 0;
    parametersKeyName.MaximumLength = sizeof(buffer);

    status = RtlAppendUnicodeStringToString(&parametersKeyName, RegistryPath);
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    status = RtlAppendUnicodeToString(&parametersKeyName, L"\\Parameters");
    if (!NT_SUCCESS(status))
    {
        return status;
    }

    InitializeObjectAttributes(&objectAttributes,
                               &parametersKeyName,
                               OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE,
                               NULL,
                               NULL);

    status = ZwOpenKey(KeyHandle, KEY_READ, &objectAttributes);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "Unable to open Parameters key: %!STATUS!",
                      status);

        *KeyHandle = NULL;
        return status;
    }

    return STATUS_SUCCESS;
}

/**
 * \brief Parses a URL into its components.
 *
 * \details The output information references information in the input URL
 * buffer. The parsed output information *must* outlive the input URL buffer.
 *
 * \param[in] Url The URL to parse.
 * \param[out] UrlInfo The parsed URL information.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphParseUrlInformation(
    _In_ PANSI_STRING Url,
    _Out_ PKPH_URL_INFORMATION UrlInfo
    )
{
    NTSTATUS status;
    PVOID buffer;
    ULONG remaining;
    PVOID part;
    ULONG_PTR length;

    PAGED_CODE();

    buffer = Url->Buffer;
    remaining = Url->Length;

    RtlZeroMemory(UrlInfo, sizeof(*UrlInfo));

    //
    // Extract any Scheme
    //

    part = KphSearchMemory(buffer,
                           remaining,
                           KphpUrlSchemeSeparator.Buffer,
                           KphpUrlSchemeSeparator.Length);
    if (part)
    {
        status = RtlULongPtrToUShort(PtrOffset(buffer, part),
                                     &UrlInfo->Scheme.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->Scheme.Buffer = buffer;
        UrlInfo->Scheme.MaximumLength = UrlInfo->Scheme.Length;

        buffer = Add2Ptr(part, KphpUrlSchemeSeparator.Length);
        remaining -= (UrlInfo->Scheme.Length + KphpUrlSchemeSeparator.Length);
    }

    //
    // Extract any Parameters
    //

    part = KphSearchMemory(buffer,
                           remaining,
                           KphpUrlParametersSeparator.Buffer,
                           KphpUrlParametersSeparator.Length);
    if (part)
    {
        length = ((ULONG_PTR)remaining - PtrOffset(buffer, part));

        status = RtlULongPtrToUShort(length, &UrlInfo->Parameters.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->Parameters.Buffer = part;
        UrlInfo->Parameters.MaximumLength = UrlInfo->Parameters.Length;
    }

    //
    // Extract any Anchor
    //

    part = KphSearchMemory(buffer,
                           remaining,
                           KphpUrlAnchorSeparator.Buffer,
                           KphpUrlAnchorSeparator.Length);

    if (part)
    {
        length = ((ULONG_PTR)remaining - PtrOffset(buffer, part));

        status = RtlULongPtrToUShort(length, &UrlInfo->Anchor.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->Anchor.Buffer = part;
        UrlInfo->Anchor.MaximumLength = UrlInfo->Anchor.Length;

        if (UrlInfo->Parameters.Buffer)
        {
            UrlInfo->Parameters.Length -= UrlInfo->Anchor.Length;
            UrlInfo->Parameters.MaximumLength = UrlInfo->Parameters.Length;
        }
    }

    //
    // Extract any Authority
    //

    part = KphSearchMemory(buffer,
                           remaining,
                           KphpUrlPathSeparator.Buffer,
                           KphpUrlPathSeparator.Length);
    if (part)
    {
        status = RtlULongPtrToUShort(PtrOffset(buffer, part),
                                     &UrlInfo->Authority.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->Authority.Buffer = buffer;
        UrlInfo->Authority.MaximumLength = UrlInfo->Authority.Length;

        buffer = part;
        remaining -= UrlInfo->Authority.Length;
    }
    else
    {
        UrlInfo->Authority.Buffer = buffer;

        if (UrlInfo->Parameters.Buffer)
        {
            length = PtrOffset(UrlInfo->Authority.Buffer,
                               UrlInfo->Parameters.Buffer);
        }
        else if (UrlInfo->Anchor.Buffer)
        {
            length = PtrOffset(UrlInfo->Authority.Buffer,
                               UrlInfo->Anchor.Buffer);
        }
        else
        {
            length = remaining;
        }

        status = RtlULongPtrToUShort(length, &UrlInfo->Authority.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->Authority.MaximumLength = UrlInfo->Authority.Length;

        UrlInfo->Path = KphpUrlPathSeparator;
    }

    //
    // Break Authority into parts, if any.
    //

    part = KphSearchMemory(UrlInfo->Authority.Buffer,
                           UrlInfo->Authority.Length,
                           KphpUrlPortSeparator.Buffer,
                           KphpUrlPortSeparator.Length);
    if (part)
    {
        status = RtlULongPtrToUShort(PtrOffset(UrlInfo->Authority.Buffer, part),
                                     &UrlInfo->DomainName.Length);
        if (!NT_SUCCESS(status))
        {
            goto Exit;
        }

        UrlInfo->DomainName.Buffer = UrlInfo->Authority.Buffer;
        UrlInfo->DomainName.MaximumLength = UrlInfo->DomainName.Length;

        UrlInfo->Port.Buffer = Add2Ptr(part, KphpUrlPortSeparator.Length);
        UrlInfo->Port.Length = (UrlInfo->Authority.Length -
                                UrlInfo->DomainName.Length -
                                KphpUrlPortSeparator.Length);
        UrlInfo->Port.MaximumLength = UrlInfo->Port.Length;
    }
    else
    {
        UrlInfo->DomainName = UrlInfo->Authority;
    }

    if (UrlInfo->Path.Buffer == KphpUrlPathSeparator.Buffer)
    {
        //
        // There was no path specified, we're done.
        //
        status = STATUS_SUCCESS;
        goto Exit;
    }

    //
    // Extract the Path
    //

    UrlInfo->Path.Buffer = buffer;

    if (UrlInfo->Parameters.Buffer)
    {
        length = PtrOffset(UrlInfo->Path.Buffer, UrlInfo->Parameters.Buffer);
    }
    else if (UrlInfo->Anchor.Buffer)
    {
        length = PtrOffset(UrlInfo->Path.Buffer, UrlInfo->Anchor.Buffer);
    }
    else
    {
        length = remaining;
    }

    status = RtlULongPtrToUShort(length, &UrlInfo->Path.Length);
    if (!NT_SUCCESS(status))
    {
        goto Exit;
    }

    UrlInfo->Path.MaximumLength = UrlInfo->Path.Length;

    status = STATUS_SUCCESS;

Exit:

    if (!NT_SUCCESS(status))
    {
        RtlZeroMemory(UrlInfo, sizeof(*UrlInfo));
    }

    return status;
}

/**
 * \brief Performs a domination check between a calling process and a target
 * process.
 *
 * \details A process dominates the other when the protected level of the
 * process exceeds the other. This domination check is not ideal, it is overly
 * strict and lacks enough information from the kernel to fully understand the
 * protected process state.
 *
 * \param[in] Process The calling process.
 * \param[in] ProcessTarget Target process to check against the caller.
 * \param[in] AccessMode Access mode of the request.
 *
 * \return Appropriate status:
 * STATUS_SUCCESS The calling process dominates the target.
 * STATUS_ACCESS_DENIED The calling process does not dominate the target.
*/
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphDominationCheck(
    _In_ PEPROCESS Process,
    _In_ PEPROCESS ProcessTarget,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    PS_PROTECTION processProtection;
    PS_PROTECTION targetProtection;

    PAGED_CODE();

    if (AccessMode == KernelMode)
    {
        //
        // Give the kernel what it wants...
        //
        return STATUS_SUCCESS;
    }

    //
    // Until Microsoft gives us more insight into protected process domination
    // we'll do a very strict check here:
    //

    processProtection = PsGetProcessProtection(Process);
    targetProtection = PsGetProcessProtection(ProcessTarget);

    if ((targetProtection.Type != PsProtectedTypeNone) &&
        (targetProtection.Type >= processProtection.Type))
    {
        //
        // Calling process protection does not dominate the other, deny access.
        // We could do our own domination check/mapping here with the signing
        // level, but it won't be great and Microsoft might change it, so we'll
        // do this strict check until Microsoft exports:
        // PsTestProtectedProcessIncompatibility
        // RtlProtectedAccess/RtlTestProtectedAccess
        //
        return STATUS_ACCESS_DENIED;
    }

    return STATUS_SUCCESS;
}

/**
 * \brief Performs a domination and privilege check to verify that the calling
 * thread has the required privilege to perform the action.
 *
 * \details This function replaces a call to KphDominationCheck in certain
 * paths. It grants access to a protected process *only* if the calling thread
 * or associated process has been granted permission to do so. Session tokens
 * implement an expiring token validated using asymmetric keys. This involves
 * verification coordinated between the driver, client, and server which
 * requires end user authentication and may be audited or revoked at any time.
 * This feature is a service similar to those provided by various security
 * or system management focused products. System Informer provides this service
 * to users in this same light. System Informer provides security and system
 * management capabilities to users.
 *
 * \param[in] Privileges The specific privileges to be checked.
 * \param[in] Thread The calling thread.
 * \param[in] ProcessTarget Target process to check against the caller.
 * \param[in] AccessMode Describes the access mode of the request.
 *
 * \return Appropriate status:
 * STATUS_SUCCESS The calling thread or process is granted access.
 * STATUS_ACCESS_DENIED The calling process does not dominate the target.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphDominationAndPrivilegeCheck(
    _In_ ULONG Privileges,
    _In_ PETHREAD Thread,
    _In_ PEPROCESS ProcessTarget,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    BOOLEAN granted;
    PKPH_THREAD_CONTEXT thread;

    PAGED_CODE();

    if (AccessMode == KernelMode)
    {
        return STATUS_SUCCESS;
    }

    granted = FALSE;

    thread = KphGetEThreadContext(Thread);
    if (thread)
    {
        granted = KphSessionTokenPrivilegeCheck(thread, Privileges);

        KphDereferenceObject(thread);
    }

    if (granted)
    {
        return STATUS_SUCCESS;
    }

    return KphDominationCheck(PsGetThreadProcess(Thread),
                              ProcessTarget,
                              AccessMode);
}
