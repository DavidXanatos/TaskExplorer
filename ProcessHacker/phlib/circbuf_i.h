/*
 * Copyright (c) 2022 Winsider Seminars & Solutions, Inc.  All rights reserved.
 *
 * This file is part of System Informer.
 *
 * Authors:
 *
 *     wj32    2010-2016
 *
 */

#ifdef T

#include <templ.h>

VOID T___(PhInitializeCircularBuffer, T)(
    _Out_ T___(PPH_CIRCULAR_BUFFER, T) Buffer,
    _In_ ULONG Size
    )
{
#ifdef PH_CIRCULAR_BUFFER_POWER_OF_TWO_SIZE
    Buffer->Size = PhRoundUpToPowerOfTwo(Size);
    Buffer->SizeMinusOne = Buffer->Size - 1;
#else
    Buffer->Size = Size;
#endif

    Buffer->Count = 0;
    Buffer->Index = 0;
    Buffer->Data = PhAllocate(sizeof(T) * Buffer->Size);
}

VOID T___(PhDeleteCircularBuffer, T)(
    _Inout_ T___(PPH_CIRCULAR_BUFFER, T) Buffer
    )
{
    PhFree(Buffer->Data);
}

VOID T___(PhResizeCircularBuffer, T)(
    _Inout_ T___(PPH_CIRCULAR_BUFFER, T) Buffer,
    _In_ ULONG NewSize
    )
{
    T *newData;
    ULONG tailSize;
    ULONG headSize;

#ifdef PH_CIRCULAR_BUFFER_POWER_OF_TWO_SIZE
    NewSize = PhRoundUpToPowerOfTwo(NewSize);
#endif

    // If we're not actually resizing it, return.
    if (NewSize == Buffer->Size)
        return;

    newData = PhAllocate(sizeof(T) * NewSize);
    tailSize = (ULONG)(Buffer->Size - Buffer->Index);
    headSize = Buffer->Count - tailSize;

    if (NewSize > Buffer->Size)
    {
        // Copy the tail, then the head.
        memcpy(newData, &Buffer->Data[Buffer->Index], sizeof(T) * tailSize);
        memcpy(&newData[tailSize], Buffer->Data, sizeof(T) * headSize);
        Buffer->Index = 0;
    }
    else
    {
        if (tailSize >= NewSize)
        {
            // Copy only a part of the tail.
            memcpy(newData, &Buffer->Data[Buffer->Index], sizeof(T) * NewSize);
            Buffer->Index = 0;
        }
        else
        {
            // Copy the tail, then only part of the head.
            memcpy(newData, &Buffer->Data[Buffer->Index], sizeof(T) * tailSize);
            memcpy(&newData[tailSize], Buffer->Data, sizeof(T) * (NewSize - tailSize));
            Buffer->Index = 0;
        }

        // Since we're making the circular buffer smaller, limit the count.
        if (Buffer->Count > NewSize)
            Buffer->Count = NewSize;
    }

    Buffer->Data = newData;
    Buffer->Size = NewSize;
#ifdef PH_CIRCULAR_BUFFER_POWER_OF_TWO_SIZE
    Buffer->SizeMinusOne = NewSize - 1;
#endif
}

VOID T___(PhClearCircularBuffer, T)(
    _Inout_ T___(PPH_CIRCULAR_BUFFER, T) Buffer
    )
{
    Buffer->Count = 0;
    Buffer->Index = 0;
}

VOID T___(PhCopyCircularBuffer, T)(
    _Inout_ T___(PPH_CIRCULAR_BUFFER, T) Buffer,
    _Out_writes_(Count) T *Destination,
    _In_ ULONG Count
    )
{
    ULONG tailSize;
    ULONG headSize;

    tailSize = (ULONG)(Buffer->Size - Buffer->Index);
    headSize = Buffer->Count - tailSize;

    if (Count > Buffer->Count)
        Count = Buffer->Count;

    if (tailSize >= Count)
    {
        // Copy only a part of the tail.
        memcpy(Destination, &Buffer->Data[Buffer->Index], sizeof(T) * Count);
    }
    else
    {
        // Copy the tail, then only part of the head.
        memcpy(Destination, &Buffer->Data[Buffer->Index], sizeof(T) * tailSize);
        memcpy(&Destination[tailSize], Buffer->Data, sizeof(T) * (Count - tailSize));
    }
}

#endif
