/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     jxy-s   2022-2026
 *
 */

#include <kph.h>

#include <trace.h>

typedef struct _KPH_KNOWN_DLL_EXPORT
{
    PCHAR Name;
    PVOID* Storage;
} KPH_KNOWN_DLL_EXPORT, *PKPH_KNOWN_DLL_EXPORT;

typedef struct _KPH_KNOWN_DLL_INFORMATION
{
    UNICODE_STRING SectionName;
    PVOID* BaseAddressStorage;
    PKPH_KNOWN_DLL_EXPORT Exports;
} KPH_KNOWN_DLL_INFORMATION, *PKPH_KNOWN_DLL_INFORMATION;

KPH_PROTECTED_DATA_SECTION_PUSH();
PVOID KphNtDllBaseAddress = NULL;
PVOID KphNtDllRtlSetBits = NULL;
static KPH_KNOWN_DLL_EXPORT KphpNtDllExports[] =
{
    { "RtlSetBits", &KphNtDllRtlSetBits },
    { NULL, NULL }
};
static KPH_KNOWN_DLL_INFORMATION KphpKnownDllInformation[] =
{
    {
        RTL_CONSTANT_STRING(L"\\KnownDlls\\ntdll.dll"),
        &KphNtDllBaseAddress,
        KphpNtDllExports
    }
};
KPH_PROTECTED_DATA_SECTION_POP();

KPH_PAGED_FILE();

/**
 * \brief Populates known DLL information.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
NTSTATUS KphInitializeKnownDll(
    VOID
    )
{
    NTSTATUS status;
    HANDLE sectionHandle;
    PVOID sectionObject;
    PVOID baseAddress;
    SIZE_T viewSize;

    KPH_PAGED_CODE_PASSIVE();

    sectionHandle = NULL;
    sectionObject = NULL;
    baseAddress = NULL;

    for (ULONG i = 0; i < ARRAYSIZE(KphpKnownDllInformation); i++)
    {
        PKPH_KNOWN_DLL_INFORMATION info;
        OBJECT_ATTRIBUTES objectAttributes;
        SECTION_IMAGE_INFORMATION sectionImageInfo;

        if (baseAddress)
        {
            MmUnmapViewInSystemSpace(baseAddress);
            baseAddress = NULL;
        }

        if (sectionObject)
        {
            ObDereferenceObject(sectionObject);
            sectionObject = NULL;
        }

        if (sectionHandle)
        {
            ObCloseHandle(sectionHandle, KernelMode);
            sectionHandle = NULL;
        }

        info = &KphpKnownDllInformation[i];

        NT_ASSERT(info->BaseAddressStorage);

        InitializeObjectAttributes(&objectAttributes,
                                   &info->SectionName,
                                   OBJ_KERNEL_HANDLE,
                                   NULL,
                                   NULL);

        status = ZwOpenSection(&sectionHandle,
                               SECTION_MAP_READ | SECTION_QUERY,
                               &objectAttributes);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          GENERAL,
                          "ZwOpenSection failed: %!STATUS!",
                          status);


            sectionHandle = NULL;
            goto Exit;
        }

        status = ZwQuerySection(sectionHandle,
                                SectionImageInformation,
                                &sectionImageInfo,
                                sizeof(SECTION_IMAGE_INFORMATION),
                                NULL);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          GENERAL,
                          "ZwQuerySection failed: %!STATUS!",
                          status);

            goto Exit;
        }

        *info->BaseAddressStorage = sectionImageInfo.TransferAddress;

        if (!info->Exports)
        {
            continue;
        }

        status = ObReferenceObjectByHandle(sectionHandle,
                                           SECTION_MAP_READ | SECTION_QUERY,
                                           *MmSectionObjectType,
                                           KernelMode,
                                           &sectionObject,
                                           NULL);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          GENERAL,
                          "ObReferenceObjectByHandle failed: %!STATUS!",
                          status);

            sectionObject = NULL;
            goto Exit;
        }

        viewSize = 0;
        status = MmMapViewInSystemSpace(sectionObject,
                                        &baseAddress,
                                        &viewSize);
        if (!NT_SUCCESS(status))
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          GENERAL,
                          "MmMapViewInSystemSpace failed: %!STATUS!",
                          status);

            baseAddress = NULL;
            goto Exit;
        }

        for (PKPH_KNOWN_DLL_EXPORT export = info->Exports;
             export->Name != NULL;
             export = export + 1)
        {
            PVOID exportAddress;

            NT_ASSERT(export->Storage);

            exportAddress = RtlFindExportedRoutineByName(baseAddress,
                                                         export->Name);
            if (!exportAddress)
            {
                KphTracePrint(TRACE_LEVEL_VERBOSE,
                              GENERAL,
                              "Failed to find %hs in %wZ",
                              export->Name,
                              &info->SectionName);

                status = STATUS_NOT_FOUND;
                goto Exit;
            }

            *export->Storage = Add2Ptr(sectionImageInfo.TransferAddress,
                                       PtrOffset(baseAddress, exportAddress));
        }
    }

    status = STATUS_SUCCESS;

Exit:

    if (baseAddress)
    {
        MmUnmapViewInSystemSpace(baseAddress);
    }

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

#ifdef IS_KTE
/**
 * \brief Resolves RtlSetBits from a process's loaded ntdll.dll.
 *
 * \param[in] Process The process to resolve RtlSetBits from.
 * \param[out] RtlSetBitsAddress Receives the address of RtlSetBits in the process.
 *
 * \return Successful or errant status.
 */
_IRQL_requires_max_(PASSIVE_LEVEL)
_Must_inspect_result_
NTSTATUS KphGetProcessNtDllRtlSetBits(
    _In_ PEPROCESS Process,
    _Out_ PVOID* RtlSetBitsAddress
    )
{
    NTSTATUS status;
    HANDLE processHandle;
    PROCESS_BASIC_INFORMATION basicInfo;
    PVOID peb;
    PVOID ldr;
    PVOID moduleListHead;
    PVOID currentEntry;
    KAPC_STATE apcState;
    BOOLEAN attached;
    PVOID ntdllBase;
    PVOID exportAddress;

    KPH_PAGED_CODE_PASSIVE();

    processHandle = NULL;
    attached = FALSE;
    *RtlSetBitsAddress = NULL;

    //
    // Open process handle for querying
    //
    status = ObOpenObjectByPointer(Process,
                                   OBJ_KERNEL_HANDLE,
                                   NULL,
                                   PROCESS_QUERY_INFORMATION,
                                   *PsProcessType,
                                   KernelMode,
                                   &processHandle);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "ObOpenObjectByPointer failed: %!STATUS!",
                      status);
        goto Exit;
    }

    //
    // Get the PEB address
    //
    status = ZwQueryInformationProcess(processHandle,
                                       ProcessBasicInformation,
                                       &basicInfo,
                                       sizeof(basicInfo),
                                       NULL);
    if (!NT_SUCCESS(status))
    {
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "ZwQueryInformationProcess failed: %!STATUS!",
                      status);
        goto Exit;
    }

    peb = basicInfo.PebBaseAddress;
    if (!peb)
    {
        status = STATUS_UNSUCCESSFUL;
        goto Exit;
    }

    //
    // Attach to the process to read its memory
    //
    KeStackAttachProcess(Process, &apcState);
    attached = TRUE;

    __try
    {
        //
        // Determine if the target process is 32-bit or 64-bit
        // We check if WoW64 PEB exists to determine architecture
        //
        PVOID wow64Peb;
        BOOLEAN is32BitProcess;
        ULONG pebLdrOffset;
        ULONG ldrInLoadOrderOffset;
        ULONG ldrDllBaseOffset;
        ULONG ldrBaseDllNameOffset;

        wow64Peb = PsGetProcessWow64Process(Process);
        is32BitProcess = (wow64Peb != NULL);

        //
        // Set offsets based on target process architecture
        //
        if (is32BitProcess)
        {
            //
            // Target is 32-bit (x86)
            // PEB.Ldr is at offset 0x0C
            // PEB_LDR_DATA.InLoadOrderModuleList is at offset 0x0C
            // LDR_DATA_TABLE_ENTRY.DllBase is at offset 0x18
            // LDR_DATA_TABLE_ENTRY.BaseDllName is at offset 0x2C
            //
            pebLdrOffset = 0x0C;
            ldrInLoadOrderOffset = 0x0C;
            ldrDllBaseOffset = 0x18;
            ldrBaseDllNameOffset = 0x2C;

            //
            // For 32-bit processes, use the WoW64 PEB if available
            //
            if (wow64Peb)
            {
                peb = wow64Peb;
            }
        }
        else
        {
            //
            // Target is 64-bit (x64 or ARM64)
            // PEB.Ldr is at offset 0x18
            // PEB_LDR_DATA.InLoadOrderModuleList is at offset 0x10
            // LDR_DATA_TABLE_ENTRY.DllBase is at offset 0x30
            // LDR_DATA_TABLE_ENTRY.BaseDllName is at offset 0x58
            //
            pebLdrOffset = 0x18;
            ldrInLoadOrderOffset = 0x10;
            ldrDllBaseOffset = 0x30;
            ldrBaseDllNameOffset = 0x58;
        }

        //
        // Read Ldr pointer from PEB
        //
        ldr = *(PVOID*)((ULONG_PTR)peb + pebLdrOffset);
        if (!ldr)
        {
            status = STATUS_UNSUCCESSFUL;
            __leave;
        }

        //
        // Get the InLoadOrderModuleList head
        //
        moduleListHead = (PVOID)((ULONG_PTR)ldr + ldrInLoadOrderOffset);
        currentEntry = *(PVOID*)moduleListHead;

        //
        // Walk the module list to find ntdll.dll
        // Each entry is an LDR_DATA_TABLE_ENTRY
        //
        ntdllBase = NULL;
        while (currentEntry != moduleListHead)
        {
            UNICODE_STRING baseDllName;
            PVOID dllBase;

            //
            // Read DllBase and BaseDllName from LDR_DATA_TABLE_ENTRY
            //
            dllBase = *(PVOID*)((ULONG_PTR)currentEntry + ldrDllBaseOffset);
            baseDllName = *(UNICODE_STRING*)((ULONG_PTR)currentEntry + ldrBaseDllNameOffset);

            if (baseDllName.Buffer && baseDllName.Length >= 9 * sizeof(WCHAR))
            {
                //
                // Check if this is ntdll.dll (case insensitive)
                //
                if ((_wcsnicmp(baseDllName.Buffer, L"ntdll.dll", 9) == 0))
                {
                    ntdllBase = dllBase;
                    break;
                }
            }

            //
            // Move to next entry (Flink is at offset 0)
            //
            currentEntry = *(PVOID*)currentEntry;

            //
            // Prevent infinite loops
            //
            if (currentEntry == moduleListHead)
                break;
        }

        if (!ntdllBase)
        {
            status = STATUS_NOT_FOUND;
            __leave;
        }

        //
        // Find RtlSetBits export
        //
        exportAddress = RtlFindExportedRoutineByName(ntdllBase, "RtlSetBits");
        if (!exportAddress)
        {
            KphTracePrint(TRACE_LEVEL_VERBOSE,
                          GENERAL,
                          "RtlFindExportedRoutineByName failed to find RtlSetBits");
            status = STATUS_NOT_FOUND;
            __leave;
        }

        *RtlSetBitsAddress = exportAddress;
        status = STATUS_SUCCESS;
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        status = GetExceptionCode();
        KphTracePrint(TRACE_LEVEL_VERBOSE,
                      GENERAL,
                      "Exception while reading process memory: %!STATUS!",
                      status);
    }

Exit:

    if (attached)
    {
        KeUnstackDetachProcess(&apcState);
    }

    if (processHandle)
    {
        ObCloseHandle(processHandle, KernelMode);
    }

    return status;
}
#endif
