#pragma once

ULONG64 EtLookupProcessGpuEngineUtilization(_In_opt_ HANDLE ProcessId);

ULONG64 EtLookupProcessGpuDedicated(_In_opt_ HANDLE ProcessId);

ULONG64 EtLookupProcessGpuSharedUsage(_In_opt_ HANDLE ProcessId);

ULONG64 EtLookupProcessGpuCommitUsage(_In_opt_ HANDLE ProcessId);