/*
 * Process Hacker -
 *   qt wrapper and syssccpu.c functions
 *
 * Copyright (C) 2011-2016 wj32
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "syssccpu.h"

BOOLEAN PhSipGetCpuFrequencyFromDistribution(PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Current, PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Previous, double* Fraction)
{
    ULONG stateSize;
    PVOID differences;
    PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION stateDistribution;
    PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION stateDifference;
    PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8 hitcountOld;
    ULONG i;
    ULONG j;
    DOUBLE count;
    DOUBLE total;

    // Calculate the differences from the last performance distribution.

    if (!Current || Current->ProcessorCount != PhSystemBasicInformation.NumberOfProcessors || !Previous || Previous->ProcessorCount != PhSystemBasicInformation.NumberOfProcessors)
        return FALSE;

    stateSize = FIELD_OFFSET(SYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION, States) + sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT) * 2;
    differences = PhAllocate(stateSize * PhSystemBasicInformation.NumberOfProcessors);

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDistribution = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(Current, Current->Offsets[i]);
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        if (stateDistribution->StateCount != 2)
        {
            PhFree(differences);
            return FALSE;
        }

        for (j = 0; j < stateDistribution->StateCount; j++)
        {
            if (WindowsVersion >= WINDOWS_8_1)
            {
                stateDifference->States[j] = stateDistribution->States[j];
            }
            else
            {
                hitcountOld = (PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8)PTR_ADD_OFFSET(stateDistribution->States, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8) * j);
                stateDifference->States[j].Hits = hitcountOld->Hits;
                stateDifference->States[j].PercentFrequency = hitcountOld->PercentFrequency;
            }
        }
    }

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDistribution = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(Previous, Previous->Offsets[i]);
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        if (stateDistribution->StateCount != 2)
        {
            PhFree(differences);
            return FALSE;
        }

        for (j = 0; j < stateDistribution->StateCount; j++)
        {
            if (WindowsVersion >= WINDOWS_8_1)
            {
                stateDifference->States[j].Hits -= stateDistribution->States[j].Hits;
            }
            else
            {
                hitcountOld = (PSYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8)PTR_ADD_OFFSET(stateDistribution->States, sizeof(SYSTEM_PROCESSOR_PERFORMANCE_HITCOUNT_WIN8) * j);
                stateDifference->States[j].Hits -= hitcountOld->Hits;
            }
        }
    }

    // Calculate the frequency.

    count = 0;
    total = 0;

    for (i = 0; i < PhSystemBasicInformation.NumberOfProcessors; i++)
    {
        stateDifference = (PSYSTEM_PROCESSOR_PERFORMANCE_STATE_DISTRIBUTION)PTR_ADD_OFFSET(differences, stateSize * i);

        for (j = 0; j < 2; j++)
        {
            count += stateDifference->States[j].Hits;
            total += stateDifference->States[j].Hits * stateDifference->States[j].PercentFrequency;
        }
    }

    PhFree(differences);

    if (count == 0)
        return FALSE;

    total /= count;
    total /= 100;
    *Fraction = total;

    return TRUE;
}

NTSTATUS PhSipQueryProcessorPerformanceDistribution(_Out_ PVOID *Buffer)
{
    NTSTATUS status;
    PVOID buffer;
    ULONG bufferSize;
    ULONG attempts;

    bufferSize = 0x100;
    buffer = PhAllocate(bufferSize);

    status = NtQuerySystemInformation(SystemProcessorPerformanceDistribution, buffer, bufferSize, &bufferSize);
    attempts = 0;

    while (status == STATUS_INFO_LENGTH_MISMATCH && attempts < 8)
    {
        PhFree(buffer);
        buffer = PhAllocate(bufferSize);

        status = NtQuerySystemInformation(SystemProcessorPerformanceDistribution, buffer, bufferSize, &bufferSize);
        attempts++;
    }

    if (NT_SUCCESS(status))
        *Buffer = buffer;
    else
        PhFree(buffer);

    return status;
}