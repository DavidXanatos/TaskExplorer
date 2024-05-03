/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2010-2016
 *     jxy-s   2020-2023
 *
 */

#include <kph.h>

#include <trace.h>

typedef struct _KPH_ENUM_FOR_PROTECTION
{
    PKPH_DYN Dyn;
    PKPH_PROCESS_CONTEXT ProcessEnum;
    PKPH_PROCESS_CONTEXT Process;
    NTSTATUS Status;
} KPH_ENUM_FOR_PROTECTION, *PKPH_ENUM_FOR_PROTECTION;

typedef struct _KPH_IMAGE_LOAD_APC
{
    KSI_KAPC Apc;
    PKPH_PROCESS_CONTEXT Process;
    PVOID ImageBase;
    PFILE_OBJECT FileObject;
} KPH_IMAGE_LOAD_APC, *PKPH_IMAGE_LOAD_APC;

typedef struct _KPH_IMAGE_LOAD_APC_INIT
{
    PKPH_PROCESS_CONTEXT Process;
    PVOID ImageBase;
    PFILE_OBJECT FileObject;
} KPH_IMAGE_LOAD_APC_INIT, *PKPH_IMAGE_LOAD_APC_INIT;

KPH_PROTECTED_DATA_SECTION_RO_PUSH();
static const UNICODE_STRING KphpImageLoadApcTypeName = RTL_CONSTANT_STRING(L"KphImageLoadApc");
KPH_PROTECTED_DATA_SECTION_RO_POP();
KPH_PROTECTED_DATA_SECTION_PUSH();
static PKPH_OBJECT_TYPE KphpImageLoadApcType = NULL;
KPH_PROTECTED_DATA_SECTION_POP();
static KPH_REFERENCE KphpDriverUnloadProtectionRef = { 0 };
static PVOID KphpDriverUnloadPreviousRoutine = NULL;

PAGED_FILE();

/**
 * \brief Allocates an image load APC object.
 *
 * \param[in] Size The size to allocate.
 *
 * \return Allocates image load APC object, null on failure.
 */
_Function_class_(KPH_TYPE_ALLOCATE_PROCEDURE)
_Return_allocatesMem_size_(Size)
PVOID KSIAPI KphpAllocateImageLoadApc(
    _In_ SIZE_T Size
    )
{
    PAGED_CODE();

    return KphAllocateNPaged(Size, KPH_TAG_IMAGE_LOAD_APC);
}

/**
 * \brief Initializes image load APC object.
 *
 * \param[in,out] Object The image load APC object to initialize.
 * \param[in] Parameter The image load ACP initialization structure.
 *
 * \return STATUS_SUCCESS
 */
_Function_class_(KPH_TYPE_INITIALIZE_PROCEDURE)
_Must_inspect_result_
NTSTATUS KSIAPI KphpInitializeImageLoadApc(
    _Inout_ PVOID Object,
    _In_opt_ PVOID Parameter
    )
{
    PKPH_IMAGE_LOAD_APC apc;
    PKPH_IMAGE_LOAD_APC_INIT init;

    PAGED_CODE();

    NT_ASSERT(Parameter);

    apc = Object;
    init = Parameter;

    apc->Process = init->Process;
    KphReferenceObject(apc->Process);

    apc->ImageBase = init->ImageBase;

    apc->FileObject = init->FileObject;
    if (apc->FileObject)
    {
        ObReferenceObject(apc->FileObject);
    }

    KphReferenceHashingInfrastructure();
    KphReferenceSigningInfrastructure();

    return STATUS_SUCCESS;
}

/**
 * \brief Deletes image load APC object.
 *
 * \param[in,out] Object The image load APC object to delete.
 */
_Function_class_(KPH_TYPE_DELETE_PROCEDURE)
VOID KSIAPI KphpDeleteImageLoadApc(
    _Inout_ PVOID Object
    )
{
    PKPH_IMAGE_LOAD_APC apc;

    PAGED_CODE();

    apc = Object;

    KphDereferenceObject(apc->Process);

    if (apc->FileObject)
    {
        ObDereferenceObject(apc->FileObject);
    }

    KphDereferenceSigningInfrastructure();
    KphDereferenceHashingInfrastructure();
}

/**
 * \brief Frees an image load APC object.
 *
 * \param[in] Object Image load APC object to free.
 */
_Function_class_(KPH_TYPE_FREE_PROCEDURE)
VOID KSIAPI KphpFreeImageLoadApc(
    _In_freesMem_ PVOID Object
    )
{
    PAGED_CODE();

    KphFree(Object, KPH_TAG_IMAGE_LOAD_APC);
}

/**
 * \brief Checks if object protections should be suppressed.
 *
 * \param[in] Actor The actor process.
 * \param[in] Target The target process.
 * \param[out] Suppress Receives TRUE if object protections should be
 * suppressed, FALSE otherwise.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS KphpShouldSuppressObjectProtections(
    _In_ PKPH_PROCESS_CONTEXT Actor,
    _In_ PKPH_PROCESS_CONTEXT Target,
    _Out_ PBOOLEAN Suppress
    )
{
    NTSTATUS status;
    BOOLEAN isLsass;

    PAGED_CODE_PASSIVE();

    *Suppress = FALSE;

    status = KphDominationCheck(Target->EProcess, Actor->EProcess, UserMode);
    if (!NT_SUCCESS(status))
    {
        //
        // Grant access when the actor is a protected process and the target is
        // not protected at a higher level.
        //
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Protected process %wZ (%lu) access granted to PPL process %wZ (%lu)",
                      &Target->ImageName,
                      HandleToULong(Target->ProcessId),
                      &Actor->ImageName,
                      HandleToULong(Actor->ProcessId));

        *Suppress = TRUE;

        return STATUS_SUCCESS;
    }

    status = KphQueryInformationProcessContext(Actor,
                                               KphProcessContextIsLsass,
                                               &isLsass,
                                               sizeof(isLsass),
                                               NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphQueryInformationProcessContext failed: %!STATUS!",
                      status);

        return status;
    }

    if (isLsass)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Protected process %wZ (%lu) access granted to LSA process %wZ (%lu)",
                      &Target->ImageName,
                      HandleToULong(Target->ProcessId),
                      &Actor->ImageName,
                      HandleToULong(Actor->ProcessId));

        *Suppress = TRUE;
    }

    return STATUS_SUCCESS;
}

/**
 * \brief Handle enumeration callback for protecting a process.
 *
 * \param[in,out] HandleTableEntry The handle table entry being enumerated.
 * \param[in] Handle The handle being enumerated.
 * \param[in] Parameter The enumerate for protection context.
 *
 * \return FALSE to continue enumerating, TRUE to stop.
 */
_Function_class_(KPH_ENUM_PROCESS_HANDLES_CALLBACK)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN KSIAPI KphpEnumProcessHandlesForProtection(
    _Inout_ PHANDLE_TABLE_ENTRY HandleTableEntry,
    _In_ HANDLE Handle,
    _In_opt_ PVOID Parameter
    )
{
    PKPH_ENUM_FOR_PROTECTION parameter;
    POBJECT_HEADER objectHeader;
    PVOID object;
    POBJECT_TYPE objectType;
    ACCESS_MASK grantedAccess;
    ACCESS_MASK allowedAccessMask;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(Parameter);

    parameter = Parameter;

    if (IsKernelHandle(Handle))
    {
        //
        // Don't screw with kernel handles.
        //
        return FALSE;
    }

    objectHeader = KphObpDecodeObject(parameter->Dyn, HandleTableEntry->Object);
    if (!objectHeader)
    {
        //
        // We don't have the dynamic data necessary to do work.
        //
        parameter->Status = STATUS_NOINTERFACE;
        return TRUE;
    }

    object = (PVOID)&objectHeader->Body;
    objectType = ObGetObjectType(object);

    if (objectType == *PsProcessType)
    {
        if (parameter->Process->EProcess != object)
        {
            return FALSE;
        }

        grantedAccess = ObpDecodeGrantedAccess(HandleTableEntry->GrantedAccess);
        allowedAccessMask = parameter->Process->ProcessAllowedMask;

        if ((grantedAccess & allowedAccessMask) != grantedAccess)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          PROTECTION,
                          "Modifying process handle (0x%04x, 0x%08x -> 0x%08x) "
                          "permissions in process %wZ (%lu) for process %wZ (%lu)",
                          HandleToULong(Handle),
                          grantedAccess,
                          (grantedAccess & allowedAccessMask),
                          &parameter->ProcessEnum->ImageName,
                          HandleToULong(parameter->ProcessEnum->ProcessId),
                          &parameter->Process->ImageName,
                          HandleToULong(parameter->Process->ProcessId));

            ObpSetGrantedAccess(&HandleTableEntry->GrantedAccess,
                                (grantedAccess & allowedAccessMask));
        }

        return FALSE;
    }

    if (objectType == *PsThreadType)
    {
        PEPROCESS process;

        process = PsGetThreadProcess(object);

        if (parameter->Process->EProcess != process)
        {
            return FALSE;
        }

        grantedAccess = ObpDecodeGrantedAccess(HandleTableEntry->GrantedAccess);
        allowedAccessMask = parameter->Process->ThreadAllowedMask;

        if ((grantedAccess & allowedAccessMask) != grantedAccess)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          PROTECTION,
                          "Modifying thread handle (0x%04x, 0x%08x -> 0x%08x) "
                          "permissions in process %wZ (%lu) for process %wZ (%lu)",
                          HandleToULong(Handle),
                          grantedAccess,
                          (grantedAccess & allowedAccessMask),
                          &parameter->ProcessEnum->ImageName,
                          HandleToULong(parameter->ProcessEnum->ProcessId),
                          &parameter->Process->ImageName,
                          HandleToULong(parameter->Process->ProcessId));

            ObpSetGrantedAccess(&HandleTableEntry->GrantedAccess,
                                (grantedAccess & allowedAccessMask));
        }
    }

    return FALSE;
}

/**
 * \brief Process context enumeration callback for process protection.
 *
 * \param[in] Process The process context being enumerated.
 * \param[in] Parameter The enumerate for protection context.
 *
 * \return FALSE to continue enumerating, TRUE to stop.
 */
_Function_class_(KPH_ENUM_PROCESS_CONTEXTS_CALLBACK)
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
BOOLEAN KSIAPI KphpEnumProcessContextsForProtection(
    _In_ PKPH_PROCESS_CONTEXT Process,
    _In_opt_ PVOID Parameter
    )
{
    NTSTATUS status;
    PKPH_ENUM_FOR_PROTECTION parameter;
    BOOLEAN suppress;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(Parameter);

    parameter = Parameter;

    if (Process->EProcess == parameter->Process->EProcess)
    {
        return FALSE;
    }

    suppress = FALSE;
    status = KphpShouldSuppressObjectProtections(Process,
                                                 parameter->Process,
                                                 &suppress);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphpShouldSuppressObjectProtections failed: %!STATUS!",
                      status);

        //
        // This usually means we don't have the dynamic data to do work.
        // We can't guarantee both protection and application compatibility.
        //
        parameter->Status = status;

        return TRUE;
    }

    if (suppress)
    {
        return FALSE;
    }

    parameter->ProcessEnum = Process;

    status = KphEnumerateProcessHandlesEx(Process->EProcess,
                                          KphpEnumProcessHandlesForProtection,
                                          parameter);
    if (status == STATUS_NOINTERFACE)
    {
        //
        // This means we don't have the dynamic data necessary to do work.
        // All other errors are failure to access process objects because
        // the process is exiting or has no object table.
        //
        parameter->Status = status;

        return TRUE;
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphEnumerateProcessHandlesEx failed: %!STATUS!",
                      status);
    }

    return FALSE;
}

/**
 * \brief Stops protecting a process.
 *
 * \param[in] Process The process to stop protecting.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphStopProtectingProcess(
    _In_ PKPH_PROCESS_CONTEXT Process
    )
{
    PAGED_CODE_PASSIVE();

    KphAcquireRWLockExclusive(&Process->ProtectionLock);

    Process->Protected = FALSE;
    Process->ProcessAllowedMask = 0;
    Process->ThreadAllowedMask = 0;

    KphReleaseRWLock(&Process->ProtectionLock);

    KphTracePrint(TRACE_LEVEL_INFORMATION,
                  PROTECTION,
                  "Stopped protecting process %wZ (%lu)",
                  &Process->ImageName,
                  HandleToULong(Process->ProcessId));
}

/**
 * \brief Determines if a process is protected.
 *
 * \param[in] Process The process to check.
 *
 * \return TRUE if the process is protected, FALSE otherwise.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN KphIsProtectedProcess(
    _In_ PKPH_PROCESS_CONTEXT Process
    )
{
    BOOLEAN isProtectedProcess;

    PAGED_CODE_PASSIVE();

    KphAcquireRWLockShared(&Process->ProtectionLock);
    isProtectedProcess = Process->Protected ? TRUE : FALSE;
    KphReleaseRWLock(&Process->ProtectionLock);

    return isProtectedProcess;
}

/**
 * \brief Starts protecting a process.
 *
 * \param[in] Process The process to start protecting.
 * \param[in] ProcessAllowedMask Access mask containing the allowed access to
 * the process.
 * \param[in] ThreadAllowedMask Access mask containing the allowed access to
 * the process threads.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphStartProtectingProcess(
    _In_ PKPH_PROCESS_CONTEXT Process,
    _In_ ACCESS_MASK ProcessAllowedMask,
    _In_ ACCESS_MASK ThreadAllowedMask
    )
{
    NTSTATUS status;
    PKPH_DYN dyn;
    BOOLEAN releaseLock;
    SECURITY_SUBJECT_CONTEXT subjectContext;
    BOOLEAN accessGranted;
    KPH_ENUM_FOR_PROTECTION context;

    PAGED_CODE_PASSIVE();

    releaseLock = FALSE;

    dyn = KphReferenceDynData();
    if (!dyn)
    {
        status = STATUS_NOINTERFACE;
        goto Exit;
    }

    SeCaptureSubjectContextEx(NULL, Process->EProcess, &subjectContext);

    accessGranted = KphSinglePrivilegeCheckEx(SeDebugPrivilege,
                                              &subjectContext,
                                              UserMode);

    SeReleaseSubjectContext(&subjectContext);

    if (!accessGranted)
    {
        status = STATUS_PRIVILEGE_NOT_HELD;
        goto Exit;
    }

    KphAcquireRWLockExclusive(&Process->ProtectionLock);
    releaseLock = TRUE;

    if (Process->Protected)
    {
        KphTracePrint(TRACE_LEVEL_INFORMATION,
                      PROTECTION,
                      "Already protecting process %wZ (%lu)",
                      &Process->ImageName,
                      HandleToULong(Process->ProcessId));

        status = STATUS_ALREADY_COMPLETE;
        goto Exit;
    }

    KphTracePrint(TRACE_LEVEL_INFORMATION,
                  PROTECTION,
                  "Started protecting process %wZ (%lu)",
                  &Process->ImageName,
                  HandleToULong(Process->ProcessId));

    Process->Protected = TRUE;
    Process->ProcessAllowedMask = ProcessAllowedMask;
    Process->ThreadAllowedMask = ThreadAllowedMask;

    context.Dyn = dyn;
    context.Status = STATUS_SUCCESS;
    context.Process = Process;

    KphEnumerateProcessContexts(KphpEnumProcessContextsForProtection, &context);

    status = context.Status;

    if (!NT_SUCCESS(status))
    {
        Process->Protected = FALSE;
        Process->ProcessAllowedMask = 0;
        Process->ThreadAllowedMask = 0;
    }

Exit:

    if (releaseLock)
    {
        KphReleaseRWLock(&Process->ProtectionLock);
    }

    if (dyn)
    {
        KphDereferenceObject(dyn);
    }

    return status;
}

/**
 * \brief Returns true if we should permit extending handle permissions for
 * the creator process.
 *
 * \param[in] Info Object pre operation information.
 * \param[in] Actor The actor thread.
 * \param[in] ProcessTarget The process to check against.
 *
 * \return TRUE if we should permit, FALSE otherwise.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
BOOLEAN
KphpShouldPermitCreatorProcess(
    _In_ POB_PRE_OPERATION_INFORMATION Info,
    _In_ PKPH_THREAD_CONTEXT Actor,
    _In_ PKPH_PROCESS_CONTEXT Process
    )
{
    KPH_PROCESS_STATE processState;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(Process->VerifiedProcess);

    if (Info->Operation == OB_OPERATION_HANDLE_DUPLICATE)
    {
        return FALSE;
    }

    NT_ASSERT(Info->Operation == OB_OPERATION_HANDLE_CREATE);

    if (!Actor->IsCreatingProcess ||
        (Actor->IsCreatingProcessId != Process->ProcessId))
    {
        return FALSE;
    }

    processState = KphGetProcessState(Actor->ProcessContext);

    return ((processState & KPH_PROCESS_STATE_MINIMUM) == KPH_PROCESS_STATE_MINIMUM);
}

/**
 * \brief Applies object protections to protected processes. Invoked by the
 * object manager callbacks.
 *
 * \param[in,out] Info Object pre operation information to apply protections.
 */
#ifndef KPP_NO_SECURITY
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphApplyObProtections(
    _Inout_ POB_PRE_OPERATION_INFORMATION Info
    )
{
    NTSTATUS status;
    PKPH_PROCESS_CONTEXT process;
    PKPH_THREAD_CONTEXT actor;
    BOOLEAN releaseLock;
    BOOLEAN suppress;
    ACCESS_MASK allowedAccessMask;
    ACCESS_MASK desiredAccess;
    PACCESS_MASK access;

    PAGED_CODE_PASSIVE();

    process = NULL;
    actor = NULL;
    releaseLock = FALSE;

    if ((Info->ObjectType != *PsProcessType) &&
        (Info->ObjectType != *PsThreadType))
    {
        goto Exit;
    }

    if (Info->KernelHandle)
    {
        goto Exit;
    }

    actor = KphGetCurrentThreadContext();
    if (!actor || !actor->ProcessContext)
    {
        goto Exit;
    }

    if (Info->ObjectType == *PsProcessType)
    {
        process = KphGetEProcessContext(Info->Object);
    }
    else
    {
        PKPH_THREAD_CONTEXT thread;

        NT_ASSERT(Info->ObjectType == *PsThreadType);

        thread = KphGetEThreadContext(Info->Object);
        if (thread)
        {
            process = thread->ProcessContext;
            if (process)
            {
                KphReferenceObject(process);
            }

            KphDereferenceObject(thread);
        }
    }

    if (!process || (process->EProcess == actor->ProcessContext->EProcess))
    {
        goto Exit;
    }

    KphAcquireRWLockShared(&process->ProtectionLock);
    releaseLock = TRUE;

    if (!process->Protected)
    {
        goto Exit;
    }

    suppress = FALSE;
    status = KphpShouldSuppressObjectProtections(actor->ProcessContext,
                                                 process,
                                                 &suppress);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_WARNING,
                      PROTECTION,
                      "KphpShouldSuppressObjectProtections failed: %!STATUS!",
                      status);

        //
        // We shouldn't get here since we would have succeeded when starting to
        // protect the process to begin with. So if we fail here we fail-safe
        // and do not suppress.
        //
        suppress = FALSE;
    }

    if (suppress)
    {
        goto Exit;
    }

    if (Info->Operation == OB_OPERATION_HANDLE_CREATE)
    {
        access = &Info->Parameters->CreateHandleInformation.DesiredAccess;
        desiredAccess = *access;
    }
    else
    {
        NT_ASSERT(Info->Operation == OB_OPERATION_HANDLE_DUPLICATE);

        access = &Info->Parameters->DuplicateHandleInformation.DesiredAccess;
        desiredAccess = *access;
    }

    if (Info->ObjectType == *PsProcessType)
    {
        allowedAccessMask = process->ProcessAllowedMask;

        if (KphpShouldPermitCreatorProcess(Info, actor, process))
        {
            allowedAccessMask |= (KPH_PROCESS_READ_ACCESS |
                                  PROCESS_TERMINATE |
                                  PROCESS_VM_OPERATION |
                                  PROCESS_VM_WRITE);

            if ((allowedAccessMask & process->ProcessAllowedMask)
                != allowedAccessMask)
            {
                KphTracePrint(TRACE_LEVEL_VERBOSE,
                              PROTECTION,
                              "Permitting extra process handle access "
                              "(0x%08x -> 0x%08x) in creator process %wZ (%lu) for "
                              "process %wZ (%lu)",
                              process->ProcessAllowedMask,
                              allowedAccessMask,
                              &actor->ProcessContext->ImageName,
                              HandleToULong(actor->ProcessContext->ProcessId),
                              &process->ImageName,
                              HandleToULong(process->ProcessId));
            }
        }

        if ((desiredAccess & allowedAccessMask) != desiredAccess)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          PROTECTION,
                          "Stripping process handle (0x%08x -> 0x%08x) "
                          "permissions in process %wZ (%lu) for process %wZ (%lu) (0x%08x)",
                          desiredAccess,
                          (desiredAccess & allowedAccessMask),
                          &actor->ProcessContext->ImageName,
                          HandleToULong(actor->ProcessContext->ProcessId),
                          &process->ImageName,
                          HandleToULong(process->ProcessId),
                          allowedAccessMask);

            *access = (desiredAccess & allowedAccessMask);
        }
    }
    else
    {
        NT_ASSERT(Info->ObjectType == *PsThreadType);

        allowedAccessMask = process->ThreadAllowedMask;

        if (KphpShouldPermitCreatorProcess(Info, actor, process))
        {
            allowedAccessMask |= (KPH_THREAD_READ_ACCESS |
                                  THREAD_TERMINATE |
                                  THREAD_RESUME);

            if ((allowedAccessMask & process->ThreadAllowedMask)
                != allowedAccessMask)
            {
                KphTracePrint(TRACE_LEVEL_VERBOSE,
                              PROTECTION,
                              "Permitting extra thread handle access "
                              "(0x%08x -> 0x%08x) in creator process %wZ (%lu) for "
                              "process %wZ (%lu)",
                              process->ThreadAllowedMask,
                              allowedAccessMask,
                              &actor->ProcessContext->ImageName,
                              HandleToULong(actor->ProcessContext->ProcessId),
                              &process->ImageName,
                              HandleToULong(process->ProcessId));
            }
        }

        if ((desiredAccess & allowedAccessMask) != desiredAccess)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          PROTECTION,
                          "Stripping thread handle (0x%08x -> 0x%08x) "
                          "permissions in process %wZ (%lu) for process %wZ (%lu) (0x%08x)",
                          desiredAccess,
                          (desiredAccess & allowedAccessMask),
                          &actor->ProcessContext->ImageName,
                          HandleToULong(actor->ProcessContext->ProcessId),
                          &process->ImageName,
                          HandleToULong(process->ProcessId),
                          allowedAccessMask);

            *access = (desiredAccess & allowedAccessMask);
        }
    }

Exit:

    if (process)
    {
        if (releaseLock)
        {
            KphReleaseRWLock(&process->ProtectionLock);
        }

        KphDereferenceObject(process);
    }

    if (actor)
    {
        KphDereferenceObject(actor);
    }
}
#endif

/**
 * \brief Cleanup routine for the image load APC.
 *
 * \param[in] Apc The APC to clean up.
 * \param[in] Reason Unused.
 */
_Function_class_(KSI_KCLEANUP_ROUTINE)
_IRQL_requires_min_(PASSIVE_LEVEL)
_IRQL_requires_max_(APC_LEVEL)
_IRQL_requires_same_
VOID KSIAPI KphpImageLoadCleanupRoutine(
    _In_ PKSI_KAPC Apc,
    _In_ KSI_KAPC_CLEANUP_REASON Reason
    )
{
    PKPH_IMAGE_LOAD_APC apc;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(Reason);

    apc = CONTAINING_RECORD(Apc, KPH_IMAGE_LOAD_APC, Apc);

    KphDereferenceObject(apc);
}

/**
 * \brief Normal kernel APC routine where we carry out image load denial.
 *
 * \param[in] NormalContext Unused.
 * \param[in] SystemArgument1 Image load APC object.
 * \param[in] SystemArgument2 Unused.
 */
_Function_class_(KNORMAL_ROUTINE)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID NTAPI KphpImageLoadKernelNormalRoutine(
    _In_opt_ PVOID NormalContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    )
{
    NTSTATUS status;
    PKPH_IMAGE_LOAD_APC apc;
    KAPC_STATE apcState;
    BOOLEAN attachToTarget;

    PAGED_CODE_PASSIVE();

    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument2);

    NT_ASSERT(SystemArgument1);

    apc = SystemArgument1;

    NT_ASSERT(apc->ImageBase <= MmHighestUserAddress);

    attachToTarget = (apc->Process->EProcess != PsGetCurrentProcess());
    if (attachToTarget)
    {
        KeStackAttachProcess(apc->Process->EProcess, &apcState);
    }

    status = ZwUnmapViewOfSection(ZwCurrentProcess(), apc->ImageBase);

    if (attachToTarget)
    {
        KeUnstackDetachProcess(&apcState);
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "ZwUnmapViewOfSection failed (%wZ (%lu), %p): %!STATUS!",
                      &apc->Process->ImageName,
                      HandleToULong(apc->Process->ProcessId),
                      apc->ImageBase,
                      status);

        InterlockedIncrementSizeT(&apc->Process->NumberOfUntrustedImageLoads);
    }
    else
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Unmapped %p from process %wZ (%lu)",
                      apc->ImageBase,
                      &apc->Process->ImageName,
                      HandleToULong(apc->Process->ProcessId));
    }
}

/**
 * \param Second kernel APC routine, say hi on the way down to passive :).
 *
 * \param[in] Apc Unused.
 * \param[in,out] NormalRoutine Pointer to the normal kernel APC routine.
 * \param[in,out] NormalContext Unused.
 * \param[in,out] SystemArgument1 Pointer to the image load APC object.
 * \param[in,out] SystemArgument2 Unused.
 */
_Function_class_(KSI_KKERNEL_ROUTINE)
_IRQL_requires_(APC_LEVEL)
_IRQL_requires_same_
VOID KSIAPI KphpImageLoadKernelRoutineSecond(
    _In_ PKSI_KAPC Apc,
    _Inout_ _Deref_pre_maybenull_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ _Deref_pre_maybenull_ PVOID* NormalContext,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument1,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument2
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Apc);
    DBG_UNREFERENCED_PARAMETER(NormalRoutine);
    UNREFERENCED_PARAMETER(NormalContext);
    DBG_UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    NT_ASSERT(*NormalRoutine == KphpImageLoadKernelNormalRoutine);
    NT_ASSERT(*SystemArgument1);
    NT_ASSERT(KphGetObjectType(*SystemArgument1) == KphpImageLoadApcType);
}

/**
 * \brief First kernel APC routine, fired by the thread returning from the
 * system. We will stage another normal kernel APC to fire at passive here.
 *
 * \param[in] Apc The APC object that is executing.
 * \param[in,out] NormalRoutine Points to the faked routine, we'll cancel it.
 * \param[in,out] NormalContext Unused.
 * \param[in,out] SystemArgument1 Unused.
 * \param[in,out] SystemArgument2 Unused.
 */
_Function_class_(KSI_KKERNEL_ROUTINE)
_IRQL_requires_(APC_LEVEL)
_IRQL_requires_same_
VOID KSIAPI KphpImageLoadKernelRoutineFirst(
    _In_ PKSI_KAPC Apc,
    _Inout_ _Deref_pre_maybenull_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ _Deref_pre_maybenull_ PVOID* NormalContext,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument1,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument2
    )
{
    NTSTATUS status;
    PKPH_IMAGE_LOAD_APC firstApc;
    PKPH_IMAGE_LOAD_APC secondApc;
    KPH_IMAGE_LOAD_APC_INIT init;
#if DBG
    PKPH_THREAD_CONTEXT actor;
#endif

    PAGED_CODE();

    secondApc = NULL;

    firstApc = CONTAINING_RECORD(Apc, KPH_IMAGE_LOAD_APC, Apc);

    DBG_UNREFERENCED_PARAMETER(NormalRoutine);
#if DBG
    actor = KphGetCurrentThreadContext();
    NT_ASSERT(actor && actor->ProcessContext);
    NT_ASSERT(actor->ProcessContext->ApcNoopRoutine);
    NT_ASSERT(*NormalRoutine == actor->ProcessContext->ApcNoopRoutine);
    KphDereferenceObject(actor);
    actor = NULL;
#endif

    *NormalContext = NULL;
    *SystemArgument1 = NULL;
    *SystemArgument2 = NULL;

    init.Process = firstApc->Process;
    init.ImageBase = firstApc->ImageBase;
    init.FileObject = NULL;
    status = KphCreateObject(KphpImageLoadApcType,
                             sizeof(KPH_IMAGE_LOAD_APC),
                             &secondApc,
                             &init);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphCreateObject failed: %!STATUS!",
                      status);


        secondApc = NULL;
        goto Exit;
    }

    //
    // Initialize a normal kernel ACP to drop down to passive.
    //
    KsiInitializeApc(&secondApc->Apc,
                     KphDriverObject,
                     KeGetCurrentThread(),
                     OriginalApcEnvironment,
                     KphpImageLoadKernelRoutineSecond,
                     KphpImageLoadCleanupRoutine,
                     KphpImageLoadKernelNormalRoutine,
                     KernelMode,
                     NULL);

    if (!KsiInsertQueueApc(&secondApc->Apc, secondApc, NULL, IO_NO_INCREMENT))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KsiInsertQueueApc failed");

        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    secondApc = NULL;
    status = STATUS_SUCCESS;

Exit:

    if (secondApc)
    {
        KphDereferenceObject(secondApc);
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Untrusted image load (%wZ (%lu), %p): %!STATUS!",
                      &firstApc->Process->ImageName,
                      HandleToULong(firstApc->Process->ProcessId),
                      firstApc->ImageBase,
                      status);

        //
        // No choice other than to just track that there is an untrusted image.
        //
        InterlockedIncrementSizeT(&firstApc->Process->NumberOfUntrustedImageLoads);
    }
}

/**
 * \brief Handles an untrusted image load in a verified process.
 *
 * \details If a failure is encountered in this path or if image load denial
 * is disabled we will denote in the target process that an untrusted image
 * was loaded. The approach here is to queue a user APC to execute when
 * the thread returns from the system. Then, we issue another APC to drop
 * down to passive level and remove the image mapping from the process.
 *
 * \param[in,out] Process The process where the image is being loaded.
 * \param[in] ImageBase The base address of the untrusted image.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphpHandleUntrustedImageLoad(
    _Inout_ PKPH_PROCESS_CONTEXT Process,
    _In_ PVOID ImageBase
    )
{
    NTSTATUS status;
    PKPH_THREAD_CONTEXT actor;
    KPH_IMAGE_LOAD_APC_INIT init;
    PKPH_IMAGE_LOAD_APC apc;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(Process->VerifiedProcess);

    status = STATUS_UNSUCCESSFUL;
    actor = NULL;
    apc = NULL;

    if (KphParameterFlags.DisableImageLoadProtection)
    {
        goto Exit;
    }

    actor = KphGetCurrentThreadContext();
    if (!actor || !actor->ProcessContext)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Insufficient tracking.");

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = KphCheckProcessApcNoopRoutine(actor->ProcessContext);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphCheckProcessApcNoopRoutine failed: %!STATUS!",
                      status);
        goto Exit;
    }

    if (ImageBase > MmHighestUserAddress)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE, PROTECTION, "Invalid image base!");

        //
        // This isn't an error, we just check for safety downstream.
        //
        status = STATUS_SUCCESS;
        goto Exit;
    }

    init.Process = Process;
    init.ImageBase = ImageBase;
    init.FileObject = NULL;
    status = KphCreateObject(KphpImageLoadApcType,
                             sizeof(KPH_IMAGE_LOAD_APC),
                             &apc,
                             &init);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphCreateObject failed: %!STATUS!",
                      status);

        apc = NULL;
        goto Exit;
    }

    KsiInitializeApc(&apc->Apc,
                     KphDriverObject,
                     actor->EThread,
                     OriginalApcEnvironment,
                     KphpImageLoadKernelRoutineFirst,
                     KphpImageLoadCleanupRoutine,
                     actor->ProcessContext->ApcNoopRoutine,
                     UserMode,
                     NULL);

    if (!KsiInsertQueueApc(&apc->Apc, NULL, NULL, IO_NO_INCREMENT))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KsiInsertQueueApc failed");

        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    //
    // Stage the thread for user mode APC delivery.
    //
    KeTestAlertThread(UserMode);

    apc = NULL;
    status = STATUS_SUCCESS;

Exit:

    if (apc)
    {
        KphDereferenceObject(apc);
    }

    if (actor)
    {
        KphDereferenceObject(actor);
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Untrusted image load (%wZ (%lu), %p): %!STATUS!",
                      &Process->ImageName,
                      HandleToULong(Process->ProcessId),
                      ImageBase,
                      status);

        //
        // No choice other than to just track that there is an untrusted image.
        //
        InterlockedIncrementSizeT(&Process->NumberOfUntrustedImageLoads);
    }
}

/**
 * \brief Applies image protections on verified processes.
 *
 * \param[in,out] Process The process where the image is being loaded.
 * \param[in] ImageBase The base address of the image.
 * \param[in] FileObject The file object for the image being loaded.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphpApplyImageProtections(
    _Inout_ PKPH_PROCESS_CONTEXT Process,
    _In_ PVOID ImageBase,
    _In_ PFILE_OBJECT FileObject
    )
{
    NTSTATUS status;
    KPH_SIGNING_INFORMATION info;
    ANSI_STRING issuer;
    ANSI_STRING subject;
    PUNICODE_STRING fileName;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(!KeAreAllApcsDisabled());

    fileName = NULL;

    if (!FileObject->SectionObjectPointer ||
        MmDoesFileHaveUserWritableReferences(FileObject->SectionObjectPointer))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "%wZ (%lu) image has user writable references",
                      &Process->ImageName,
                      HandleToULong(Process->ProcessId));

        KphpHandleUntrustedImageLoad(Process, ImageBase);
        goto Exit;
    }

    status = KphGetNameFileObject(FileObject, &fileName);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "%wZ (%lu) KphGetNameFileObject failed: %!STATUS!",
                      &Process->ImageName,
                      HandleToULong(Process->ProcessId),
                      status);

        fileName = NULL;
        KphpHandleUntrustedImageLoad(Process, ImageBase);
        goto Exit;
    }

    //
    // Do the verification since it will be a faster check than asking CI.
    //

    status = KphVerifyFile(fileName);

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  PROTECTION,
                  "KphVerifyFile: %wZ (%lu) \"%wZ\": %!STATUS!",
                  &Process->ImageName,
                  HandleToULong(Process->ProcessId),
                  fileName,
                  status);

    if (NT_SUCCESS(status))
    {
        InterlockedIncrementSizeT(&Process->NumberOfVerifiedImageLoads);
        goto Exit;
    }

    //
    // We have to ask CI for the signing information.
    //

    RtlZeroMemory(&issuer, sizeof(issuer));
    RtlZeroMemory(&subject, sizeof(subject));

    status = KphGetSigningInformation(fileName, &info);

    //
    // Pull out the issuer and subject if it's there. The size check for
    // PolicyInfo.Size is correct. Checking for when the ChainInfo started
    // exposing the chain elements (Win10).
    //
    if ((info.PolicyInfo.Size >=
         RTL_SIZEOF_THROUGH_FIELD(MINCRYPT_POLICY_INFO, ValidToTime)) &&
        info.PolicyInfo.ChainInfo &&
        info.PolicyInfo.ChainInfo->ChainElements &&
        (info.PolicyInfo.ChainInfo->NumberOfChainElements > 0))
    {
        issuer.Buffer = info.PolicyInfo.ChainInfo->ChainElements[0].Issuer.Buffer;
        issuer.Length = info.PolicyInfo.ChainInfo->ChainElements[0].Issuer.Length;
        issuer.MaximumLength = issuer.Length;

        subject.Buffer = info.PolicyInfo.ChainInfo->ChainElements[0].Subject.Buffer;
        subject.Length = info.PolicyInfo.ChainInfo->ChainElements[0].Subject.Length;
        subject.MaximumLength = subject.Length;
    }

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  PROTECTION,
                  "KphGetSigningInfoByFileName: \"%wZ\" 0x%08lx \"%wZ\" "
                  "\"%Z\" \"%Z\" %!STATUS! %!STATUS!",
                  fileName,
                  info.PolicyInfo.PolicyBits,
                  &info.CatalogName,
                  &subject,
                  &issuer,
                  info.PolicyInfo.VerificationStatus,
                  status);

    if (!NT_SUCCESS(status) ||
        !NT_SUCCESS(info.PolicyInfo.VerificationStatus) ||
        BooleanFlagOn(info.PolicyInfo.PolicyBits,
                      (MINCRYPT_POLICY_ERROR_FLAGS |
                       MINCRYPT_POLICY_3RD_PARTY_ROOT |
                       MINCRYPT_POLICY_NO_ROOT |
                       MINCRYPT_POLICY_OTHER_ROOT)))
    {
        KphpHandleUntrustedImageLoad(Process, ImageBase);
    }
    else
    {
        InterlockedIncrementSizeT(&Process->NumberOfMicrosoftImageLoads);
    }

    KphFreeSigningInformation(&info);

Exit:

    if (fileName)
    {
        KphFreeNameFileObject(fileName);
    }
}

/**
 * \brief Normal kernel APC routine for when we must get outside of APC
 * disablement.
 *
 * \param[in] NormalContext Unused.
 * \param[in] SystemArgument1 Image load APC object.
 * \param[in] SystemArgument2 Unused.
 */
_Function_class_(KNORMAL_ROUTINE)
_IRQL_requires_(PASSIVE_LEVEL)
_IRQL_requires_same_
VOID NTAPI KphpImageLoadKernelNormalRoutineApcsDisabled(
    _In_opt_ PVOID NormalContext,
    _In_opt_ PVOID SystemArgument1,
    _In_opt_ PVOID SystemArgument2
    )
{
    PKPH_IMAGE_LOAD_APC apc;

    PAGED_CODE_PASSIVE();

    UNREFERENCED_PARAMETER(NormalContext);
    UNREFERENCED_PARAMETER(SystemArgument2);

    NT_ASSERT(SystemArgument1);

    apc = SystemArgument1;

    NT_ASSERT(apc->FileObject);

    KphpApplyImageProtections(apc->Process, apc->ImageBase, apc->FileObject);
}

/**
 * \brief Kernel APC routine for when we must get outside of APC disablement.
 *
 * \param[in] Apc Unused.
 * \param[in,out] NormalRoutine Pointer to the normal kernel APC routine.
 * \param[in,out] NormalContext Unused.
 * \param[in,out] SystemArgument1 Pointer to the image load APC object.
 * \param[in,out] SystemArgument2 Unused.
 */
_Function_class_(KSI_KKERNEL_ROUTINE)
_IRQL_requires_(APC_LEVEL)
_IRQL_requires_same_
VOID KSIAPI KphpImageLoadKernelRoutineApcsDisabled(
    _In_ PKSI_KAPC Apc,
    _Inout_ _Deref_pre_maybenull_ PKNORMAL_ROUTINE* NormalRoutine,
    _Inout_ _Deref_pre_maybenull_ PVOID* NormalContext,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument1,
    _Inout_ _Deref_pre_maybenull_ PVOID* SystemArgument2
    )
{
    PAGED_CODE();

    UNREFERENCED_PARAMETER(Apc);
    DBG_UNREFERENCED_PARAMETER(NormalRoutine);
    UNREFERENCED_PARAMETER(NormalContext);
    DBG_UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(SystemArgument2);

    NT_ASSERT(*NormalRoutine == KphpImageLoadKernelNormalRoutineApcsDisabled);
    NT_ASSERT(*SystemArgument1);
    NT_ASSERT(KphGetObjectType(*SystemArgument1) == KphpImageLoadApcType);
}

/**
 * \brief Applies image protections on verified processes.
 *
 * \param[in,out] Process The process where the image is being loaded.
 * \param[in] ImageBase The base address of the image.
 * \param[in] FileObject The file object for the image being loaded.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphpApplyImageProtectionsApcsDisabled(
    _Inout_ PKPH_PROCESS_CONTEXT Process,
    _In_ PVOID ImageBase,
    _In_ PFILE_OBJECT FileObject
    )
{
    NTSTATUS status;
    PKPH_IMAGE_LOAD_APC apc;
    KPH_IMAGE_LOAD_APC_INIT init;

    PAGED_CODE_PASSIVE();

    NT_ASSERT(KeAreAllApcsDisabled());

    init.Process = Process;
    init.ImageBase = ImageBase;
    init.FileObject = FileObject;
    status = KphCreateObject(KphpImageLoadApcType,
                             sizeof(KPH_IMAGE_LOAD_APC),
                             &apc,
                             &init);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphCreateObject failed: %!STATUS!",
                      status);

        apc = NULL;
        goto Exit;
    }

    //
    // Initialize a normal kernel ACP to handle this outside of the callback.
    //
    KsiInitializeApc(&apc->Apc,
                     KphDriverObject,
                     KeGetCurrentThread(),
                     OriginalApcEnvironment,
                     KphpImageLoadKernelRoutineApcsDisabled,
                     KphpImageLoadCleanupRoutine,
                     KphpImageLoadKernelNormalRoutineApcsDisabled,
                     KernelMode,
                     NULL);

    if (!KsiInsertQueueApc(&apc->Apc, apc, NULL, IO_NO_INCREMENT))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KsiInsertQueueApc failed");

        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    apc = NULL;
    status = STATUS_SUCCESS;

Exit:

    if (apc)
    {
        KphDereferenceObject(apc);
    }

    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "Untrusted image load (%wZ (%lu), %p): %!STATUS!",
                      &Process->ImageName,
                      HandleToULong(Process->ProcessId),
                      ImageBase,
                      status);

        //
        // No choice other than to just track that there is an untrusted image.
        //
        InterlockedIncrementSizeT(&Process->NumberOfUntrustedImageLoads);
    }
}

/**
 * \brief Applies image protections on verified processes.
 *
 * \param[in,out] Process The process where the image is being loaded.
 * \param[in] ImageInfo The image info from the notification routine.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphApplyImageProtections(
    _Inout_ PKPH_PROCESS_CONTEXT Process,
    _In_ PIMAGE_INFO_EX ImageInfo
    )
{
    PAGED_CODE_PASSIVE();

    KphAcquireRWLockShared(&Process->ProtectionLock);

    if (!Process->VerifiedProcess || !Process->Protected)
    {
        goto Exit;
    }

    if ((ImageInfo->ImageInfo.ImageSignatureLevel == SE_SIGNING_LEVEL_MICROSOFT) ||
        (ImageInfo->ImageInfo.ImageSignatureLevel == SE_SIGNING_LEVEL_WINDOWS) ||
        (ImageInfo->ImageInfo.ImageSignatureLevel == SE_SIGNING_LEVEL_WINDOWS_TCB))
    {
        InterlockedIncrementSizeT(&Process->NumberOfMicrosoftImageLoads);
        goto Exit;
    }

    if (ImageInfo->ImageInfo.ImageSignatureLevel == SE_SIGNING_LEVEL_ANTIMALWARE)
    {
        InterlockedIncrementSizeT(&Process->NumberOfAntimalwareImageLoads);
        goto Exit;
    }

    //
    // We have to do a signing check ourselves which involves looking up the
    // file name. If we can do it now, do so, otherwise handle it later.
    //

    if (KeAreAllApcsDisabled())
    {
        //
        // We can't safely (or accurately) look up the file name. We have to
        // special case it.
        //
        KphpApplyImageProtectionsApcsDisabled(Process,
                                              ImageInfo->ImageInfo.ImageBase,
                                              ImageInfo->FileObject);
    }
    else
    {
        KphpApplyImageProtections(Process,
                                  ImageInfo->ImageInfo.ImageBase,
                                  ImageInfo->FileObject);
    }

Exit:

    KphReleaseRWLock(&Process->ProtectionLock);
}

/**
 * \brief Acquires a reference to the driver unload protection. Enables driver
 * unload protection if this is the first reference.
 *
 * \param[out] PreviousCount Optionally set to the previous reference count.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphAcquireDriverUnloadProtection(
    _Out_opt_ PLONG PreviousCount
    )
{
    NTSTATUS status;
    LONG previousCount;

    PAGED_CODE();

    status = KphAcquireReference(&KphpDriverUnloadProtectionRef,
                                 &previousCount);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphAcquireReference failed: %!STATUS!",
                      status);

        previousCount = 0;
        goto Exit;
    }

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  PROTECTION,
                  "Acquired driver unload protection (%ld)",
                  previousCount + 1);

    if (previousCount == 0)
    {
#pragma prefast(push)
#pragma prefast(disable : 28175)

        NT_ASSERT(KphDriverObject->DriverUnload);
        NT_ASSERT(!KphpDriverUnloadPreviousRoutine);

        KphpDriverUnloadPreviousRoutine = InterlockedExchangePointer(
                               (volatile PVOID*)&KphDriverObject->DriverUnload,
                               NULL);

        NT_ASSERT(!KphDriverObject->DriverUnload);
        NT_ASSERT(KphpDriverUnloadPreviousRoutine);

#pragma prefast(pop)

        KphTracePrint(TRACE_LEVEL_INFORMATION,
                      PROTECTION,
                      "Driver unload protection activated");
    }

Exit:

    if (PreviousCount)
    {
        *PreviousCount = previousCount;
    }

    return STATUS_SUCCESS;
}

/**
 * \brief Releases a reference to the driver unload protection. Disables driver
 * unload protection if this is the last reference.
 *
 * \param[out] PreviousCount Optionally set to the previous reference count.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(APC_LEVEL)
_Must_inspect_result_
NTSTATUS KphReleaseDriverUnloadProtection(
    _Out_opt_ PLONG PreviousCount
    )
{
    NTSTATUS status;
    LONG previousCount;

    PAGED_CODE();

    status = KphReleaseReference(&KphpDriverUnloadProtectionRef,
                                 &previousCount);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphReleaseReference failed: %!STATUS!",
                      status);

        previousCount = 0;
        goto Exit;
    }

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  PROTECTION,
                  "Released driver unload protection (%ld)",
                  previousCount - 1);

    if (previousCount == 1)
    {
#pragma prefast(push)
#pragma prefast(disable : 28175)

        NT_ASSERT(!KphDriverObject->DriverUnload);
        NT_ASSERT(KphpDriverUnloadPreviousRoutine);

        KphpDriverUnloadPreviousRoutine = InterlockedExchangePointer(
                               (volatile PVOID*)&KphDriverObject->DriverUnload,
                               KphpDriverUnloadPreviousRoutine);

        NT_ASSERT(KphDriverObject->DriverUnload);
        NT_ASSERT(!KphpDriverUnloadPreviousRoutine);

#pragma prefast(pop)

        KphTracePrint(TRACE_LEVEL_INFORMATION,
                      PROTECTION,
                      "Driver unload protection deactivated");
    }

Exit:

    if (PreviousCount)
    {
        *PreviousCount = previousCount;
    }

    return status;
}

/**
 * \brief Retrieves the current driver unload protection reference count.
 *
 * \details If the driver unload protection reference count is greater than
 * zero, the driver unload protection is active.
 *
 * \return The current driver unload protection reference count.
 */
_IRQL_requires_max_(APC_LEVEL)
LONG KphGetDriverUnloadProtectionCount(
    VOID
    )
{
    LONG count;

    PAGED_CODE();

    count = KphpDriverUnloadProtectionRef.Count;
    MemoryBarrier();

    return count;
}

/**
 * \brief Strips the process and thread allowed masks from a protected process.
 *
 * \details Callers may only strip allowed masks. In other words, callers may
 * only make the protection more restrictive.
 *
 * \param[in] Process The protected process to strip the masks from.
 * \param[in] ProcessAllowedMask The process allowed mask to strip.
 * \param[in] ThreadAllowedMask The thread allowed mask to strip.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphpStripProtectedProcessMasks(
    _In_ PKPH_PROCESS_CONTEXT Process,
    _In_ ACCESS_MASK ProcessAllowedMask,
    _In_ ACCESS_MASK ThreadAllowedMask
    )
{
    NTSTATUS status;
    PKPH_DYN dyn;
    KPH_ENUM_FOR_PROTECTION context;
    ACCESS_MASK prevProcessAllowedMask;
    ACCESS_MASK prevThreadAllowedMask;

    PAGED_CODE_PASSIVE();

    dyn = KphReferenceDynData();
    if (!dyn)
    {
        return STATUS_NOINTERFACE;
    }

    KphAcquireRWLockExclusive(&Process->ProtectionLock);

    if (!Process->Protected)
    {
        status = STATUS_INVALID_PARAMETER;
        goto Exit;
    }

    prevProcessAllowedMask = Process->ProcessAllowedMask;
    prevThreadAllowedMask = Process->ThreadAllowedMask;

    Process->ProcessAllowedMask &= ~ProcessAllowedMask;
    Process->ThreadAllowedMask &= ~ThreadAllowedMask;

    KphTracePrint(TRACE_LEVEL_VERBOSE,
                  PROTECTION,
                  "Modifying protected process %wZ (%lu) allowed masks, "
                  "process: 0x%08x -> 0x%08x, "
                  "thread: 0x%08x -> 0x%08x",
                  &Process->ImageName,
                  HandleToULong(Process->ProcessId),
                  prevProcessAllowedMask,
                  Process->ProcessAllowedMask,
                  prevThreadAllowedMask,
                  Process->ThreadAllowedMask);

    if ((Process->ProcessAllowedMask == prevProcessAllowedMask) &&
        (Process->ThreadAllowedMask == prevThreadAllowedMask))
    {
        status = STATUS_SUCCESS;
        goto Exit;
    }

    context.Dyn = dyn;
    context.Status = STATUS_SUCCESS;
    context.Process = Process;

    KphEnumerateProcessContexts(KphpEnumProcessContextsForProtection, &context);

    status = context.Status;

    if (!NT_SUCCESS(status))
    {
        Process->ProcessAllowedMask = prevProcessAllowedMask;
        Process->ThreadAllowedMask = prevThreadAllowedMask;
    }

Exit:

    KphReleaseRWLock(&Process->ProtectionLock);

    KphDereferenceObject(dyn);

    return status;
}
/**
 * \brief Strips the process and thread allowed masks from a protected process.
 *
 * \details Callers may only strip allowed masks. In other words, callers may
 * only make the protection more restrictive.
 *
 * \param[in] ProcessHandle A handle to a process to strip the masks from.
 * \param[in] ProcessAllowedMask The process allowed mask to strip.
 * \param[in] ThreadAllowedMask The thread allowed mask to strip.
 * \param[in] AccessMode The mode in which to perform access checks.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphStripProtectedProcessMasks(
    _In_ HANDLE ProcessHandle,
    _In_ ACCESS_MASK ProcessAllowedMask,
    _In_ ACCESS_MASK ThreadAllowedMask,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status;
    PEPROCESS processObject;
    PKPH_PROCESS_CONTEXT processContext;

    PAGED_CODE_PASSIVE();

    processContext = NULL;

    status = ObReferenceObjectByHandle(ProcessHandle,
                                       PROCESS_SET_INFORMATION,
                                       *PsProcessType,
                                       AccessMode,
                                       &processObject,
                                       NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "ObReferenceObjectByHandle failed: %!STATUS!",
                      status);

        processObject = NULL;
        goto Exit;
    }

    processContext = KphGetEProcessContext(processObject);
    if (!processContext)
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      PROTECTION,
                      "KphGetEProcessContext failed");

        status = STATUS_INSUFFICIENT_RESOURCES;
        goto Exit;
    }

    status = KphpStripProtectedProcessMasks(processContext,
                                            ProcessAllowedMask,
                                            ThreadAllowedMask);

Exit:

    if (processContext)
    {
        KphDereferenceObject(processContext);
    }

    if (processObject)
    {
        ObDereferenceObject(processObject);
    }

    return status;
}

/**
 * \brief Initializes the protection infrastructure.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
VOID KphInitializeProtection(
    VOID
    )
{
    KPH_OBJECT_TYPE_INFO typeInfo;

    PAGED_CODE_PASSIVE();

    typeInfo.Allocate = KphpAllocateImageLoadApc;
    typeInfo.Initialize = KphpInitializeImageLoadApc;
    typeInfo.Delete = KphpDeleteImageLoadApc;
    typeInfo.Free = KphpFreeImageLoadApc;

    KphCreateObjectType(&KphpImageLoadApcTypeName,
                        &typeInfo,
                        &KphpImageLoadApcType);
}
