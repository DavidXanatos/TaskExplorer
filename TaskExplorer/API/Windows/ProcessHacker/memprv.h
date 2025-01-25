#pragma once

typedef enum _PH_MEMORY_REGION_TYPE
{
    UnknownRegion,
    CustomRegion,
    UnusableRegion,
    MappedFileRegion,
    UserSharedDataRegion,
    PebRegion,
    Peb32Region,
    TebRegion,
    Teb32Region, // Not used
    StackRegion,
    Stack32Region,
    HeapRegion,
    Heap32Region,
    HeapSegmentRegion,
    HeapSegment32Region,
    CfgBitmapRegion,
    CfgBitmap32Region,
    ApiSetMapRegion,
    HypervisorSharedDataRegion,
    ReadOnlySharedMemoryRegion,
    CodePageDataRegion,
    GdiSharedHandleTableRegion,
    ShimDataRegion,
    ActivationContextDataRegion,
    WerRegistrationDataRegion,
    SiloSharedDataRegion,
    TelemetryCoverageRegion
} PH_MEMORY_REGION_TYPE;

typedef enum _PH_ACTIVATION_CONTEXT_DATA_TYPE
{
    CustomActivationContext,
    ProcessActivationContext,
    SystemActivationContext
} PH_ACTIVATION_CONTEXT_DATA_TYPE;

#define PH_QUERY_MEMORY_IGNORE_FREE 0x1
#define PH_QUERY_MEMORY_REGION_TYPE 0x2
#define PH_QUERY_MEMORY_WS_COUNTERS 0x4
//#define PH_QUERY_MEMORY_ZERO_PAD_ADDRESSES 0x8

NTSTATUS PhQueryMemoryItemList(_In_ HANDLE ProcessId, _In_ ULONG Flags, QMap<quint64, CMemoryPtr>& MemoryMap);


