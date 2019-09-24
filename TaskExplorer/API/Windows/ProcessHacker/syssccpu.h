#pragma once

#include "../ProcessHacker.h"

BOOLEAN PhSipGetCpuFrequencyFromDistribution(PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Current, PSYSTEM_PROCESSOR_PERFORMANCE_DISTRIBUTION Previous, double* Fraction);
NTSTATUS PhSipQueryProcessorPerformanceDistribution(_Out_ PVOID *Buffer);