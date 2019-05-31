/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2020 David Xanatos
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
#include <QFutureWatcher>
#include <QtConcurrent>
#include "ProcessHacker.h"
#include <lsasup.h>

#include "WinProcess.h"

#include <QtWin>

 // CWinProcess private members

struct SWinProcess
{
	SWinProcess() : 
		UniqueProcessId(NULL),
		QueryHandle(NULL),
		Flags(NULL),
		AsyncFinished(false),
		OsContextVersion(0)
	{}

	// Handles
	HANDLE UniqueProcessId;
	HANDLE QueryHandle;

	// Flags
	union
	{
		ULONG Flags;
		struct
		{
			//ULONG UpdateIsDotNet : 1;
			ULONG IsBeingDebugged : 1;
			ULONG IsDotNet : 1;
			ULONG IsElevated : 1;
			ULONG IsInJob : 1;
			ULONG IsInSignificantJob : 1;
			ULONG IsPacked : 1;
			ULONG IsHandleValid : 1;
			ULONG IsSuspended : 1;
			ULONG IsWow64 : 1;
			ULONG IsImmersive : 1;
			ULONG IsWow64Valid : 1;
			ULONG IsPartiallySuspended : 1;
			ULONG IsProtectedHandle : 1;
			ULONG IsProtectedProcess : 1;
			ULONG IsSecureProcess : 1;
			ULONG IsSubsystemProcess : 1;
			ULONG IsControlFlowGuardEnabled : 1;
			ULONG Spare : 15; //14;
		};
	};
	bool AsyncFinished;

	// Security
	QByteArray Sid;
	TOKEN_ELEVATION_TYPE ElevationType;
	MANDATORY_LEVEL IntegrityLevel;
	QString IntegrityString;

	// Other
	HANDLE ConsoleHostProcessId;

	// Dynamic
	VM_COUNTERS_EX VmCounters;
	IO_COUNTERS IoCounters;


	// Other Fields
	PH_KNOWN_PROCESS_TYPE KnownProcessType;
	PS_PROTECTION Protection;
	quint64 ProcessSequenceNumber;
	ulong JobObjectId;
	QString PackageFullName;

	// Signature, Packed
	VERIFY_RESULT VerifyResult;
	QString VerifySignerName;
	ulong ImportFunctions;
	ulong ImportModules;

	// OS Context
    GUID OsContextGuid;
    ulong OsContextVersion;

	// File
	//QPixmap SmallIcon;
	//QPixmap LargeIcon;
	struct SVersionInfo
	{
		QString CompanyName;
		QString FileDescription;
		QString FileVersion;
		QString ProductName;
	} VersionInfo;


	/*PH_HASH_ENTRY HashEntry;
ULONG State;
PPH_PROCESS_RECORD Record;


// Misc.

ULONG JustProcessed;
PH_EVENT Stage1Event;

PPH_POINTER_LIST ServiceList;
PH_QUEUED_LOCK ServiceListLock;

// Dynamic


FLOAT CpuUsage; // Below Windows 7, sum of kernel and user CPU usage; above Windows 7, cycle-based CPU usage.
FLOAT CpuKernelUsage;
FLOAT CpuUserUsage;

PH_UINT64_DELTA CpuKernelDelta;
PH_UINT64_DELTA CpuUserDelta;
PH_UINT64_DELTA IoReadDelta;
PH_UINT64_DELTA IoWriteDelta;
PH_UINT64_DELTA IoOtherDelta;
PH_UINT64_DELTA IoReadCountDelta;
PH_UINT64_DELTA IoWriteCountDelta;
PH_UINT64_DELTA IoOtherCountDelta;
PH_UINT32_DELTA ContextSwitchesDelta;
PH_UINT32_DELTA PageFaultsDelta;
PH_UINT64_DELTA CycleTimeDelta; // since WIN7


ULONG TimeSequenceNumber;
PH_CIRCULAR_BUFFER_FLOAT CpuKernelHistory;
PH_CIRCULAR_BUFFER_FLOAT CpuUserHistory;
PH_CIRCULAR_BUFFER_ULONG64 IoReadHistory;
PH_CIRCULAR_BUFFER_ULONG64 IoWriteHistory;
PH_CIRCULAR_BUFFER_ULONG64 IoOtherHistory;
PH_CIRCULAR_BUFFER_SIZE_T PrivateBytesHistory;
//PH_CIRCULAR_BUFFER_SIZE_T WorkingSetHistory;

// New fields
PH_UINTPTR_DELTA PrivateBytesDelta;
*/
};

// CWinProcess Helper Functions

QReadWriteLock g_Sid2NameMutex;
QMap<QByteArray, QString> g_Sid2NameCache;

QString GetSidFullNameCached(const QByteArray& Sid, bool bQuick = false)
{
	QReadLocker ReadLocker(&g_Sid2NameMutex);
	QMap<QByteArray, QString>::iterator I = g_Sid2NameCache.find(Sid);
	if (I != g_Sid2NameCache.end())
		return I.value();
	ReadLocker.unlock();
	
	// From PhpProcessQueryStage1 
	// Note: We delay resolving the SID name because the local LSA cache might still be
	// initializing for users on domain networks with slow links (e.g. VPNs). This can block
	// for a very long time depending on server/network conditions. (dmex)

	if (bQuick) // so if we want a quick result we skip this step
		return "";

	QWriteLocker WriteLocker(&g_Sid2NameMutex);
	PPH_STRING fullName = PhGetSidFullName((PSID)Sid.data(), TRUE, NULL);
	QString FullName = CastPhString(fullName);
	g_Sid2NameCache.insert(Sid, FullName);
	return FullName;
}

// CWinProcess Class members

CWinProcess::CWinProcess(QObject *parent) : CProcess(parent)
{
	// Basic
	m_SessionId = -1;

	// Dynamic
	m_BasePriority = 0;
	m_NumberOfHandles = 0;

	// GDI, USER handles
	m_GdiHandles = 0;
    m_UserHandles = 0;

	// ph special
	m = new SWinProcess();
}

CWinProcess::~CWinProcess()
{
	UnInit(); // just in case we forgot to do that

	delete m;
}

bool CWinProcess::InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* Process)
{
	QWriteLocker Locker(&m_Mutex);

	PPH_STRING FileName = NULL;

	// PhCreateProcessItem
	m->UniqueProcessId = Process->UniqueProcessId;
	m_ProcessId = (uint64_t)m->UniqueProcessId;

	// PhpFillProcessItem Begin
	m_ParentProcessId = (uint64_t)Process->InheritedFromUniqueProcessId;
	m_SessionId = (uint64_t)Process->SessionId;

	if (m_ProcessId != (uint64_t)SYSTEM_IDLE_PROCESS_ID)
		m_ProcessName = QString::fromWCharArray(Process->ImageName.Buffer, Process->ImageName.Length / sizeof(wchar_t));
	else
		m_ProcessName = QString::fromWCharArray(SYSTEM_IDLE_PROCESS_NAME);

	// MSDN: FILETIME Contains a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601 (UTC).
	m_CreateTime = QDateTime::fromTime_t((int64_t)Process->CreateTime.QuadPart / 10000000ULL - 11644473600ULL);

	// Open a handle to the Process for later usage.
	if (PH_IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		// Note: we need PROCESS_VM_READ for OS Context (DX)
		if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
		{
			m->IsHandleValid = TRUE;
		}

		if (!m->QueryHandle)
		{
			PhOpenProcess(&m->QueryHandle, PROCESS_QUERY_LIMITED_INFORMATION, m->UniqueProcessId);
		}
	}

	// Process flags
	if (m->QueryHandle)
	{
		PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;

		if (NT_SUCCESS(PhGetProcessExtendedBasicInformation(m->QueryHandle, &basicInfo)))
		{
			m->IsProtectedProcess = basicInfo.IsProtectedProcess;
			m->IsSecureProcess = basicInfo.IsSecureProcess;
			m->IsSubsystemProcess = basicInfo.IsSubsystemProcess;
			m->IsWow64 = basicInfo.IsWow64Process;
			m->IsWow64Valid = TRUE;
		}
	}

	// Process information
	{
		// If we're dealing with System (PID 4), we need to get the
		// kernel file name. Otherwise, get the image file name. (wj32)
		if (m->UniqueProcessId != SYSTEM_PROCESS_ID)
		{
			NTSTATUS status = STATUS_UNSUCCESSFUL;
			PPH_STRING fileName;

			if (m->QueryHandle && !m->IsSubsystemProcess)
			{
				status = PhGetProcessImageFileNameWin32(m->QueryHandle, &fileName);
			}

			if (!NT_SUCCESS(status))
			{
				status = PhGetProcessImageFileNameByProcessId(m->UniqueProcessId, &fileName);
			}

			if (NT_SUCCESS(status))
			{
				FileName = PhGetFileName(fileName);
				PhDereferenceObject(fileName);
			}
		}
		else
		{
			PPH_STRING fileName;

			fileName = PhGetKernelFileName();

			if (fileName)
			{
				FileName = PhGetFileName(fileName);
				PhDereferenceObject(fileName);
			}
		}

		m_FileName = CastPhString(FileName, false); // we need the FileName for PhGetProcessKnownTypeEx so dereference later
	}

	// Token information
	if (m->QueryHandle && m->UniqueProcessId != SYSTEM_PROCESS_ID) // System token can't be opened (dmex)
	{
		// Note: this is done in UpdateDynamicData which is to be called right after InitStaticData
	}
	else
	{
		if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID) // System token can't be opened on XP (wj32)
		{
			m->Sid = QByteArray((char*)&PhSeLocalSystemSid, RtlLengthSid(&PhSeLocalSystemSid));
			m_UserName = GetSidFullNameCached(m->Sid/*, true*/);
		}
	}

	// Known Process Type
	{
		m->KnownProcessType = PhGetProcessKnownTypeEx(m->UniqueProcessId, FileName);
	}

	// Protection
	if (m->QueryHandle)
	{
		if (WindowsVersion >= WINDOWS_8_1)
		{
			PS_PROTECTION protection;

			if (NT_SUCCESS(PhGetProcessProtection(m->QueryHandle, &protection)))
			{
				m->Protection.Level = protection.Level;
			}
		}
		else
		{
			// HACK: 'emulate' the PS_PROTECTION info for older OSes. (ge0rdi)
			if (m->IsProtectedProcess)
				m->Protection.Type = PsProtectedTypeProtected;
		}
	}
	else
	{
		// Signalize that we weren't able to get protection info with a special value.
		// Note: We use this value to determine if we should show protection information. (ge0rdi)
		m->Protection.Level = UCHAR_MAX;
	}

	// Control Flow Guard
	if (WindowsVersion >= WINDOWS_8_1 && m->QueryHandle)
	{
		BOOLEAN cfguardEnabled;

		if (NT_SUCCESS(PhGetProcessIsCFGuardEnabled(m->QueryHandle, &cfguardEnabled)))
		{
			m->IsControlFlowGuardEnabled = cfguardEnabled;
		}
	}

	// On Windows 8.1 and above, processes without threads are reflected processes
	// which will not terminate if we have a handle open. (wj32)
	if (Process->NumberOfThreads == 0 && m->QueryHandle)
	{
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}
	// PhpFillProcessItem End

	// PhpFillProcessItemExtension is done in UpdateDynamicData which is to be called right after InitStaticData


	if (FileName)
		PhDereferenceObject(FileName);


	/*processItem->TimeSequenceNumber = PhTimeSequenceNumber;

    processRecord = PhpCreateProcessRecord(processItem);
    PhpAddProcessRecord(processRecord);
    processItem->Record = processRecord;

    PhpGetProcessThreadInformation(process, &isSuspended, &isPartiallySuspended, &contextSwitches);
    PhpUpdateDynamicInfoProcessItem(processItem, process);

    // Initialize the deltas.
    PhUpdateDelta(&processItem->CpuKernelDelta, process->KernelTime.QuadPart);
    PhUpdateDelta(&processItem->CpuUserDelta, process->UserTime.QuadPart);
    PhUpdateDelta(&processItem->IoReadDelta, process->ReadTransferCount.QuadPart);
    PhUpdateDelta(&processItem->IoWriteDelta, process->WriteTransferCount.QuadPart);
    PhUpdateDelta(&processItem->IoOtherDelta, process->OtherTransferCount.QuadPart);
    PhUpdateDelta(&processItem->IoReadCountDelta, process->ReadOperationCount.QuadPart);
    PhUpdateDelta(&processItem->IoWriteCountDelta, process->WriteOperationCount.QuadPart);
    PhUpdateDelta(&processItem->IoOtherCountDelta, process->OtherOperationCount.QuadPart);
    PhUpdateDelta(&processItem->ContextSwitchesDelta, contextSwitches);
    PhUpdateDelta(&processItem->PageFaultsDelta, process->PageFaultCount);
    PhUpdateDelta(&processItem->CycleTimeDelta, process->CycleTime);
    PhUpdateDelta(&processItem->PrivateBytesDelta, process->PagefileUsage);

    processItem->IsSuspended = isSuspended;
    processItem->IsPartiallySuspended = isPartiallySuspended;*/

	/*// If this is the first run of the provider, queue the
            // process query tasks. Otherwise, perform stage 1
            // processing now and queue stage 2 processing.
            if (runCount > 0)
            {
                PH_PROCESS_QUERY_S1_DATA data;

                memset(&data, 0, sizeof(PH_PROCESS_QUERY_S1_DATA));
                data.Header.Stage = 1;
                data.Header.ProcessItem = processItem;
                PhpProcessQueryStage1(&data);
                PhpFillProcessItemStage1(&data);
                PhSetEvent(&processItem->Stage1Event);
            }
            

            // Add pending service items to the process item.
            PhUpdateProcessItemServices(processItem);*/


	// Note: on the first listing ProcessHacker does this asynchroniusly, on subsequent listings synchroniusly

	// PhpProcessQueryStage1 Begin
	NTSTATUS status;

    // Command line, .NET
    if (m->QueryHandle && !m->IsSubsystemProcess)
    {
        BOOLEAN isDotNet = FALSE;
        HANDLE processHandle = NULL;
        ULONG processQueryFlags = 0;

        if (WindowsVersion >= WINDOWS_8_1)
        {
            processHandle = m->QueryHandle;
            processQueryFlags |= PH_CLR_USE_SECTION_CHECK;
            status = STATUS_SUCCESS;
        }
        else
        {
            status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId);
        }

        if (NT_SUCCESS(status))
        {
            PPH_STRING commandLine;

            if (NT_SUCCESS(PhGetProcessCommandLine(processHandle, &commandLine)))
            {
                // Some command lines (e.g. from taskeng.exe) have nulls in them. Since Windows
                // can't display them, we'll replace them with spaces.
                for (ULONG i = 0; i < (ULONG)commandLine->Length / sizeof(WCHAR); i++)
                {
                    if (commandLine->Buffer[i] == UNICODE_NULL)
                        commandLine->Buffer[i] = ' ';
                }

                m_CommandLine = CastPhString(commandLine);
            }
        }

        if (NT_SUCCESS(status))
        {
            PhGetProcessIsDotNetEx(m->UniqueProcessId, processHandle,
#ifdef _WIN64
                processQueryFlags | PH_CLR_NO_WOW64_CHECK | (m->IsWow64 ? PH_CLR_KNOWN_IS_WOW64 : 0),
#else
                processQueryFlags,
#endif
                &isDotNet, NULL);
            m->IsDotNet = isDotNet;
        }

        if (!(processQueryFlags & PH_CLR_USE_SECTION_CHECK) && processHandle)
            NtClose(processHandle);
    }

    // Job
    // Note: this is done during dynamic update

    // Console host process
    if (m->QueryHandle)
    {
        PhGetProcessConsoleHostProcessId(m->QueryHandle, &m->ConsoleHostProcessId);
    }

    // Immersive
    if (m->QueryHandle && WINDOWS_HAS_IMMERSIVE && !m->IsSubsystemProcess)
    {
        m->IsImmersive = !!IsImmersiveProcess(m->QueryHandle);
    }

    // Package full name
    if (m->QueryHandle && WINDOWS_HAS_IMMERSIVE && m->IsImmersive)
    {
        m->PackageFullName = CastPhString(PhGetProcessPackageFullName(m->QueryHandle));
    }

    if (m->QueryHandle && m->IsHandleValid)
    {
        OBJECT_BASIC_INFORMATION basicInfo;

        if (NT_SUCCESS(PhGetHandleInformationEx(NtCurrentProcess(), m->QueryHandle, ULONG_MAX, 0, NULL, &basicInfo, NULL, NULL, NULL, NULL)))
        {
            if ((basicInfo.GrantedAccess & PROCESS_QUERY_INFORMATION) != PROCESS_QUERY_INFORMATION)
                m->IsProtectedHandle = TRUE;
        }
        else
        {
            m->IsProtectedHandle = TRUE;
        }
    }
	// PhpProcessQueryStage1 End

	// OS context
	if (!m->IsSubsystemProcess && m->QueryHandle)
	{	
		if (NT_SUCCESS(PhGetProcessSwitchContext(m->QueryHandle, &m->OsContextGuid)))
		{
			if (IsEqualGUID(m->OsContextGuid, WIN10_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_10;
			else if (IsEqualGUID(m->OsContextGuid, WINBLUE_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_8_1;
			else if (IsEqualGUID(m->OsContextGuid, WIN8_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_8;
			else if (IsEqualGUID(m->OsContextGuid, WIN7_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_7;
			else if (IsEqualGUID(m->OsContextGuid, VISTA_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_VISTA;
			else if (IsEqualGUID(m->OsContextGuid, XP_CONTEXT_GUID))
				m->OsContextVersion = WINDOWS_XP;
		}
	}

	return true;
}

bool CWinProcess::UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* Process)
{
	QWriteLocker Locker(&m_Mutex);

	bool modified = FALSE;

	//PhpGetProcessThreadInformation Begin
	BOOLEAN isSuspended;
	BOOLEAN isPartiallySuspended;
	ULONG contextSwitches;

	isSuspended = PH_IS_REAL_PROCESS_ID(Process->UniqueProcessId);
	isPartiallySuspended = FALSE;
	contextSwitches = 0;

	for (ULONG i = 0; i < Process->NumberOfThreads; i++)
	{
		if (Process->Threads[i].ThreadState != Waiting || Process->Threads[i].WaitReason != Suspended)
		{
			isSuspended = FALSE;
		}
		else
		{
			isPartiallySuspended = TRUE;
		}

		contextSwitches += Process->Threads[i].ContextSwitches;
	}

	// HACK: Minimal/Reflected processes don't have threads (TODO: Use PhGetProcessIsSuspended instead).
	if (Process->NumberOfThreads == 0)
	{
		isSuspended = FALSE;
		isPartiallySuspended = FALSE;
	}
	// PhpGetProcessThreadInformation End

	// PhpUpdateDynamicInfoProcessItem Begin
	m_BasePriority = Process->BasePriority;

	if (m->QueryHandle)
	{
		PROCESS_PRIORITY_CLASS priorityClass;

		if (NT_SUCCESS(PhGetProcessPriority(m->QueryHandle, &priorityClass)))
		{
			m_PriorityClass = priorityClass.PriorityClass;
		}
	}
	else
	{
		m_PriorityClass = 0;
	}

	m_KernelTime = Process->KernelTime.QuadPart;
	m_UserTime = Process->UserTime.QuadPart;
	m_NumberOfHandles = Process->HandleCount;
	m_NumberOfThreads = Process->NumberOfThreads;
	m_WorkingSetPrivateSize = (size_t)Process->WorkingSetPrivateSize.QuadPart;
	m_PeakNumberOfThreads = Process->NumberOfThreadsHighWatermark;
	m_HardFaultCount = Process->HardFaultCount;

	// Update VM and I/O counters.
	m->VmCounters = *(PVM_COUNTERS_EX)&Process->PeakVirtualSize;
	m->IoCounters = *(PIO_COUNTERS)&Process->ReadOperationCount;
	// PhpUpdateDynamicInfoProcessItem End


	// PhpFillProcessItemExtension
	PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension;
	if (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64())
	{
		processExtension = PH_PROCESS_EXTENSION(Process);

		m->JobObjectId = processExtension->JobObjectId;
		m->ProcessSequenceNumber = processExtension->ProcessSequenceNumber;
	}


	// Update the deltas.
	/*PhUpdateDelta(&processItem->CpuKernelDelta, process->KernelTime.QuadPart);
	PhUpdateDelta(&processItem->CpuUserDelta, process->UserTime.QuadPart);
	PhUpdateDelta(&processItem->IoReadDelta, process->ReadTransferCount.QuadPart);
	PhUpdateDelta(&processItem->IoWriteDelta, process->WriteTransferCount.QuadPart);
	PhUpdateDelta(&processItem->IoOtherDelta, process->OtherTransferCount.QuadPart);
	PhUpdateDelta(&processItem->IoReadCountDelta, process->ReadOperationCount.QuadPart);
	PhUpdateDelta(&processItem->IoWriteCountDelta, process->WriteOperationCount.QuadPart);
	PhUpdateDelta(&processItem->IoOtherCountDelta, process->OtherOperationCount.QuadPart);
	PhUpdateDelta(&processItem->ContextSwitchesDelta, contextSwitches);
	PhUpdateDelta(&processItem->PageFaultsDelta, process->PageFaultCount);
	PhUpdateDelta(&processItem->CycleTimeDelta, process->CycleTime);
	PhUpdateDelta(&processItem->PrivateBytesDelta, process->PagefileUsage);
	
	processItem->TimeSequenceNumber++;
    PhAddItemCircularBuffer_ULONG64(&processItem->IoReadHistory, processItem->IoReadDelta.Delta);
    PhAddItemCircularBuffer_ULONG64(&processItem->IoWriteHistory, processItem->IoWriteDelta.Delta);
    PhAddItemCircularBuffer_ULONG64(&processItem->IoOtherHistory, processItem->IoOtherDelta.Delta);

    PhAddItemCircularBuffer_SIZE_T(&processItem->PrivateBytesHistory, processItem->VmCounters.PagefileUsage);
    //PhAddItemCircularBuffer_SIZE_T(&processItem->WorkingSetHistory, processItem->VmCounters.WorkingSetSize);

    if (InterlockedExchange(&processItem->JustProcessed, 0) != 0)
        modified = TRUE;

    if (PhEnableCycleCpuUsage)
    {
        FLOAT totalDelta;

        newCpuUsage = (FLOAT)processItem->CycleTimeDelta.Delta / sysTotalCycleTime;

        // Calculate the kernel/user CPU usage based on the kernel/user time. If the kernel
        // and user deltas are both zero, we'll just have to use an estimate. Currently, we
        // split the CPU usage evenly across the kernel and user components, except when the
        // total user time is zero, in which case we assign it all to the kernel component.

        totalDelta = (FLOAT)(processItem->CpuKernelDelta.Delta + processItem->CpuUserDelta.Delta);

        if (totalDelta != 0)
        {
            kernelCpuUsage = newCpuUsage * ((FLOAT)processItem->CpuKernelDelta.Delta / totalDelta);
            userCpuUsage = newCpuUsage * ((FLOAT)processItem->CpuUserDelta.Delta / totalDelta);
        }
        else
        {
            if (processItem->UserTime.QuadPart != 0)
            {
                kernelCpuUsage = newCpuUsage / 2;
                userCpuUsage = newCpuUsage / 2;
            }
            else
            {
                kernelCpuUsage = newCpuUsage;
                userCpuUsage = 0;
            }
        }
    }
    else
    {
        kernelCpuUsage = (FLOAT)processItem->CpuKernelDelta.Delta / sysTotalTime;
        userCpuUsage = (FLOAT)processItem->CpuUserDelta.Delta / sysTotalTime;
        newCpuUsage = kernelCpuUsage + userCpuUsage;
    }

    processItem->CpuUsage = newCpuUsage;
    processItem->CpuKernelUsage = kernelCpuUsage;
    processItem->CpuUserUsage = userCpuUsage;

    PhAddItemCircularBuffer_FLOAT(&processItem->CpuKernelHistory, kernelCpuUsage);
    PhAddItemCircularBuffer_FLOAT(&processItem->CpuUserHistory, userCpuUsage);

    // Max. values

    if (processItem->ProcessId)
    {
        if (maxCpuValue < newCpuUsage)
        {
            maxCpuValue = newCpuUsage;
            maxCpuProcessItem = processItem;
        }

        // I/O for Other is not included because it is too generic.
        if (maxIoValue < processItem->IoReadDelta.Delta + processItem->IoWriteDelta.Delta)
        {
            maxIoValue = processItem->IoReadDelta.Delta + processItem->IoWriteDelta.Delta;
            maxIoProcessItem = processItem;
        }
    }
	*/

	if (m->AsyncFinished)
	{
		modified = TRUE;
		m->AsyncFinished = false;
	}

    // Token information
	if (m->QueryHandle && m->UniqueProcessId != SYSTEM_PROCESS_ID) // System token can't be opened (dmex)
	{
		// Note: this is done in UpdateDynamicData for non system processes
		HANDLE tokenHandle;

		if (NT_SUCCESS(PhOpenProcessToken(m->QueryHandle, TOKEN_QUERY, &tokenHandle)))
		{
			PTOKEN_USER tokenUser;
			TOKEN_ELEVATION_TYPE elevationType;
			MANDATORY_LEVEL integrityLevel;
			PWSTR integrityString;  // this will point to static stings so dont free it

			// User
			if (NT_SUCCESS(PhGetTokenUser(tokenHandle, &tokenUser)))
			{
				size_t sid_len = RtlLengthSid(tokenUser->User.Sid);
				if (m->Sid.size() != sid_len || !RtlEqualSid(m->Sid.data(), tokenUser->User.Sid))
				{
					m->Sid = QByteArray((char*)tokenUser->User.Sid, sid_len);
					m_UserName = GetSidFullNameCached(m->Sid/*, true*/);
					modified = TRUE;
				}
				PhFree(tokenUser);
			}

			// Elevation
			if (NT_SUCCESS(PhGetTokenElevationType(tokenHandle, &elevationType)))
			{
				if (m->ElevationType != elevationType)
				{
					m->ElevationType = elevationType;
					m->IsElevated = elevationType == TokenElevationTypeFull;
					modified = TRUE;
				}
			}

			// Integrity
			if (NT_SUCCESS(PhGetTokenIntegrityLevel(tokenHandle, &integrityLevel, &integrityString)))
			{
				if (m->IntegrityLevel != integrityLevel)
				{
					m->IntegrityLevel = integrityLevel;
					m->IntegrityString = QString::fromWCharArray(integrityString);
					modified = TRUE;
				}
			}

			NtClose(tokenHandle);
		}
	}

    // Job
    if (m->QueryHandle)
    {
        NTSTATUS status;
        BOOLEAN isInSignificantJob = FALSE;
        BOOLEAN isInJob = FALSE;

        if (KphIsConnected())
        {
            HANDLE jobHandle = NULL;

            status = KphOpenProcessJob(m->QueryHandle, JOB_OBJECT_QUERY, &jobHandle);

            if (NT_SUCCESS(status) && status != STATUS_PROCESS_NOT_IN_JOB)
            {
                JOBOBJECT_BASIC_LIMIT_INFORMATION basicLimits;

                isInJob = TRUE;

				// Process Explorer only recognizes processes as being in jobs if they don't have
				// the silent-breakaway-OK limit as their only limit. Emulate this behaviour.
                if (NT_SUCCESS(PhGetJobBasicLimits(jobHandle, &basicLimits)))
                {
                    isInSignificantJob = basicLimits.LimitFlags != JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
                }
            }

            if (jobHandle)
                NtClose(jobHandle);
        }
        else
        {
            status = NtIsProcessInJob(m->QueryHandle, NULL);

            if (NT_SUCCESS(status))
                isInJob = status == STATUS_PROCESS_IN_JOB;
        }

        if (m->IsInSignificantJob != isInSignificantJob)
        {
            m->IsInSignificantJob = isInSignificantJob;
            modified = TRUE;
        }

        if (m->IsInJob != isInJob)
        {
            m->IsInJob = isInJob;
            modified = TRUE;
        }
    }

    // Debugged
    if (m->QueryHandle && !m->IsSubsystemProcess && !m->IsProtectedHandle)
    {
        BOOLEAN isBeingDebugged = FALSE;

        PhGetProcessIsBeingDebugged(m->QueryHandle, &isBeingDebugged);

        if (m->IsBeingDebugged != isBeingDebugged)
        {
			m->IsBeingDebugged = isBeingDebugged;
            modified = TRUE;
        }
    }

    // Suspended
    if (m->IsSuspended != isSuspended)
    {
        m->IsSuspended = isSuspended;
        modified = TRUE;
    }

    m->IsPartiallySuspended = isPartiallySuspended;

    // .NET
	// Since we dont support pluginy (yet) so this is not needed for now
    /*if (m->UpdateIsDotNet) 
    {
        BOOLEAN isDotNet;
        ULONG flags = 0;

        if (NT_SUCCESS(PhGetProcessIsDotNetEx(m->UniqueProcessId, NULL, 0, &isDotNet, &flags)))
        {
            m->IsDotNet = isDotNet;

            // This check is needed for the DotNetTools plugin. (dmex)
            if (!isDotNet && (flags & PH_CLR_JIT_PRESENT))
                m->IsDotNet = TRUE;

            modified = TRUE;
        }

        m->UpdateIsDotNet = FALSE;
    }*/

	// Note: The immersive state of a process can never change. No need to update it like Process Hacker does

	// GDI, USER handles
	if (m->QueryHandle)
    {
        m_GdiHandles = GetGuiResources(m->QueryHandle, GR_GDIOBJECTS);
        m_UserHandles = GetGuiResources(m->QueryHandle, GR_USEROBJECTS);
    }
    else
    {
        m_GdiHandles = 0;
        m_UserHandles = 0;
    }

	return modified;
}

void CWinProcess::UnInit()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->QueryHandle != NULL) {
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}
}


void CWinProcess::InitAsyncData()
{
	QReadLocker Locker(&m_Mutex);

	QVariantMap Params;
	Params["FileName"] = m_FileName;
	Params["PackageFullName"] = m->PackageFullName;
	Params["IsSubsystemProcess"] = (bool)m->IsSubsystemProcess;

	/*if (m_UserName.isEmpty())
		Params["Sid"] = m->Sid;*/

	// Note: this instance of CWinProcess may be deleted before the async proces finishes,
	// so to make things simple and avoid emmory leaks we pass all params and results as a QVariantMap
	// its not the most eficient way but its simple and reliable.

	QFutureWatcher<QVariantMap>* pWatcher = new QFutureWatcher<QVariantMap>(this); // Note: the job will be canceled if the file will be deleted :D
	connect(pWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(OnInitAsyncData(int)));
	connect(pWatcher, SIGNAL(finished()), pWatcher, SLOT(deleteLater()));
	pWatcher->setFuture(QtConcurrent::run(CWinProcess::InitAsyncData, Params));
}

// Note: PhInitializeImageVersionInfoCached does not look thread safe, so we have to guard it.
QMutex g_ImageVersionInfoCachedMutex;

QVariantMap CWinProcess::InitAsyncData(QVariantMap Params)
{
	QVariantMap Result;

	PPH_STRING FileName = CastQString(Params["FileName"].toString());
	PPH_STRING PackageFullName = CastQString(Params["PackageFullName"].toString());
	BOOLEAN IsSubsystemProcess = Params["IsSubsystemProcess"].toBool();

	PH_IMAGE_VERSION_INFO VersionInfo = { NULL, NULL, NULL, NULL };

	// PhpProcessQueryStage1 Begin
	NTSTATUS status;

	if (FileName && !IsSubsystemProcess)
	{
		HICON SmallIcon;
		HICON LargeIcon;
		if (!PhExtractIcon(FileName->Buffer, &LargeIcon, &SmallIcon))
		{
			LargeIcon = NULL;
			SmallIcon = NULL;
		}

		if (SmallIcon)
		{
			Result["SmallIcon"] = QtWin::fromHICON(SmallIcon);
			DestroyIcon(SmallIcon);
		}

		if (LargeIcon)
		{
			Result["LargeIcon"] = QtWin::fromHICON(LargeIcon);
			DestroyIcon(LargeIcon);
		}

		// Version info.
		QMutexLocker Lock(&g_ImageVersionInfoCachedMutex);
		PhInitializeImageVersionInfoCached(&VersionInfo, FileName, FALSE);
	}

	/*if (Params.contains("Sid"))
		Result["UserName"] = GetSidFullNameCached(Params["Sid"].toByteArray());*/
	// PhpProcessQueryStage1 End

	// PhpProcessQueryStage2 Begin
	if (FileName && !IsSubsystemProcess)
	{
		NTSTATUS status;

		VERIFY_RESULT VerifyResult;
		PPH_STRING VerifySignerName;
		ulong ImportFunctions;
		ulong ImportModules;

		BOOLEAN IsPacked;

		VerifyResult = PhVerifyFileCached(FileName, PackageFullName, &VerifySignerName, FALSE);


		status = PhIsExecutablePacked(FileName->Buffer, &IsPacked, &ImportModules, &ImportFunctions);

		// If we got an image-related error, the image is packed.
		if (status == STATUS_INVALID_IMAGE_NOT_MZ || status == STATUS_INVALID_IMAGE_FORMAT || status == STATUS_ACCESS_VIOLATION)
		{
			IsPacked = TRUE;
			ImportModules = ULONG_MAX;
			ImportFunctions = ULONG_MAX;
		}

		Result["VerifyResult"] = (int)VerifyResult;
		Result["VerifySignerName"] = CastPhString(VerifySignerName);
		Result["IsPacked"] = IsPacked;
		Result["ImportFunctions"] = (quint32)ImportFunctions;
		Result["ImportModules"] = (quint32)ImportModules;
	}

	if (/*PhEnableLinuxSubsystemSupport &&*/ FileName && IsSubsystemProcess)
	{
		QMutexLocker Lock(&g_ImageVersionInfoCachedMutex);
		PhInitializeImageVersionInfoCached(&VersionInfo, FileName, TRUE);
	}
	// PhpProcessQueryStage2 End

	Result["CompanyName"] = CastPhString(VersionInfo.CompanyName);
	Result["FileDescription"] = CastPhString(VersionInfo.FileDescription);
	Result["FileVersion"] = CastPhString(VersionInfo.FileVersion);
	Result["ProductName"] = CastPhString(VersionInfo.ProductName);

	PhDereferenceObject(FileName);
	PhDereferenceObject(PackageFullName);

	return Result;
}

void CWinProcess::OnInitAsyncData(int Index)
{
	QFutureWatcher<QVariantMap>* pWatcher = (QFutureWatcher<QVariantMap>*)sender();

	QVariantMap Result = pWatcher->resultAt(Index);

	QWriteLocker Locker(&m_Mutex);

	/*if (Result.contains("UserName"))
		m_UserName = Result["UserName"].toString();*/

	m_SmallIcon = Result["SmallIcon"].value<QPixmap>();
	m_LargeIcon = Result["LargeIcon"].value<QPixmap>();

	m->VersionInfo.CompanyName = Result["CompanyName"].toString();
	m->VersionInfo.FileDescription = Result["FileDescription"].toString();
	m->VersionInfo.FileVersion = Result["FileVersion"].toString();
	m->VersionInfo.ProductName = Result["ProductName"].toString();

	m->VerifyResult = (VERIFY_RESULT)Result["VerifyResult"].toInt();
	m->VerifySignerName = Result["VerifySignerName"].toString();
	m->IsPacked = Result["IsPacked"].toBool();
	m->ImportFunctions = Result["ImportFunctions"].toUInt();
	m->ImportModules = Result["ImportModules"].toUInt();

	m->AsyncFinished = true;
}

// Flags
bool CWinProcess::IsSubsystemProcess() const
{
	QReadLocker Locker(&m_Mutex);
	return (int)m->IsSecureProcess;
}

// File
QString CWinProcess::GetDescription() const
{
	QReadLocker Locker(&m_Mutex);
	return m->VersionInfo.FileDescription;
}

// GDI, USER handles
int CWinProcess::IntegrityLevel() const
{
	QReadLocker Locker(&m_Mutex);
	return (int)m->IntegrityLevel;
}

QString CWinProcess::GetIntegrityString() const
{
	QReadLocker Locker(&m_Mutex);
	return m->IntegrityString;
}


// OS context
ulong CWinProcess::GetOsContextVersion() const
{
	QReadLocker Locker(&m_Mutex);
	return m->OsContextVersion;
}

QString CWinProcess::GetOsContextString() const
{
	QReadLocker Locker(&m_Mutex);
    switch (m->OsContextVersion)
    {
    case WINDOWS_10:	return tr("Windows 10");
    case WINDOWS_8_1:	return tr("Windows 8.1");
    case WINDOWS_8:		return tr("Windows 8");
    case WINDOWS_7:		return tr("Windows 7");
    case WINDOWS_VISTA:	return tr("Windows Vista");
    case WINDOWS_XP:	return tr("Windows XP");
	default: return "";
    }
}