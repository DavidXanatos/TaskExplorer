/*
 * XProcessHacker
 *
 * Copyright (C) 2020 DavidXanatos
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
#include "log_buff.h"

LOG_BUFFER* DebugLogBuffer = NULL;
KSPIN_LOCK DebugLogLock;

void DebugPrintCallback(IN PANSI_STRING String, IN ULONG ComponentId, IN ULONG Level);

#pragma alloc_text(PAGE, KpiSetDebugLog) 
#pragma alloc_text(PAGE, KpiReadDebugLog) 
#pragma alloc_text(NONPAGE, DebugPrintCallback) 

BOOLEAN DebugFilterSaved = FALSE;
BOOLEAN DebugFilterBackup[3900];

void SetDebugFilterState();
void RestoreFilterState();

#pragma alloc_text(PAGE, SetDebugFilterState)
#pragma alloc_text(PAGE, RestoreFilterState)

NTSTATUS KpiSetDebugLog(
    _In_ ULONG Enable,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if ((DebugLogBuffer != NULL) == (Enable != 0))
        return status;

    KIRQL OldIrql = KeAcquireSpinLockRaiseToSynch(&DebugLogLock);
    if (Enable)
    {
        DebugLogBuffer = log_buffer_init(8 * 8 * 1024);

        DbgSetDebugPrintCallback(DebugPrintCallback, TRUE);

        SetDebugFilterState();
    }
    else
    {
        DbgSetDebugPrintCallback(DebugPrintCallback, FALSE);

        log_buffer_free(DebugLogBuffer);
        DebugLogBuffer = NULL;

        RestoreFilterState();
    }
    KeReleaseSpinLock(&DebugLogLock, OldIrql);

    return status;
}

void DebugPrintCallback(IN PANSI_STRING String, IN ULONG ComponentId, IN ULONG Level)
{
    PIRP irp = NULL;
    if (!String || !String->Buffer)
        return;

    KIRQL OldIrql = KeAcquireSpinLockRaiseToSynch(&DebugLogLock);
    if (DebugLogBuffer)
    {
        CHAR* write_buff = log_buffer_push_entry(String->Length, DebugLogBuffer);
        if (write_buff) // check if we head enough space in the
        {
            memcpy(write_buff, String->Buffer, String->Length);
        }
    }
    KeReleaseSpinLock(&DebugLogLock, OldIrql);
}


NTSTATUS KpiReadDebugLog(
    _Inout_ PULONG SeqNumber,
    _Out_writes_bytes_(BufferSize) PVOID Buffer,
    _In_ SIZE_T BufferLength,
    _Out_ PSIZE_T ReturnLength,
    _In_ KPROCESSOR_MODE AccessMode
    )
{
    NTSTATUS status = STATUS_SUCCESS;

    if (AccessMode != KernelMode)
    {
        __try
        {
            ProbeForWrite(Buffer, BufferLength, sizeof(ULONG));

            if (ReturnLength)
                ProbeForWrite(ReturnLength, sizeof(ULONG), sizeof(ULONG));
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            return GetExceptionCode();
        }
    }

    KIRQL OldIrql = KeAcquireSpinLockRaiseToSynch(&DebugLogLock);
    if (DebugLogBuffer)
    {
        CHAR* read_ptr = log_buffer_get_next(*SeqNumber, DebugLogBuffer);
        if (read_ptr)
        {
            LOG_BUFFER_SIZE_T Size = log_buffer_get_size(&read_ptr, DebugLogBuffer);
            *SeqNumber = log_buffer_get_seq_num(&read_ptr, DebugLogBuffer);

            if (Size + 1 > BufferLength)
            {
                status = STATUS_BUFFER_TOO_SMALL;
                Size = (LOG_BUFFER_SIZE_T)BufferLength - 1;
            }

            memcpy(Buffer, read_ptr, Size);
            ((char*)Buffer)[Size] = '\0';

            if (ReturnLength)
                *ReturnLength = Size;

        }
        else
            status = STATUS_NO_MORE_ENTRIES;
    }
    else
        status = STATUS_UNSUCCESSFUL;
    KeReleaseSpinLock(&DebugLogLock, OldIrql);

    return status;
}

void SetDebugFilterState()
{
    DebugFilterSaved = TRUE;

    for (int i = 0; i < 130; i++)
    {
        for (int j = 0; j < 30; j++)
        {
            DebugFilterBackup[30 * i + j] = (BOOLEAN)DbgQueryDebugFilterState(i, j);

            DbgSetDebugFilterState(i, j, 1);
        }
    }
}

void RestoreFilterState()
{
    if (!DebugFilterSaved)
        return;

    for (int i = 0; i < 130; i++)
    {
        for (int j = 0; j < 30; j++)
        {
            DbgSetDebugFilterState(i, j, DebugFilterBackup[30 * i + j]);
        }
    }
}
