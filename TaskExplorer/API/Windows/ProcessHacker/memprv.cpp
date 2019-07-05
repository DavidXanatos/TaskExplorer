/*
 * Process Hacker -
 *   qt port of memory provider
 *
 * Copyright (C) 2010-2015 wj32
 * Copyright (C) 2017-2018 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Process Hacker.
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

#include "stdafx.h"
#include "../ProcessHacker.h"
#include "../WinMemory.h"
#include "memprv.h"

#define MAX_HEAPS 1000
#define WS_REQUEST_COUNT (PAGE_SIZE / sizeof(MEMORY_WORKING_SET_EX_INFORMATION))

QMap<quint64, CMemoryPtr>::iterator PhLookupMemoryItemList(
    QMap<quint64, CMemoryPtr>& MemoryMap,
    _In_ PVOID Address
    )
{
    // Do an approximate search on the set to locate the memory item with the largest
    // base address that is still smaller than the given address.
	QMap<quint64, CMemoryPtr>::iterator I = MemoryMap.upperBound((quint64)Address);
    
    if (I != MemoryMap.end())
    {
		QSharedPointer<CWinMemory> memoryItem = I.value().objectCast<CWinMemory>();

        if ((ULONG_PTR)Address < (ULONG_PTR)memoryItem->m_BaseAddress + memoryItem->m_RegionSize)
            return I;
    }

    return MemoryMap.end();
}

QSharedPointer<CWinMemory> PhpSetMemoryRegionType(
    QMap<quint64, CMemoryPtr>& MemoryMap,
    _In_ PVOID Address,
    _In_ BOOLEAN GoToAllocationBase,
    _In_ int RegionType
    )
{
    QMap<quint64, CMemoryPtr>::iterator I = PhLookupMemoryItemList(MemoryMap, Address);

    if (I == MemoryMap.end())
        return NULL;

	QSharedPointer<CWinMemory> memoryItem = I.value().objectCast<CWinMemory>();

    if (GoToAllocationBase && memoryItem->m_AllocationBaseItem)
        memoryItem = memoryItem->m_AllocationBaseItem.objectCast<CWinMemory>();

    if (memoryItem->m_RegionType != UnknownRegion)
        return NULL;

    memoryItem->m_RegionType = RegionType;

    return memoryItem;
}

NTSTATUS PhpUpdateMemoryRegionTypes(
    QMap<quint64, CMemoryPtr>& MemoryMap,
	_In_ HANDLE ProcessId,
    _In_ HANDLE ProcessHandle
    )
{
    NTSTATUS status;
    PVOID processes;
    PSYSTEM_PROCESS_INFORMATION process;
    ULONG i;
#ifdef _WIN64
    BOOLEAN isWow64 = FALSE;
#endif
    QSharedPointer<CWinMemory> memoryItem;
    PLIST_ENTRY listEntry;

    if (!NT_SUCCESS(status = PhEnumProcessesEx(&processes, SystemExtendedProcessInformation)))
        return status;

    process = PhFindProcessInformation(processes, ProcessId);

    if (!process)
    {
        PhFree(processes);
        return STATUS_NOT_FOUND;
    }

    // USER_SHARED_DATA
    PhpSetMemoryRegionType(MemoryMap, USER_SHARED_DATA, TRUE, UserSharedDataRegion);

    // HYPERVISOR_SHARED_DATA
    if (WindowsVersion >= WINDOWS_10_RS4)
    {
        static PVOID HypervisorSharedDataVa = NULL;
        static PH_INITONCE HypervisorSharedDataInitOnce = PH_INITONCE_INIT;

        if (PhBeginInitOnce(&HypervisorSharedDataInitOnce))
        {
            SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION hypervSharedPageInfo;

            if (NT_SUCCESS(NtQuerySystemInformation(
                SystemHypervisorSharedPageInformation,
                &hypervSharedPageInfo,
                sizeof(SYSTEM_HYPERVISOR_SHARED_PAGE_INFORMATION),
                NULL
                )))
            {
                HypervisorSharedDataVa = hypervSharedPageInfo.HypervisorSharedUserVa;
            }

            PhEndInitOnce(&HypervisorSharedDataInitOnce);
        }

        if (HypervisorSharedDataVa)
        {
            PhpSetMemoryRegionType(MemoryMap, HypervisorSharedDataVa, TRUE, HypervisorSharedDataRegion);
        }
    }

    // PEB, heap
    {
        PROCESS_BASIC_INFORMATION basicInfo;
        ULONG numberOfHeaps;
        PVOID processHeapsPtr;
        PVOID *processHeaps;
        PVOID apiSetMap;
        ULONG i;
#ifdef _WIN64
        PVOID peb32;
        ULONG processHeapsPtr32;
        ULONG *processHeaps32;
        ULONG apiSetMap32;
#endif

        if (NT_SUCCESS(PhGetProcessBasicInformation(ProcessHandle, &basicInfo)) && basicInfo.PebBaseAddress != 0)
        {
            // HACK: Windows 10 RS2 and above 'added TEB/PEB sub-VAD segments' and we need to tag individual sections.
            PhpSetMemoryRegionType(MemoryMap, basicInfo.PebBaseAddress, WindowsVersion < WINDOWS_10_RS2 ? TRUE : FALSE, PebRegion);

            if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, NumberOfHeaps)),
                &numberOfHeaps, sizeof(ULONG), NULL)) && numberOfHeaps < MAX_HEAPS)
            {
                processHeaps = (PVOID *)PhAllocate(numberOfHeaps * sizeof(PVOID));

                if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                    PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, ProcessHeaps)),
                    &processHeapsPtr, sizeof(PVOID), NULL)) &&
                    NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                    processHeapsPtr,
                    processHeaps, numberOfHeaps * sizeof(PVOID), NULL)))
                {
                    for (i = 0; i < numberOfHeaps; i++)
                    {
                        if (memoryItem = PhpSetMemoryRegionType(MemoryMap, processHeaps[i], TRUE, HeapRegion))
                            memoryItem->u.Heap.Index = i;
                    }
                }

                PhFree(processHeaps);
            }

            // ApiSet schema map
            if (NT_SUCCESS(NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, ApiSetMap)),
                &apiSetMap,
                sizeof(PVOID),
                NULL
                )))
            {
                PhpSetMemoryRegionType(MemoryMap, apiSetMap, TRUE, ApiSetMapRegion);
            }
        }
#ifdef _WIN64

        if (NT_SUCCESS(PhGetProcessPeb32(ProcessHandle, &peb32)) && peb32 != 0)
        {
            isWow64 = TRUE;
            PhpSetMemoryRegionType(MemoryMap, peb32, TRUE, Peb32Region);

            if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, NumberOfHeaps)),
                &numberOfHeaps, sizeof(ULONG), NULL)) && numberOfHeaps < MAX_HEAPS)
            {
                processHeaps32 = (ULONG *)PhAllocate(numberOfHeaps * sizeof(ULONG));

                if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                    PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, ProcessHeaps)),
                    &processHeapsPtr32, sizeof(ULONG), NULL)) &&
                    NT_SUCCESS(NtReadVirtualMemory(ProcessHandle,
                    UlongToPtr(processHeapsPtr32),
                    processHeaps32, numberOfHeaps * sizeof(ULONG), NULL)))
                {
                    for (i = 0; i < numberOfHeaps; i++)
                    {
                        if (memoryItem = PhpSetMemoryRegionType(MemoryMap, UlongToPtr(processHeaps32[i]), TRUE, Heap32Region))
                            memoryItem->u.Heap.Index = i;
                    }
                }

                PhFree(processHeaps32);
            }

            // ApiSet schema map
            if (NT_SUCCESS(NtReadVirtualMemory(
                ProcessHandle,
                PTR_ADD_OFFSET(peb32, FIELD_OFFSET(PEB32, ApiSetMap)),
                &apiSetMap32,
                sizeof(ULONG),
                NULL
                )))
            {
                PhpSetMemoryRegionType(MemoryMap, UlongToPtr(apiSetMap32), TRUE, ApiSetMapRegion);
            }
        }
#endif
    }

    // TEB, stack
    for (i = 0; i < process->NumberOfThreads; i++)
    {
        PSYSTEM_EXTENDED_THREAD_INFORMATION thread = (PSYSTEM_EXTENDED_THREAD_INFORMATION)process->Threads + i;

        if (thread->TebBase)
        {
            NT_TIB ntTib;
            SIZE_T bytesRead;

            // HACK: Windows 10 RS2 and above 'added TEB/PEB sub-VAD segments' and we need to tag individual sections.
            if (memoryItem = PhpSetMemoryRegionType(MemoryMap, thread->TebBase, WindowsVersion < WINDOWS_10_RS2 ? TRUE : FALSE, TebRegion))
                memoryItem->u.Teb.ThreadId = thread->ThreadInfo.ClientId.UniqueThread;

            if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, thread->TebBase, &ntTib, sizeof(NT_TIB), &bytesRead)) &&
                bytesRead == sizeof(NT_TIB))
            {
                if ((ULONG_PTR)ntTib.StackLimit < (ULONG_PTR)ntTib.StackBase)
                {
                    if (memoryItem = PhpSetMemoryRegionType(MemoryMap, ntTib.StackLimit, TRUE, StackRegion))
                        memoryItem->u.Stack.ThreadId = thread->ThreadInfo.ClientId.UniqueThread;
                }
#ifdef _WIN64

                if (isWow64 && ntTib.ExceptionList)
                {
                    ULONG teb32 = PtrToUlong(ntTib.ExceptionList);
                    NT_TIB32 ntTib32;

                    // 64-bit and 32-bit TEBs usually share the same memory region, so don't do anything for the 32-bit
                    // TEB.

                    if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, UlongToPtr(teb32), &ntTib32, sizeof(NT_TIB32), &bytesRead)) &&
                        bytesRead == sizeof(NT_TIB32))
                    {
                        if (ntTib32.StackLimit < ntTib32.StackBase)
                        {
                            if (memoryItem = PhpSetMemoryRegionType(MemoryMap, UlongToPtr(ntTib32.StackLimit), TRUE, Stack32Region))
                                memoryItem->u.Stack.ThreadId = thread->ThreadInfo.ClientId.UniqueThread;
                        }
                    }
                }
#endif
            }
        }
    }

    // Mapped file, heap segment, unusable
    for(QMap<quint64, CMemoryPtr>::iterator I = MemoryMap.begin(); I != MemoryMap.end(); ++I)
    {
		memoryItem = I.value().objectCast<CWinMemory>();

        if (memoryItem->m_RegionType != UnknownRegion)
            continue;

        if ((memoryItem->m_Type & (MEM_MAPPED | MEM_IMAGE)) && memoryItem->m_AllocationBaseItem == memoryItem)
        {
            PPH_STRING fileName;

            if (NT_SUCCESS(PhGetProcessMappedFileName(ProcessHandle, (PVOID)memoryItem->m_BaseAddress, &fileName)))
            {
                PPH_STRING newFileName = PhResolveDevicePrefix(fileName);

                if (newFileName)
                    PhMoveReference((PVOID*)&fileName, newFileName);

                memoryItem->m_RegionType = MappedFileRegion;
                memoryItem->u_Custom_Text = CastPhString(fileName);
                continue;
            }
        }

        if (memoryItem->m_State & MEM_COMMIT)
        {
            UCHAR buffer[HEAP_SEGMENT_MAX_SIZE];

            if (NT_SUCCESS(NtReadVirtualMemory(ProcessHandle, (PVOID)memoryItem->m_BaseAddress,
                buffer, sizeof(buffer), NULL)))
            {
                PVOID candidateHeap = NULL;
                ULONG candidateHeap32 = 0;
                QSharedPointer<CWinMemory> heapMemoryItem;
                PHEAP_SEGMENT heapSegment = (PHEAP_SEGMENT)buffer;
                PHEAP_SEGMENT32 heapSegment32 = (PHEAP_SEGMENT32)buffer;

                if (heapSegment->SegmentSignature == HEAP_SEGMENT_SIGNATURE)
                    candidateHeap = heapSegment->Heap;
                if (heapSegment32->SegmentSignature == HEAP_SEGMENT_SIGNATURE)
                    candidateHeap32 = heapSegment32->Heap;

                if (candidateHeap)
                {
                    QMap<quint64, CMemoryPtr>::iterator I = PhLookupMemoryItemList(MemoryMap, candidateHeap);

					if (I != MemoryMap.end())
					{
						heapMemoryItem = I.value().objectCast<CWinMemory>();
						if (heapMemoryItem->m_BaseAddress == (quint64)candidateHeap &&
							heapMemoryItem->m_RegionType == HeapRegion)
						{
							memoryItem->m_RegionType = HeapSegmentRegion;
							memoryItem->u_HeapSegment_HeapItem = heapMemoryItem;
							continue;
						}
					}
                }
                else if (candidateHeap32)
                {
                    QMap<quint64, CMemoryPtr>::iterator I = PhLookupMemoryItemList(MemoryMap, UlongToPtr(candidateHeap32));

					if (I != MemoryMap.end())
					{
						heapMemoryItem = I.value().objectCast<CWinMemory>();
						if (heapMemoryItem->m_BaseAddress == candidateHeap32 &&
							heapMemoryItem->m_RegionType == Heap32Region)
						{
							memoryItem->m_RegionType = HeapSegment32Region;
							memoryItem->u_HeapSegment_HeapItem = heapMemoryItem;
							continue;
						}
					}
                }
            }
        }
    }

#ifdef _WIN64

    PS_SYSTEM_DLL_INIT_BLOCK ldrInitBlock = { 0 };
    PVOID ldrInitBlockBaseAddress = NULL;
    QMap<quint64, CMemoryPtr>::iterator cfgBitmapMemoryItem;
    PPH_STRING ntdllFileName;
    
    ntdllFileName = PhConcatStrings2(USER_SHARED_DATA->NtSystemRoot, L"\\System32\\ntdll.dll");
    status = PhGetProcedureAddressRemote(
        ProcessHandle,
        ntdllFileName->Buffer,
        "LdrSystemDllInitBlock",
        0,
        &ldrInitBlockBaseAddress,
        NULL
        );

    if (NT_SUCCESS(status) && ldrInitBlockBaseAddress)
    {
        status = NtReadVirtualMemory(
            ProcessHandle,
            ldrInitBlockBaseAddress,
            &ldrInitBlock,
            sizeof(PS_SYSTEM_DLL_INIT_BLOCK),
            NULL
            );
    }

    PhDereferenceObject(ntdllFileName);

    if (NT_SUCCESS(status) && ldrInitBlock.Size != 0)
    {
        PVOID cfgBitmapAddress = NULL;
        PVOID cfgBitmapWow64Address = NULL;

        if (RTL_CONTAINS_FIELD(&ldrInitBlock, ldrInitBlock.Size, Wow64CfgBitMap))
        {
            cfgBitmapAddress = (PVOID)ldrInitBlock.CfgBitMap;
            cfgBitmapWow64Address = (PVOID)ldrInitBlock.Wow64CfgBitMap;
        }

        if (cfgBitmapAddress)
		{
			cfgBitmapMemoryItem = PhLookupMemoryItemList(MemoryMap, cfgBitmapAddress);

			if(cfgBitmapMemoryItem != MemoryMap.end())
			{
				QMap<quint64, CMemoryPtr>::iterator memoryItem = cfgBitmapMemoryItem;

				while (memoryItem != MemoryMap.end() && memoryItem.value().objectCast<CWinMemory>()->m_AllocationBaseItem == cfgBitmapMemoryItem.value())
				{
					// lucasg: We could do a finer tagging since each MEM_COMMIT memory
					// map is the CFG bitmap of a loaded module. However that might be
					// brittle to changes made by Windows dev teams.
					memoryItem.value().objectCast<CWinMemory>()->m_RegionType = CfgBitmapRegion;

					memoryItem++;
				}
			}
        }

        // Note: Wow64 processes on 64bit also have CfgBitmap regions.
        if (isWow64 && cfgBitmapWow64Address)
        {
			cfgBitmapMemoryItem = PhLookupMemoryItemList(MemoryMap, cfgBitmapWow64Address);

			if(cfgBitmapMemoryItem != MemoryMap.end())
			{
				QMap<quint64, CMemoryPtr>::iterator memoryItem = cfgBitmapMemoryItem;

				while (memoryItem != MemoryMap.end() && memoryItem.value().objectCast<CWinMemory>()->m_AllocationBaseItem == cfgBitmapMemoryItem.value())
				{
					memoryItem.value().objectCast<CWinMemory>()->m_RegionType = CfgBitmap32Region;

					memoryItem++;
				}
			}
        }
    }
#endif

    PhFree(processes);

    return STATUS_SUCCESS;
}

NTSTATUS PhpUpdateMemoryWsCounters(
    QMap<quint64, CMemoryPtr>& MemoryMap,
    _In_ HANDLE ProcessHandle
    )
{
    PLIST_ENTRY listEntry;
    PMEMORY_WORKING_SET_EX_INFORMATION info;

    info = (PMEMORY_WORKING_SET_EX_INFORMATION)PhAllocatePage(WS_REQUEST_COUNT * sizeof(MEMORY_WORKING_SET_EX_INFORMATION), NULL);

    if (!info)
        return STATUS_NO_MEMORY;

	for(QMap<quint64, CMemoryPtr>::iterator I = MemoryMap.begin(); I != MemoryMap.end(); ++I)
    {
		QSharedPointer<CWinMemory> memoryItem = I.value().objectCast<CWinMemory>();
        ULONG_PTR virtualAddress;
        SIZE_T remainingPages;
        SIZE_T requestPages;
        SIZE_T i;

        if (!(memoryItem->m_State & MEM_COMMIT))
            continue;

        virtualAddress = (ULONG_PTR)memoryItem->m_BaseAddress;
        remainingPages = memoryItem->m_RegionSize / PAGE_SIZE;

        while (remainingPages != 0)
        {
            requestPages = min(remainingPages, WS_REQUEST_COUNT);

            for (i = 0; i < requestPages; i++)
            {
                info[i].VirtualAddress = (PVOID)virtualAddress;
                virtualAddress += PAGE_SIZE;
            }

            if (NT_SUCCESS(NtQueryVirtualMemory(
                ProcessHandle,
                NULL,
                MemoryWorkingSetExInformation,
                info,
                requestPages * sizeof(MEMORY_WORKING_SET_EX_INFORMATION),
                NULL
                )))
            {
                for (i = 0; i < requestPages; i++)
                {
                    PMEMORY_WORKING_SET_EX_BLOCK block = &info[i].u1.VirtualAttributes;

                    if (block->Valid)
                    {
                        memoryItem->m_TotalWorkingSet += PAGE_SIZE;

                        if (block->ShareCount > 1)
                            memoryItem->m_SharedWorkingSet += PAGE_SIZE;
                        if (block->ShareCount == 0)
                            memoryItem->m_PrivateWorkingSet += PAGE_SIZE;
                        if (block->Shared)
                            memoryItem->m_ShareableWorkingSet += PAGE_SIZE;
                        if (block->Locked)
                            memoryItem->m_LockedWorkingSet += PAGE_SIZE;
                    }
                }
            }

            remainingPages -= requestPages;
        }
    }

    PhFreePage(info);

    return STATUS_SUCCESS;
}

/*NTSTATUS PhpUpdateMemoryWsCountersOld(
    QMap<quint64, CMemoryPtr>& MemoryMap,
    _In_ HANDLE ProcessHandle
    )
{
    NTSTATUS status;
    PMEMORY_WORKING_SET_INFORMATION info;
    QSharedPointer<CWinMemory> memoryItem = NULL;
    ULONG_PTR i;

    if (!NT_SUCCESS(status = PhGetProcessWorkingSetInformation(ProcessHandle, &info)))
        return status;

    for (i = 0; i < info->NumberOfEntries; i++)
    {
        PMEMORY_WORKING_SET_BLOCK block = &info->WorkingSetInfo[i];
        ULONG_PTR virtualAddress = block->VirtualPage * PAGE_SIZE;

        if (!memoryItem || virtualAddress < (ULONG_PTR)memoryItem->m_BaseAddress ||
            virtualAddress >= (ULONG_PTR)memoryItem->m_BaseAddress + memoryItem->m_RegionSize)
        {
            memoryItem = MemoryMap.value(virtualAddress);
        }

        if (memoryItem)
        {
            memoryItem->m_TotalWorkingSet += PAGE_SIZE;

            if (block->ShareCount > 1)
                memoryItem->m_SharedWorkingSet += PAGE_SIZE;
            if (block->ShareCount == 0)
                memoryItem->m_PrivateWorkingSet += PAGE_SIZE;
            if (block->Shared)
                memoryItem->m_ShareableWorkingSet += PAGE_SIZE;
        }
    }

    PhFree(info);

    return STATUS_SUCCESS;
}*/

NTSTATUS PhQueryMemoryItemList(
	_In_ HANDLE ProcessId,
	_In_ ULONG Flags,
	QMap<quint64, CMemoryPtr>& MemoryMap
)
{
    NTSTATUS status;
    HANDLE processHandle;
    ULONG_PTR allocationGranularity;
    PVOID baseAddress = (PVOID)0;
    MEMORY_BASIC_INFORMATION basicInfo;
    QSharedPointer<CWinMemory> allocationBaseItem;
    QSharedPointer<CWinMemory> previousMemoryItem;

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, ProcessId)))
    {
        if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION, ProcessId)))
        {
			return status;
        }
    }

    allocationGranularity = PhSystemBasicInformation.AllocationGranularity;

	int count = 0;

    while (NT_SUCCESS(NtQueryVirtualMemory(
		processHandle, 
		baseAddress, 
		MemoryBasicInformation, 
		&basicInfo, 
		sizeof(MEMORY_BASIC_INFORMATION), 
		NULL
		)))
    {
	    QSharedPointer<CWinMemory> memoryItem;

        if (basicInfo.State & MEM_FREE)
        {
            if (Flags & PH_QUERY_MEMORY_IGNORE_FREE)
                goto ContinueLoop;

            basicInfo.AllocationBase = basicInfo.BaseAddress;
        }

		memoryItem = QSharedPointer<CWinMemory>(new CWinMemory());
		memoryItem->InitBasicInfo(&basicInfo, ProcessId);

        if (basicInfo.AllocationBase == basicInfo.BaseAddress)
            allocationBaseItem = memoryItem;
        if (allocationBaseItem && (quint64)basicInfo.AllocationBase == allocationBaseItem->m_BaseAddress)
            memoryItem->m_AllocationBaseItem = allocationBaseItem;

        if (basicInfo.State & MEM_COMMIT)
        {
            memoryItem->m_CommittedSize = memoryItem->m_RegionSize;

            if (basicInfo.Type & MEM_PRIVATE)
                memoryItem->m_PrivateSize = memoryItem->m_RegionSize;
        }

		MemoryMap.insert(memoryItem->m_BaseAddress, memoryItem);

        if (basicInfo.State & MEM_FREE)
        {
            if ((ULONG_PTR)basicInfo.BaseAddress & (allocationGranularity - 1))
            {
                ULONG_PTR nextAllocationBase;
                ULONG_PTR potentialUnusableSize;

                // Split this free region into an unusable and a (possibly empty) usable region.

                nextAllocationBase = ALIGN_UP_BY(basicInfo.BaseAddress, allocationGranularity);
                potentialUnusableSize = nextAllocationBase - (ULONG_PTR)basicInfo.BaseAddress;

                memoryItem->m_RegionType = UnusableRegion;

                // VMMap does this, but is it correct?
                //if (previousMemoryItem && (previousMemoryItem->State & MEM_COMMIT))
                //    memoryItem->CommittedSize = min(potentialUnusableSize, basicInfo.RegionSize);

                if (nextAllocationBase < (ULONG_PTR)basicInfo.BaseAddress + basicInfo.RegionSize)
                {
                    QSharedPointer<CWinMemory> otherMemoryItem;

                    memoryItem->m_RegionSize = potentialUnusableSize;

                    otherMemoryItem = QSharedPointer<CWinMemory>(new CWinMemory());
					otherMemoryItem->InitBasicInfo(&basicInfo, ProcessId);

                    otherMemoryItem->m_BaseAddress = nextAllocationBase;
                    otherMemoryItem->m_AllocationBase = otherMemoryItem->m_BaseAddress;
                    otherMemoryItem->m_RegionSize = basicInfo.RegionSize - potentialUnusableSize;
                    otherMemoryItem->m_AllocationBaseItem = otherMemoryItem;

                    MemoryMap.insert(otherMemoryItem->m_BaseAddress, otherMemoryItem);

                    previousMemoryItem = otherMemoryItem;
                    goto ContinueLoop;
                }
            }
        }

        previousMemoryItem = memoryItem;

ContinueLoop:
        baseAddress = PTR_ADD_OFFSET(baseAddress, basicInfo.RegionSize);
    }

    if (Flags & PH_QUERY_MEMORY_REGION_TYPE)
        PhpUpdateMemoryRegionTypes(MemoryMap, ProcessId, processHandle);

    if (Flags & PH_QUERY_MEMORY_WS_COUNTERS)
        PhpUpdateMemoryWsCounters(MemoryMap, processHandle);

    NtClose(processHandle);

	return STATUS_SUCCESS;
}