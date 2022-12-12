#pragma once

extern "C"{

// counters

VOID EtPerfCounterInitialization(
    VOID
);

NTSTATUS EtUpdatePerfCounterData(
    VOID
);

FLOAT EtLookupProcessGpuUtilization(
    _In_ HANDLE ProcessId
);

_Success_(return)
BOOLEAN EtLookupProcessGpuMemoryCounters(
    _In_opt_ HANDLE ProcessId,
    _Out_ PULONG64 SharedUsage,
    _Out_ PULONG64 DedicatedUsage,
    _Out_ PULONG64 CommitUsage
);

FLOAT EtLookupTotalGpuUtilization(
    VOID
);

FLOAT EtLookupTotalGpuEngineUtilization(
    _In_ ULONG EngineId
);

FLOAT EtLookupTotalGpuAdapterUtilization(
    _In_ LUID AdapterLuid
);

FLOAT EtLookupTotalGpuAdapterEngineUtilization(
    _In_ LUID AdapterLuid,
    _In_ ULONG EngineId
);

ULONG64 EtLookupTotalGpuDedicated(
    VOID
);

ULONG64 EtLookupTotalGpuAdapterDedicated(
    _In_ LUID AdapterLuid
);

ULONG64 EtLookupTotalGpuShared(
    VOID
);

ULONG64 EtLookupTotalGpuAdapterShared(
    _In_ LUID AdapterLuid
);

}