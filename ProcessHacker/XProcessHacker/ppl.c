/*
 * XProcessHacker
 *
 * Copyright (C) 2019 DavidXanatos
 *
 * This file based on the PPL-Killer https://github.com/Mattiwatti/PPLKiller
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <kph.h>
#include "procinfo.h"

#define Log dprintf

 // Exclude false positive matches in the KPROCESS/Pcb header
#ifdef _M_AMD64
#define PS_SEARCH_START				0x600
#else
#define PS_SEARCH_START				0x200
#endif

 // This can be done faster and in fewer lines of code by ghetto-disassembling
 // PsIsProtectedProcess, but this method should hopefully be more robust
NTSTATUS
FindPsProtectionOffset(
    _Out_ PULONG PsProtectionOffset
)
{
    PAGED_CODE();

    *PsProtectionOffset = 0;

    // Since the EPROCESS struct is opaque and we don't know its size, allocate for 4K possible offsets
    const PULONG CandidateOffsets =
        ExAllocatePoolWithTag(NonPagedPoolNx,
            PAGE_SIZE * sizeof(ULONG),
            'LPPK');
    if (CandidateOffsets == NULL)
        return STATUS_NO_MEMORY;
    RtlZeroMemory(CandidateOffsets, sizeof(ULONG) * PAGE_SIZE);

    // Query all running processes
    ULONG NumProtectedProcesses = 0, BestMatchCount = 0, Offset = 0;
    NTSTATUS Status;
    ULONG Size;
    PSYSTEM_PROCESS_INFORMATION SystemProcessInfo = NULL, Entry;
    if ((Status = ZwQuerySystemInformation(SystemProcessInformation,
        SystemProcessInfo,
        0,
        &Size)) != STATUS_INFO_LENGTH_MISMATCH)
        goto finished;
    SystemProcessInfo =
        ExAllocatePoolWithTag(NonPagedPoolNx,
            2 * Size,
            'LPPK');
    if (SystemProcessInfo == NULL)
    {
        Status = STATUS_NO_MEMORY;
        goto finished;
    }
    Status = ZwQuerySystemInformation(SystemProcessInformation,
        SystemProcessInfo,
        2 * Size,
        NULL);
    if (!NT_SUCCESS(Status))
        goto finished;

    // Enumerate the process list
    Entry = SystemProcessInfo;
    for (;;)
    {
        OBJECT_ATTRIBUTES ObjectAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(NULL,
            OBJ_KERNEL_HANDLE);
        CLIENT_ID ClientId = { Entry->UniqueProcessId, NULL };
        HANDLE ProcessHandle;
        Status = ZwOpenProcess(&ProcessHandle,
            PROCESS_QUERY_LIMITED_INFORMATION,
            &ObjectAttributes,
            &ClientId);
        if (NT_SUCCESS(Status))
        {
            // Query the process's protection status
            PS_PROTECTION ProtectionInfo;
            Status = ZwQueryInformationProcess(ProcessHandle,
                ProcessProtectionInformation,
                &ProtectionInfo,
                sizeof(ProtectionInfo),
                NULL);

            // If it's protected (light or otherwise), get the EPROCESS
            if (NT_SUCCESS(Status) && ProtectionInfo.Level > 0)
            {
                PEPROCESS Process;
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    *PsProcessType,
                    KernelMode,
                    &Process,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    // Find offsets in the EPROCESS that are a match for the PS_PROTECTION we got
                    CONST ULONG_PTR End = ALIGN_UP_BY(Process, PAGE_SIZE) - (ULONG_PTR)(Process);
                    for (ULONG_PTR i = PS_SEARCH_START; i < End; ++i)
                    {
                        CONST PPS_PROTECTION Candidate = (PPS_PROTECTION)((PUCHAR)Process + i);
                        if (Candidate->Level == ProtectionInfo.Level)
                            CandidateOffsets[i]++;
                    }
                    NumProtectedProcesses++;
                    ObfDereferenceObject(Process);
                }
            }
            ZwClose(ProcessHandle);
        }

        if (Entry->NextEntryOffset == 0)
            break;

        Entry = (PSYSTEM_PROCESS_INFORMATION)(((ULONG_PTR)Entry) + Entry->NextEntryOffset);
    }

    // Go over the possible offsets to find the one that is correct for all processes
    for (ULONG i = PS_SEARCH_START; i < PAGE_SIZE; ++i)
    {
        if (CandidateOffsets[i] > BestMatchCount)
        {
            if (BestMatchCount == NumProtectedProcesses)
            {
                Log("Found multiple offsets for PS_PROTECTION that match all processes! You should uninstall some rootkits.\n");
                Status = STATUS_NOT_FOUND;
                goto finished;
            }
            Offset = i;
            BestMatchCount = CandidateOffsets[i];
        }
    }

    if (BestMatchCount == 0 && NumProtectedProcesses > 0)
    {
        Log("Did not find any possible offsets for the PS_PROTECTION field.\n");
        Status = STATUS_NOT_FOUND;
        goto finished;
    }

    if (BestMatchCount != NumProtectedProcesses)
    {
        Log("Best found PS_PROTECTION offset match +0x%02X is only valid for %u of %u protected processes.\n",
            Offset, BestMatchCount, NumProtectedProcesses);
        Status = STATUS_NOT_FOUND;
        goto finished;
    }

    if (NumProtectedProcesses > 1) // Require at least System + 1 PPL to give a reliable result
        Log("Found PS_PROTECTION offset +0x%02X.\n", Offset);
    else
    {
        // This is not an error condition; it just means there are no processes to unprotect.
        // There may still be processes with signature requirements to remove. Set a non-error status to indicate this.
        Log("Did not find any non-system protected processes.\n");
        Status = STATUS_NO_MORE_ENTRIES;
        Offset = 0;
    }

    *PsProtectionOffset = Offset;

finished:
    if (SystemProcessInfo != NULL)
        ExFreePoolWithTag(SystemProcessInfo, 'LPPK');
    ExFreePoolWithTag(CandidateOffsets, 'LPPK');
    return Status;
}

// This is only called on Windows >= 10.0.15063.0. The 'MS signature required' mitigation
// policy predates that, but the kernel mode check in MiValidateSectionCreate does not
NTSTATUS
FindSignatureLevelOffsets(
    _Out_ PULONG SignatureLevelOffset,
    _Out_ PULONG SectionSignatureLevelOffset
)
{
    PAGED_CODE();

    *SignatureLevelOffset = 0;
    *SectionSignatureLevelOffset = 0;

    // Since the EPROCESS struct is opaque and we don't know its size, allocate for 4K possible offsets
    const PULONG CandidateSignatureLevelOffsets =
        ExAllocatePoolWithTag(NonPagedPoolNx,
            PAGE_SIZE * sizeof(ULONG),
            'LPPK');
    if (CandidateSignatureLevelOffsets == NULL)
        return STATUS_NO_MEMORY;
    const PULONG CandidateSectionSignatureLevelOffsets =
        ExAllocatePoolWithTag(NonPagedPoolNx,
            PAGE_SIZE * sizeof(ULONG),
            'LPPK');
    if (CandidateSectionSignatureLevelOffsets == NULL)
    {
        ExFreePoolWithTag(CandidateSignatureLevelOffsets, 'LPPK');
        return STATUS_NO_MEMORY;
    }
    RtlZeroMemory(CandidateSignatureLevelOffsets, sizeof(ULONG) * PAGE_SIZE);
    RtlZeroMemory(CandidateSectionSignatureLevelOffsets, sizeof(ULONG) * PAGE_SIZE);

    // Query all running processes
    ULONG NumSignatureRequiredProcesses = 0, BestMatchCount = 0;
    ULONG SignatureOffset = 0, SectionSignatureOffset = 0;
    NTSTATUS Status;
    ULONG Size;
    PSYSTEM_PROCESS_INFORMATION SystemProcessInfo = NULL, Entry;
    if ((Status = ZwQuerySystemInformation(SystemProcessInformation,
        SystemProcessInfo,
        0,
        &Size)) != STATUS_INFO_LENGTH_MISMATCH)
        goto finished;
    SystemProcessInfo =
        ExAllocatePoolWithTag(NonPagedPoolNx,
            2 * Size,
            'LPPK');
    if (SystemProcessInfo == NULL)
    {
        Status = STATUS_NO_MEMORY;
        goto finished;
    }
    Status = ZwQuerySystemInformation(SystemProcessInformation,
        SystemProcessInfo,
        2 * Size,
        NULL);
    if (!NT_SUCCESS(Status))
        goto finished;

    // Enumerate the process list
    Entry = SystemProcessInfo;
    for (;;)
    {
        OBJECT_ATTRIBUTES ObjectAttributes = RTL_CONSTANT_OBJECT_ATTRIBUTES(NULL,
            OBJ_KERNEL_HANDLE);
        CLIENT_ID ClientId = { Entry->UniqueProcessId, NULL };
        HANDLE ProcessHandle;
        Status = ZwOpenProcess(&ProcessHandle,
            PROCESS_QUERY_LIMITED_INFORMATION,
            &ObjectAttributes,
            &ClientId);
        if (NT_SUCCESS(Status))
        {
            // Query the process's signature policy status
            PROCESS_MITIGATION_POLICY_INFORMATION PolicyInfo;
            PolicyInfo.Policy = ProcessSignaturePolicy;
            Status = ZwQueryInformationProcess(ProcessHandle,
                ProcessMitigationPolicy,
                &PolicyInfo,
                sizeof(PolicyInfo),
                NULL);

            // If it has an MS signature policy requirement, get the EPROCESS
            if (NT_SUCCESS(Status) && PolicyInfo.u.SignaturePolicy.MicrosoftSignedOnly != 0)
            {
                PEPROCESS Process;
                Status = ObReferenceObjectByHandle(ProcessHandle,
                    PROCESS_QUERY_LIMITED_INFORMATION,
                    *PsProcessType,
                    KernelMode,
                    &Process,
                    NULL);
                if (NT_SUCCESS(Status))
                {
                    // Find plausible offsets in the EPROCESS
                    const ULONG_PTR End = ALIGN_UP_BY(Process, PAGE_SIZE) - (ULONG_PTR)(Process)-sizeof(UCHAR);
                    for (ULONG_PTR i = PS_SEARCH_START; i < End; ++i)
                    {
                        // Take the low nibble of both bytes, which contains the SE_SIGNING_LEVEL_*
                        const UCHAR CandidateSignatureLevel = *((PUCHAR)(Process)+i) & 0xF;
                        const ULONG CandidateSectionSignatureLevel = *((PUCHAR)(Process)+i + sizeof(UCHAR)) & 0xF;

                        if ((CandidateSignatureLevel == SE_SIGNING_LEVEL_MICROSOFT ||
                            CandidateSignatureLevel == SE_SIGNING_LEVEL_WINDOWS ||
                            CandidateSignatureLevel == SE_SIGNING_LEVEL_ANTIMALWARE ||
                            CandidateSignatureLevel == SE_SIGNING_LEVEL_WINDOWS_TCB)
                            &&
                            (CandidateSectionSignatureLevel == SE_SIGNING_LEVEL_MICROSOFT ||
                                CandidateSectionSignatureLevel == SE_SIGNING_LEVEL_WINDOWS))
                        {
                            CandidateSignatureLevelOffsets[i]++;
                            i += sizeof(UCHAR);
                            CandidateSectionSignatureLevelOffsets[i]++;
                        }
                    }
                    NumSignatureRequiredProcesses++;
                    ObfDereferenceObject(Process);
                }
            }
            ZwClose(ProcessHandle);
        }

        if (Entry->NextEntryOffset == 0)
            break;

        Entry = (PSYSTEM_PROCESS_INFORMATION)(((ULONG_PTR)Entry) + Entry->NextEntryOffset);
    }

    // Go over the possible offsets to find the combination that is correct for all processes
    for (ULONG i = PS_SEARCH_START; i < PAGE_SIZE; ++i)
    {
        if (CandidateSignatureLevelOffsets[i] > BestMatchCount)
        {
            if (BestMatchCount == NumSignatureRequiredProcesses)
            {
                Log("Found multiple offsets for SignatureLevel that match all processes! This is probably a bug - please report.\n");
                Status = STATUS_NOT_FOUND;
                goto finished;
            }
            SignatureOffset = i;
            SectionSignatureOffset = i + sizeof(UCHAR);
            BestMatchCount = CandidateSignatureLevelOffsets[i];
        }
    }

    if (BestMatchCount == 0 && NumSignatureRequiredProcesses > 0)
    {
        Log("Did not find any possible offsets for the SignatureLevel field.\n");
        Status = STATUS_NOT_FOUND;
        goto finished;
    }

    if (BestMatchCount != NumSignatureRequiredProcesses)
    {
        Log("Best found SignatureLevel offset match +0x%02X is only valid for %u of %u processes.\n",
            SignatureOffset, BestMatchCount, NumSignatureRequiredProcesses);
        Status = STATUS_NOT_FOUND;
        goto finished;
    }

    if (NumSignatureRequiredProcesses > 1) // Require at least System + 1 other MS signing policy process to give a reliable result
        Log("Found SignatureLevel offset +0x%02X and SectionSignatureLevel offset +0x%02X.\n\n",
            SignatureOffset, SectionSignatureOffset);
    else
    {
        // This is not an error condition; it just means there are no processes with MS code signing requirements.
        // There may still be PPLs to kill. Set a non-error status to indicate this.
        Log("Did not find any non-system processes with signature requirements.\n");
        Status = STATUS_NO_MORE_ENTRIES;
        SignatureOffset = 0;
        SectionSignatureOffset = 0;
    }
    *SignatureLevelOffset = SignatureOffset;
    *SectionSignatureLevelOffset = SectionSignatureOffset;

finished:
    if (SystemProcessInfo != NULL)
        ExFreePoolWithTag(SystemProcessInfo, 'LPPK');
    ExFreePoolWithTag(CandidateSectionSignatureLevelOffsets, 'LPPK');
    ExFreePoolWithTag(CandidateSignatureLevelOffsets, 'LPPK');
    return Status;
}

CHAR* SeSigningLevelNames[] =
{
    "SE_SIGNING_LEVEL_UNCHECKED",		// 0x0
    "SE_SIGNING_LEVEL_UNSIGNED",
    "SE_SIGNING_LEVEL_ENTERPRISE",
    "SE_SIGNING_LEVEL_CUSTOM_1",
    "SE_SIGNING_LEVEL_AUTHENTICODE",
    "SE_SIGNING_LEVEL_CUSTOM_2",
    "SE_SIGNING_LEVEL_STORE",
    "SE_SIGNING_LEVEL_ANTIMALWARE",
    "SE_SIGNING_LEVEL_MICROSOFT",
    "SE_SIGNING_LEVEL_CUSTOM_4",
    "SE_SIGNING_LEVEL_CUSTOM_5",
    "SE_SIGNING_LEVEL_DYNAMIC_CODEGEN",
    "SE_SIGNING_LEVEL_WINDOWS",
    "SE_SIGNING_LEVEL_CUSTOM_7",
    "SE_SIGNING_LEVEL_WINDOWS_TCB",
    "SE_SIGNING_LEVEL_CUSTOM_6",		// 0xf
};

CHAR* SeSigningTypeNames[] =
{
    "SeImageSignatureNone",				// 0x0
    "SeImageSignatureEmbedded",
    "SeImageSignatureCache",
    "SeImageSignatureCatalogCached",
    "SeImageSignatureCatalogNotCached",
    "SeImageSignatureCatalogHint",
    "SeImageSignaturePackageCatalog",	// 0x6

    // Make sure it isn't possible to overrun the array bounds using 3 index bits
    "<INVALID>"							// 0x7
};

NTSTATUS
SetProcessesProtectionFlag(
    _In_opt_ ULONG PsProtectionOffset,
    _In_opt_ ULONG SignatureLevelOffset,
    _In_opt_ ULONG SectionSignatureLevelOffset,
    HANDLE UniqueProcessId,
    UCHAR Flag
)
{
    NTSTATUS Status;
    PEPROCESS Process;

    const ULONG Pid = HandleToULong(UniqueProcessId);

    Status = PsLookupProcessByProcessId(UniqueProcessId,
        &Process);

    if (NT_SUCCESS(Status))
    {
        if (PsProtectionOffset != 0) // Do we have any PPLs to unprotect?
        {
            const PPS_PROTECTION PsProtection = (PPS_PROTECTION)((PUCHAR)(Process) + PsProtectionOffset);

            // Skip non-light protected processes (i.e. System).
            // You could also discriminate by signer, e.g. to leave LSASS or antimalware protection enabled
            // if (PsProtection->Level != 0 && PsProtection->s.Type == PsProtectedTypeProtectedLight)
            if (PsProtection->Level != Flag)
            {
                Log("PID %u at 0x%p is a PPL: { type: %u, audit: %u, signer: %u }.\n", Pid,
                    Process, PsProtection->s.Type, PsProtection->s.Audit, PsProtection->s.Signer);

                PsProtection->Level = Flag;

                if(Flag == 0)
                    Log("Protection removed.\n");
                else
                    Log("PID %u at 0x%p set to be a PPL: { type: %u, audit: %u, signer: %u }.\n", Pid,
                        Process, PsProtection->s.Type, PsProtection->s.Audit, PsProtection->s.Signer);
            }
        }

        if (Pid != 0 && !PsIsSystemProcess(Process) &&						// Not a system process?
            SignatureLevelOffset != 0 && SectionSignatureLevelOffset != 0)	// >= Windows 10 RS2, and offsets known?
        {
            const PUCHAR SignatureLevelByte = (PUCHAR)(Process) + SignatureLevelOffset;
            const PUCHAR SectionSignatureLevelByte = (PUCHAR)(Process) + SectionSignatureLevelOffset;
            const UCHAR SignatureLevel = *SignatureLevelByte & 0xF;
            const UCHAR ImageSignatureType = (*SignatureLevelByte >> 4) & 0x7;
            const UCHAR SectionSignatureLevel = *SectionSignatureLevelByte & 0xF;

            if ((SignatureLevel == SE_SIGNING_LEVEL_MICROSOFT ||
                SignatureLevel == SE_SIGNING_LEVEL_WINDOWS ||
                SignatureLevel == SE_SIGNING_LEVEL_ANTIMALWARE ||
                SignatureLevel == SE_SIGNING_LEVEL_WINDOWS_TCB)
                &&
                (SectionSignatureLevel == SE_SIGNING_LEVEL_MICROSOFT ||
                    SectionSignatureLevel == SE_SIGNING_LEVEL_WINDOWS))
            {
                Log("PID %u at 0x%p has a Microsoft code signing requirement:\n", Pid, Process);

                // NB: the SE_IMAGE_SIGNATURE_TYPE can be 'none' while still having an MS code signing policy, so this isn't a reliable indicator.
                // Normally though it will either be SeImageSignatureEmbedded (system process) or SeImageSignatureCatalogCached (other processes).
                Log("Image signature level:\t0x%02X [%s], type: 0x%02X [%s]\n",
                    SignatureLevel, SeSigningLevelNames[SignatureLevel],
                    ImageSignatureType, SeSigningTypeNames[ImageSignatureType]);
                Log("Section signature level:\t0x%02X [%s]\n",
                    SectionSignatureLevel, SeSigningLevelNames[SectionSignatureLevel]);

                *SignatureLevelByte = 0;
                *SectionSignatureLevelByte = 0;
                Log("Requirements removed.\n\n");
            }
        }

        ObfDereferenceObject(Process);
    }
    else
    {
        Log("Process with PID %u not found:\n", Pid);
    }

    return Status;
}
