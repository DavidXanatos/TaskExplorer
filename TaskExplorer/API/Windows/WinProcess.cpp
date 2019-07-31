/*
 * Process Hacker -
 *   qt wrapper and support functions based on procprv.c
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
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
#include "ProcessHacker.h"
#include "ProcessHacker/ProcMtgn.h"
#include <lsasup.h>
#include <userenv.h>

#include "../../GUI/TaskExplorer.h"

#include "WindowsAPI.h"
#include "WinProcess.h"
#include "WinThread.h"
#include "WinHandle.h"
#include "WinModule.h"
#include "WinWnd.h"
#include "ProcessHacker/memprv.h"
#include "../Common/Common.h"
#include "../../SVC/TaskService.h"

 // CWinProcess private members

struct SWinProcess
{
	SWinProcess()
	{
		UniqueProcessId = NULL;
		QueryHandle = NULL;

		SessionId = -1;
		CreateTime.QuadPart = 0;

		ImageTimeDateStamp = 0;
		ImageCharacteristics = 0;
		ImageReserved = 0;
		ImageSubsystem = 0;
		ImageDllCharacteristics = 0;
		PebBaseAddress = 0;
		PebBaseAddress32 = 0;

		Flags = NULL;
		AsyncFinished = false;
		OsContextVersion = 0;
		DepStatus = 0;
		DpiAwareness = -1;

		memset(&VmCounters, 0, sizeof(VM_COUNTERS_EX));
		memset(&IoCounters, 0, sizeof(IO_COUNTERS));
		memset(&WsCounters, 0, sizeof(PH_PROCESS_WS_COUNTERS));
		memset(&QuotaLimits, 0, sizeof(QUOTA_LIMITS));
	}

	
	// Handles
	HANDLE UniqueProcessId;
	HANDLE QueryHandle;

	// Basic
	quint64 SessionId;
	LARGE_INTEGER CreateTime;

	// Image
    ULONG ImageTimeDateStamp;
    USHORT ImageCharacteristics;
    USHORT ImageReserved;
    USHORT ImageSubsystem;
    USHORT ImageDllCharacteristics;
	quint64 PebBaseAddress;
	quint64 PebBaseAddress32;

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

	// Other
	HANDLE ConsoleHostProcessId;

	// Dynamic
	VM_COUNTERS_EX VmCounters;
	IO_COUNTERS IoCounters;
    PH_PROCESS_WS_COUNTERS WsCounters;
	QUOTA_LIMITS QuotaLimits;

	//PROCESS_HANDLE_INFORMATION handleInfo;
	//PROCESS_UPTIME_INFORMATION uptimeInfo;

	// Other Fields
	PH_KNOWN_PROCESS_TYPE KnownProcessType;
	PS_PROTECTION Protection;
	quint64 ProcessSequenceNumber;
	ulong JobObjectId;
	QString PackageFullName;
	QString AppID;
	ULONG DpiAwareness;

	// Signature, Packed
	ulong ImportFunctions;
	ulong ImportModules;

	// OS Context
    GUID OsContextGuid;
    ulong OsContextVersion;

	// Misc.
	ULONG DepStatus;

	QString DesktopInfo;
};


// CWinProcess Class members

CWinProcess::CWinProcess(QObject *parent) : CProcessInfo(parent)
{
	// Dynamic

	// GDI, USER handles
	m_GdiHandles = 0;
    m_UserHandles = 0;
	m_WndHandles = 0;

	m_IsCritical = false;

	m_pModuleInfo = CModulePtr(new CWinModule());
	connect(m_pModuleInfo.data(), SIGNAL(AsyncDataDone(bool, ulong, ulong)), this, SLOT(OnAsyncDataDone(bool, ulong, ulong)));

	m_LastUpdateHandles = 0;

	m_lastExtUpdate = 0;

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
	m_ProcessId = (quint64)m->UniqueProcessId;

	// PhpFillProcessItem Begin
	m_ParentProcessId = (quint64)Process->InheritedFromUniqueProcessId;
	m->SessionId = (quint64)Process->SessionId;

	if (m_ProcessId != (quint64)SYSTEM_IDLE_PROCESS_ID)
		m_ProcessName = QString::fromWCharArray(Process->ImageName.Buffer, Process->ImageName.Length / sizeof(wchar_t));
	else
		m_ProcessName = QString::fromWCharArray(SYSTEM_IDLE_PROCESS_NAME);

	m->CreateTime = Process->CreateTime;
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);

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
		// kernel file name. Otherwise, get the Module file name. (wj32)
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
	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID) 
	{
		m_pToken = CWinTokenPtr(CWinToken::NewSystemToken()); // System token can't be opened (dmex)
	}
	else if (m->QueryHandle) 
	{
		m_pToken = CWinTokenPtr(new CWinToken());
		m_pToken->InitStaticData(m->QueryHandle);
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
	*/

    // Initialize the deltas.
	// not needed deltas are self initializing

	/*
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
            */

    // Add pending service items to the process item.
    
	m_ServiceList = ((CWindowsAPI*)theAPI)->GetServicesByPID(m_ProcessId);

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
        m->IsImmersive = !!::IsImmersiveProcess(m->QueryHandle);
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

	// subsystem
	if (m->IsSubsystemProcess)
    {
        m->ImageSubsystem = IMAGE_SUBSYSTEM_POSIX_CUI;
    }
    else if(m->QueryHandle)
    {
        PROCESS_BASIC_INFORMATION basicInfo;
        if (NT_SUCCESS(PhGetProcessBasicInformation(m->QueryHandle, &basicInfo)) && basicInfo.PebBaseAddress != 0)
        {
			m->PebBaseAddress = (quint64)basicInfo.PebBaseAddress;
			if (m->IsWow64)
			{
				PVOID peb32;
				PhGetProcessPeb32(m->QueryHandle, &peb32);
				m->PebBaseAddress32 = (quint64)peb32;
			}

			if (m->IsHandleValid)
			{
				PVOID imageBaseAddress;
				PH_REMOTE_MAPPED_IMAGE mappedImage;
                if (NT_SUCCESS(NtReadVirtualMemory(m->QueryHandle, PTR_ADD_OFFSET(basicInfo.PebBaseAddress, FIELD_OFFSET(PEB, ImageBaseAddress)), &imageBaseAddress, sizeof(PVOID), NULL )))
                {
                    if (NT_SUCCESS(PhLoadRemoteMappedImage(m->QueryHandle, imageBaseAddress, &mappedImage)))
                    {
                        m->ImageTimeDateStamp = mappedImage.NtHeaders->FileHeader.TimeDateStamp;
                        m->ImageCharacteristics = mappedImage.NtHeaders->FileHeader.Characteristics;

                        if (mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
                        {
                            m->ImageSubsystem = ((PIMAGE_OPTIONAL_HEADER32)&mappedImage.NtHeaders->OptionalHeader)->Subsystem;
                            m->ImageDllCharacteristics = ((PIMAGE_OPTIONAL_HEADER32)&mappedImage.NtHeaders->OptionalHeader)->DllCharacteristics;
                        }
                        else if (mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC)
                        {
                            m->ImageSubsystem = ((PIMAGE_OPTIONAL_HEADER64)&mappedImage.NtHeaders->OptionalHeader)->Subsystem;
                            m->ImageDllCharacteristics = ((PIMAGE_OPTIONAL_HEADER64)&mappedImage.NtHeaders->OptionalHeader)->DllCharacteristics;
                        }

                        PhUnloadRemoteMappedImage(&mappedImage);
                    }
                }
            }
        }
    }

	// desktop
	if (m->IsHandleValid)
	{
		PPH_STRING desktopinfo;
		if (NT_SUCCESS(PhGetProcessDesktopInfo(m->QueryHandle, &desktopinfo)))
		{
			m->DesktopInfo = CastPhString(desktopinfo);
		}
	}

	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == DPCS_PROCESS_ID || m->UniqueProcessId == INTERRUPTS_PROCESS_ID)
		return true;

	qobject_cast<CWinModule*>(m_pModuleInfo)->SetSubsystemProcess(m->IsSubsystemProcess);
	qobject_cast<CWinModule*>(m_pModuleInfo)->InitAsyncData(m_FileName, m->PackageFullName);

	return true;
}

void CWinProcess::OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules)
{
	m->IsPacked = IsPacked;
	m->ImportFunctions = ImportFunctions;
	m->ImportModules = ImportModules;
	m->AsyncFinished = true;
}

bool CWinProcess::UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* Process, quint64 sysTotalTime, quint64 sysTotalCycleTime)
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

	if (m->IsHandleValid)
	{

		int pebOffset = PhpoCurrentDirectory;
#ifdef _WIN64
        // Tell the function to get the WOW64 current directory, because that's the one that
        // actually gets updated.
        if (m->IsWow64)
            pebOffset |= PhpoWow64;
#endif

		PPH_STRING curDir = NULL;
        PhGetProcessPebString(m->QueryHandle, (PH_PEB_OFFSET)pebOffset, &curDir);
		m_WorkingDirectory = CastPhString(curDir);
	}

	// PhpUpdateDynamicInfoProcessItem Begin
	m_BasePriority = Process->BasePriority;

	if (m->QueryHandle)
	{
		PROCESS_PRIORITY_CLASS PriorityClass;
		if (NT_SUCCESS(PhGetProcessPriority(m->QueryHandle, &PriorityClass)))
			m_Priority = PriorityClass.PriorityClass;

		IO_PRIORITY_HINT IoPriority;
		if (NT_SUCCESS(PhGetProcessIoPriority(m->QueryHandle, &IoPriority)))
			m_IOPriority = IoPriority;
                
		ULONG PagePriority;
		if (NT_SUCCESS(PhGetProcessPagePriority(m->QueryHandle, &PagePriority)))
			m_PagePriority = PagePriority;

        PROCESS_BASIC_INFORMATION basicInfo;
		if (NT_SUCCESS(PhGetProcessBasicInformation(m->QueryHandle, &basicInfo)))
			m_AffinityMask = basicInfo.AffinityMask;

		// todo modifyed
	}
	else
	{
		m_Priority = 0;
		m_IOPriority = 0;
		m_PagePriority = 0;
	}

	// update critical flag
	{
		BOOLEAN breakOnTermination;
		if (NT_SUCCESS(PhGetProcessBreakOnTermination(m->QueryHandle, &breakOnTermination)))
		{
			if ((bool)breakOnTermination != m_IsCritical)
			{
				m_IsCritical = breakOnTermination;
				modified = TRUE;
			}
		}
	}


	m_KernelTime = Process->KernelTime.QuadPart;
	m_UserTime = Process->UserTime.QuadPart;
	m_NumberOfHandles = Process->HandleCount;
	m_NumberOfThreads = Process->NumberOfThreads;
	m_WorkingSetPrivateSize = Process->WorkingSetPrivateSize.QuadPart;
	m_PeakNumberOfThreads = Process->NumberOfThreadsHighWatermark;
	m_HardFaultCount = Process->HardFaultCount;

	// todo modifyed

	// todo modifyed

	// todo modifyed

	// Update VM and I/O counters.
	m->VmCounters = *(PVM_COUNTERS_EX)&Process->PeakVirtualSize;
	m->IoCounters = *(PIO_COUNTERS)&Process->ReadOperationCount;
	// PhpUpdateDynamicInfoProcessItem End


	// PhpFillProcessItemExtension
	PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension;
	if (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64() && PH_IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		processExtension = PH_PROCESS_EXTENSION(Process);

		m->JobObjectId = processExtension->JobObjectId;
		m->ProcessSequenceNumber = processExtension->ProcessSequenceNumber;
	}

	if (m->AsyncFinished)
	{
		modified = TRUE;
		m->AsyncFinished = false;
	}

	if (m->QueryHandle && m->UniqueProcessId != SYSTEM_PROCESS_ID) // System token can't be opened (dmex)
	{
		if (m_pToken && m_pToken->UpdateDynamicData())
			modified = TRUE;
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



	QWriteLocker StatsLocker(&m_StatsMutex);

	// Update the deltas.
	m_CpuStats.CpuKernelDelta.Update(Process->KernelTime.QuadPart);
	m_CpuStats.CpuUserDelta.Update(Process->UserTime.QuadPart);
	m_CpuStats.CycleDelta.Update(Process->CycleTime);

	m_CpuStats.ContextSwitchesDelta.Update(contextSwitches);
	m_CpuStats.PageFaultsDelta.Update(Process->PageFaultCount);
	m_CpuStats.PrivateBytesDelta.Update(Process->PagefileUsage);

	m_CpuStats.UpdateStats(sysTotalTime, sysTotalCycleTime);

    /*PhAddItemCircularBuffer_FLOAT(&processItem->CpuKernelHistory, kernelCpuUsage);
    PhAddItemCircularBuffer_FLOAT(&processItem->CpuUserHistory, userCpuUsage);
	*/


	m_Stats.Io.SetRead(Process->ReadTransferCount.QuadPart, Process->ReadOperationCount.QuadPart);
	m_Stats.Io.SetWrite(Process->WriteTransferCount.QuadPart, Process->WriteOperationCount.QuadPart);
	m_Stats.Io.SetOther(Process->OtherTransferCount.QuadPart, Process->OtherOperationCount.QuadPart);

	m_Stats.UpdateStats();

	return modified;
}

bool CWinProcess::UpdateDynamicDataExt()
{
	m_lastExtUpdate = GetCurTick();

	// Note: this is to slow to be pooled always, query only if needed

	if (!m->QueryHandle)
		return false;
	
	PhGetProcessWsCounters(m->QueryHandle, &m->WsCounters);

	PhGetProcessQuotaLimits(m->QueryHandle, &m->QuotaLimits);

	
	/*PhGetProcessHandleCount(m->QueryHandle, &m->handleInfo);

	// todo is that slow do we need this?
    if (WindowsVersion >= WINDOWS_10_RS3 && NT_SUCCESS(PhGetProcessUptime(m->QueryHandle, &m->uptimeInfo)))
    {
        runningTime = uptimeInfo.Uptime;
        suspendedTime = uptimeInfo.SuspendedTime;
        hangCount = uptimeInfo.HangCount;
        ghostCount = uptimeInfo.GhostCount;
        gotUptime = TRUE;
    }
	*/

	return true;
}

void CWinProcess::UpdateExtDataIfNeeded() const 
{
	if (GetCurTick() - m_lastExtUpdate < 1000)
		return;
	((CWinProcess*)this)->UpdateDynamicDataExt();
}

bool CWinProcess::UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* Process, quint64 sysTotalTime, quint64 sysTotalCycleTime)
{
    // System Idle Process has one thread per CPU. They all have a TID of 0. We can't have duplicate
    // TIDs, so we'll assign unique TIDs.
    if (Process->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID)
    {
        for (int i = 0; i < Process->NumberOfThreads; i++)
            Process->Threads[i].ClientId.UniqueThread = UlongToHandle(i);
    }

	// todo changed removed etc events?

	QMap<quint64, CThreadPtr> OldThreads = GetThreadList();

	bool HaveFirst = OldThreads.count() > 0;

	// handle threads
	for (int i = 0; i < Process->NumberOfThreads; i++)
	{
		_SYSTEM_THREAD_INFORMATION* Thread = &Process->Threads[i];

		quint64 ThreadID = (quint64)Thread->ClientId.UniqueThread;

		QSharedPointer<CWinThread> pWinThread = OldThreads.take(ThreadID).objectCast<CWinThread>();
		if (pWinThread.isNull())
		{
			pWinThread = QSharedPointer<CWinThread>(new CWinThread());
			pWinThread->InitStaticData(&m->QueryHandle, Thread);
			theAPI->AddThread(pWinThread);
			if (!HaveFirst)
			{
				// Sometimes an application exits its first thread leaving other threads running (DX)
				if (pWinThread->GetRawCreateTime() - m->CreateTime.QuadPart < 1000)
				{
					pWinThread->SetMainThread();
					HaveFirst = true;
				}
			}
			QWriteLocker Locker(&m_ThreadMutex);
			ASSERT(!m_ThreadList.contains(ThreadID));
			m_ThreadList.insert(ThreadID, pWinThread);
		}

		pWinThread->UpdateDynamicData(Thread, sysTotalTime, sysTotalCycleTime);
	}

	//if (!HaveFirst)
	//	qDebug() << "No Main ThreadIn:" << m_ProcessName;

	QWriteLocker Locker(&m_HandleMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 ThreadID, OldThreads.keys())
	{
		QSharedPointer<CWinThread> pWinThread = m_ThreadList.value(ThreadID).objectCast<CWinThread>();
		if (pWinThread->CanBeRemoved())
		{
			m_ThreadList.remove(ThreadID);
			//Removed.insert(ThreadID); 
		}
		else if (!pWinThread->IsMarkedForRemoval())
		{
			pWinThread->MarkForRemoval();
			pWinThread->UnInit();
			//Changed.insert(ThreadID); 
		}
	}

	return true;
}

bool CWinProcess::UpdateThreads()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	// Note: On windows UpdateThreads updates only minor extended informations, all major task informations are updated during UpdateDynamicData.

	QMap<quint64, CThreadPtr> Threads = GetThreadList();
	foreach(const CThreadPtr& pThread, Threads)
	{
		QSharedPointer<CWinThread> pWinThread = pThread.objectCast<CWinThread>();
		pWinThread->UpdateExtendedData();
	}

	emit ThreadsUpdated(Added, Changed, Removed);

	return true;
}

void CWinProcess::UnInit()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->QueryHandle != NULL) {
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}
}

NTSTATUS PhEnumHandlesGeneric(_In_ HANDLE ProcessId, _In_ HANDLE ProcessHandle, _Out_ PSYSTEM_HANDLE_INFORMATION_EX *Handles, _Out_ PBOOLEAN FilterNeeded);

bool CWinProcess::UpdateHandles()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	HANDLE ProcessHandle;
    PSYSTEM_HANDLE_INFORMATION_EX handleInfo;
    BOOLEAN filterNeeded;

	if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_DUP_HANDLE, m->UniqueProcessId)))
	{
		emit HandlesUpdated(Added, Changed, Removed);
		return false;
	}

	if (!NT_SUCCESS(PhEnumHandlesGeneric(m->UniqueProcessId, ProcessHandle, &handleInfo, &filterNeeded))) {
		NtClose(ProcessHandle);
		emit HandlesUpdated(Added, Changed, Removed);
		return false;
	}

	// Note: since we are not continusly monitoring all handles for all processes 
	//			when updating them on demand we waht to prevent all aprearing as new once a proces got selected
	quint64 TimeStamp = (GetCurTick() - m_LastUpdateHandles < 5000) ? GetTime() * 1000 : m_CreateTimeStamp;

	// Copy the handle Map
	QMap<quint64, CHandlePtr> OldHandles = GetHandleList();

	BOOLEAN useWorkQueue = !KphIsConnected();
	QList<QFutureWatcher<bool>*> Watchers;

	for (int i = 0; i < handleInfo->NumberOfHandles; i++)
	{
		PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = &handleInfo->Handles[i];

		// Skip irrelevant handles.
		if (filterNeeded && handle->UniqueProcessId != (ULONG_PTR)m->UniqueProcessId)
			continue;

		QSharedPointer<CWinHandle> pWinHandle = OldHandles.take(handle->HandleValue).objectCast<CWinHandle>();

		bool WasReused = false;
		// Also compare the object pointers to make sure a
        // different object wasn't re-opened with the same
        // handle value. This isn't 100% accurate as pool
        // addresses may be re-used, but it works well.
		if (handle->Object && !pWinHandle.isNull() && (quint64)handle->Object != pWinHandle->GetObjectAddress())
			WasReused = true;

		bool bAdd = false;
		if (pWinHandle.isNull())
		{
			pWinHandle = QSharedPointer<CWinHandle>(new CWinHandle());
			bAdd = true;
			QWriteLocker Locker(&m_HandleMutex);
			ASSERT(!m_HandleList.contains(handle->HandleValue));
			m_HandleList.insert(handle->HandleValue, pWinHandle);
		}
		
		if (WasReused)
			Removed.insert(handle->HandleValue);

		if (bAdd || WasReused)
		{
			pWinHandle->InitStaticData(handle, TimeStamp);
			Added.insert(handle->HandleValue);

			// When we don't have KPH, query handle information in parallel to take full advantage of the
            // PhCallWithTimeout functionality.
            if (useWorkQueue && handle->ObjectTypeIndex == g_fileObjectTypeIndex)
				Watchers.append(pWinHandle->InitExtDataAsync(handle,(quint64)ProcessHandle));
			else
				pWinHandle->InitExtData(handle,(quint64)ProcessHandle);	
		}
		// ToDo: do we want to start it right away and dont wait?
		else if (pWinHandle->UpdateDynamicData(handle,(quint64)ProcessHandle))
			Changed.insert(handle->HandleValue);
	}

	// wait for all watchers to finish
	while (!Watchers.isEmpty())
	{
		QFutureWatcher<bool>* pWatcher = Watchers.takeFirst();
		pWatcher->waitForFinished();
		pWatcher->deleteLater();
	}

	QWriteLocker Locker(&m_HandleMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 HandleID, OldHandles.keys())
	{
		QSharedPointer<CWinHandle> pWinHandle = OldHandles.value(HandleID).objectCast<CWinHandle>();
		if (pWinHandle->CanBeRemoved())
		{
			m_HandleList.remove(HandleID);
			Removed.insert(HandleID);
		}
		else if (!pWinHandle->IsMarkedForRemoval())
		{
			pWinHandle->MarkForRemoval();
			Changed.insert(HandleID); 
		}
	}
	Locker.unlock();
	
	NtClose(ProcessHandle);
    
	m_LastUpdateHandles = GetCurTick();

	emit HandlesUpdated(Added, Changed, Removed);

	return true;
}

static BOOLEAN NTAPI EnumModulesCallback(_In_ PPH_MODULE_INFO Module, _In_opt_ PVOID Context)
{
    PPH_MODULE_INFO copy;

    copy = (PPH_MODULE_INFO)PhAllocateCopy(Module, sizeof(PH_MODULE_INFO));
    PhReferenceObject(copy->Name);
    PhReferenceObject(copy->FileName);

    PhAddItemList((PPH_LIST)Context, copy);

    return TRUE;
}

/*QMultiMap<QString, CModulePtr>::iterator FindModuleEntry(QMultiMap<QString, CModulePtr> &Modules, const QString& FileName, quint64 BaseAddress)
{
	for (QMultiMap<QString, CModulePtr>::iterator I = Modules.find(FileName); I != Modules.end() && I.key() == FileName; I++)
	{
		if (I.value().objectCast<CWinModule>()->GetBaseAddress() == BaseAddress)
			return I;
	}

	// no matching entry
	return Modules.end();
}*/

bool CWinProcess::UpdateModules()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	// If we didn't get a handle when we created the provider,
    // abort (unless this is the System process - in that case
    // we don't need a handle).
    if (!m->QueryHandle && m->UniqueProcessId != SYSTEM_PROCESS_ID)
        return false;

	PPH_LIST modules;
	modules = PhCreateList(100);

	NTSTATUS runSttus = PhEnumGenericModules(m->UniqueProcessId, m->QueryHandle, PH_ENUM_GENERIC_MAPPED_FILES | PH_ENUM_GENERIC_MAPPED_IMAGES, EnumModulesCallback, modules);

	QMap<quint64, CModulePtr> OldModules = GetModuleList();

	bool HaveFirst = OldModules.count() > 0;
	//quint64 FirstBaseAddress = 0;

	// Look for new modules.
	for (ulong i = 0; i < modules->Count; i++)
	{
		PPH_MODULE_INFO module = (PPH_MODULE_INFO)modules->Items[i];

		quint64 BaseAddress = (quint64)module->BaseAddress;
		QString FileName = CastPhString(module->FileName, false);


		QSharedPointer<CWinModule> pModule;

		bool bAdd = false;
		//QMap<quint64, CModulePtr>::iterator I = FindModuleEntry(OldModules, FileName, BaseAddress);
		QMap<quint64, CModulePtr>::iterator I = OldModules.find(BaseAddress);
		if (I == OldModules.end())
		{
			pModule = QSharedPointer<CWinModule>(new CWinModule(m_ProcessId, m->IsSubsystemProcess));
			bAdd = pModule->InitStaticData(module, (quint64)m->QueryHandle);
			if (pModule->GetType() != PH_MODULE_TYPE_ELF_MAPPED_IMAGE)
				pModule->InitAsyncData(pModule->GetFileName());

			// remove CF Guard flag if CFG mitigation is not enabled for the process
			if (!m->IsControlFlowGuardEnabled)
				pModule->ClearControlFlowGuardEnabled();

			if (!HaveFirst)
			{
				if (WindowsVersion < WINDOWS_10)
				{
					pModule->SetFirst(true);
					HaveFirst = true;
				}
				else
				{
					// Windows loads the PE image first and WSL loads the ELF image last (dmex)
					if (!m_FileName.isEmpty() && m_FileName.compare(pModule->GetFileName(),Qt::CaseInsensitive) == 0)
					{
						pModule->SetFirst(true);
						HaveFirst = true;
					}
				}
			}

			QWriteLocker Locker(&m_ModuleMutex);
			ASSERT(!m_ModuleList.contains(pModule->GetBaseAddress()));
			m_ModuleList.insert(pModule->GetBaseAddress(), pModule);
		}
		else
		{
			pModule = I.value().objectCast<CWinModule>();
			OldModules.erase(I);
		}

		//if (!FirstBaseAddress && pModule->IsFirst())
		//	FirstBaseAddress = pModule->GetBaseAddress();

		bool bChanged = false;
		bChanged = pModule->UpdateDynamicData(module);

		if (bAdd)
			Added.insert(pModule->GetBaseAddress());
		else if (bChanged)
			Changed.insert(pModule->GetBaseAddress());
	}

    for (ulong i = 0; i < modules->Count; i++)
    {
        PPH_MODULE_INFO module = (PPH_MODULE_INFO)modules->Items[i];

        PhDereferenceObject(module->Name);
        PhDereferenceObject(module->FileName);
        PhFree(module);
    }

    PhDereferenceObject(modules);

	QWriteLocker Locker(&m_ModuleMutex);
	// purle all modules left as thay are not longer valid
	foreach(quint64 BaseAddress, OldModules.keys())
	{
		m_ModuleList.remove(BaseAddress);
		Removed.insert(BaseAddress);
	}

	/*foreach(const CModulePtr& pModule, m_ModuleList)
	{
		if (pModule->GetParentBaseAddress() == 0)
			pModule->SetParentBaseAddress(FirstBaseAddress);
	}*/
	Locker.unlock();

	emit ModulesUpdated(Added, Changed, Removed);

	return true;
}

bool CWinProcess::UpdateWindows()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CWndPtr> OldWindows = GetWindowList();

	QMultiMap<quint64, CWindowsAPI::SWndInfo> Windows = ((CWindowsAPI*)theAPI)->GetWindowByPID(m_ProcessId);

	for (QMultiMap<quint64, CWindowsAPI::SWndInfo>::iterator I = Windows.begin(); I != Windows.end(); I++)
	{
		quint64 ThreadID = I.key();
		CWindowsAPI::SWndInfo WndInfo = I.value();
		QSharedPointer<CWinWnd> pWinWnd = OldWindows.take(WndInfo.hwnd).objectCast<CWinWnd>();

		bool bAdd = false;
		if (pWinWnd.isNull())
		{
			pWinWnd = QSharedPointer<CWinWnd>(new CWinWnd());
			pWinWnd->InitStaticData(WndInfo, m->QueryHandle);
			bAdd = true;
			QWriteLocker Locker(&m_WindowMutex);
			ASSERT(!m_WindowList.contains(WndInfo.hwnd));
			m_WindowList.insert(WndInfo.hwnd, pWinWnd);
		}

		bool bChanged = false;
		bChanged = pWinWnd->UpdateDynamicData();
			
		if (bAdd)
			Added.insert(WndInfo.hwnd);
		else if (bChanged)
			Changed.insert(WndInfo.hwnd);
	}

	QWriteLocker Locker(&m_WindowMutex);
	foreach(quint64 hwnd, OldWindows.keys())
	{
		m_WindowList.remove(hwnd);
		Removed.insert(hwnd);
	}
	Locker.unlock();

	emit WindowsUpdated(Added, Changed, Removed);

	return true;
}

void CWinProcess::AddNetworkIO(int Type, ulong TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtEtwNetworkReceiveType:	m_Stats.Net.AddReceive(TransferSize); break;
	case EtEtwNetworkSendType:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

void CWinProcess::AddDiskIO(int Type, ulong TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtEtwDiskReadType:			m_Stats.Disk.AddRead(TransferSize); break;
	case EtEtwDiskWriteType:		m_Stats.Disk.AddWrite(TransferSize); break;
	}
}

void* CWinProcess::GetQueryHandle() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->QueryHandle;
}

bool CWinProcess::IsWoW64() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsWow64Valid && m->IsWow64;
}

QString CWinProcess::GetArchString() const
{
	QReadLocker Locker(&m_Mutex); 
	if (m->IsWow64Valid && !m->IsWow64)
		return tr("x86_64");
	return tr("x86");
}

quint64 CWinProcess::GetSessionID() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m->SessionId; 
}

ulong CWinProcess::GetSubsystem() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->ImageSubsystem;
}

QString CWinProcess::GetSubsystemString() const
{
	QReadLocker Locker(&m_Mutex); 
    switch (m->ImageSubsystem)
    {
	case 0:								return "";
    case IMAGE_SUBSYSTEM_NATIVE:		return tr("Native");
    case IMAGE_SUBSYSTEM_WINDOWS_GUI:	return tr("Windows");
    case IMAGE_SUBSYSTEM_WINDOWS_CUI:	return tr("Windows console");
    case IMAGE_SUBSYSTEM_OS2_CUI:		return tr("OS/2");
    case IMAGE_SUBSYSTEM_POSIX_CUI:		return tr("POSIX");
    default:							return tr("Unknown");
    }
}

quint64 CWinProcess::GetProcessSequenceNumber() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m->ProcessSequenceNumber; 
}

quint64 CWinProcess::GetRawCreateTime() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->CreateTime.QuadPart; 
}

bool CWinProcess::ValidateParent(CProcessInfo* pParent) const
{ 
	QReadLocker Locker(&m_Mutex); 

	if (!pParent || pParent->GetProcessId() == m_ProcessId) // for cases where the parent PID = PID (e.g. System Idle Process)
        return false;

	if (m_ProcessId == (quint64)SYSTEM_PROCESS_ID && pParent->GetProcessId() == (quint64)SYSTEM_IDLE_PROCESS_ID)
		return true;

	if (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64())
	{
		// We make sure that the process item we found is actually the parent process - its sequence number
		// must not be higher than the supplied sequence.
		if (qobject_cast<CWinProcess*>(pParent)->GetProcessSequenceNumber() <= m->ProcessSequenceNumber)
			return true;
	}
	else
	{
		// We make sure that the process item we found is actually the parent process - its start time
		// must not be larger than the supplied time.
		if (qobject_cast<CWinProcess*>(pParent)->GetRawCreateTime() <= m->CreateTime.QuadPart)
			return true;
	}
	return false;
}

// Flags
bool CWinProcess::IsSubsystemProcess() const
{
	QReadLocker Locker(&m_Mutex);
	return (int)m->IsSecureProcess;
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

QString CWinProcess::GetDEPStatusString() const
{
	QReadLocker Locker(&m_Mutex);
    if (m->DepStatus & PH_PROCESS_DEP_ENABLED)
    {
        if (m->DepStatus & PH_PROCESS_DEP_PERMANENT)
            return tr("DEP (permanent)");
        else
            return tr("DEP");
    }
	return "";
}

QString CWinProcess::GetCFGuardString() const
{
	QReadLocker Locker(&m_Mutex);
	if (m->IsControlFlowGuardEnabled)
		return tr("CF Guard");
	return "";
}

QDateTime CWinProcess::GetTimeStamp() const
{
	QReadLocker Locker(&m_Mutex);
	return QDateTime::fromTime_t(m->ImageTimeDateStamp);
}

QMap<QString, CWinProcess::SEnvVar>	CWinProcess::GetEnvVariables() const
{
	QMap<QString, SEnvVar> EnvVars;

    PVOID SystemDefaultEnvironment = NULL;
    PVOID UserDefaultEnvironment = NULL;

	HANDLE processHandle;
	PVOID environment;
    ULONG environmentLength;
	ULONG enumerationKey;
    PH_ENVIRONMENT_VARIABLE variable;

	if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
	{

		HANDLE tokenHandle;
		ULONG flags = 0;

		if (CreateEnvironmentBlock != NULL)
		{
			CreateEnvironmentBlock(&SystemDefaultEnvironment, NULL, FALSE);

			if (NT_SUCCESS(PhOpenProcessToken(processHandle, TOKEN_QUERY | TOKEN_DUPLICATE, &tokenHandle)))
			{
				CreateEnvironmentBlock(&UserDefaultEnvironment, tokenHandle, FALSE);
				NtClose(tokenHandle);
			}
		}

#ifdef _WIN64
		if (m->IsWow64)
			flags |= PH_GET_PROCESS_ENVIRONMENT_WOW64;
#endif

		if (NT_SUCCESS(PhGetProcessEnvironment(processHandle, flags, &environment, &environmentLength)))
		{
			enumerationKey = 0;
			while (PhEnumProcessEnvironmentVariables(environment, environmentLength, &enumerationKey, &variable))
			{
				// Remove the most confusing item. Some say it's just a weird per-drive current directory 
				// with a colon used as a drive letter for some reason. It should not be here. (diversenok)
				//if (PhEqualStringRef2(&variable.Name, L"=::", FALSE) && PhEqualStringRef2(&variable.Value, L"::\\", FALSE))
				//    continue;

				SEnvVar EnvVar;
				EnvVar.Name = QString::fromWCharArray(variable.Name.Buffer, variable.Name.Length / sizeof(wchar_t));
				EnvVar.Value = QString::fromWCharArray(variable.Value.Buffer, variable.Value.Length / sizeof(wchar_t));
				
				
				PPH_STRING variableValue;				
				if (SystemDefaultEnvironment && PhQueryEnvironmentVariable(SystemDefaultEnvironment, &variable.Name, NULL) == STATUS_BUFFER_TOO_SMALL)
				{
					EnvVar.Type = SEnvVar::eSystem;

					if (NT_SUCCESS(PhQueryEnvironmentVariable(SystemDefaultEnvironment, &variable.Name, &variableValue )))
					{
						if (EnvVar.Value != CastPhString(variableValue))
						{
							EnvVar.Type = SEnvVar::eProcess;
						}
					}
				}
				else if (UserDefaultEnvironment && PhQueryEnvironmentVariable(UserDefaultEnvironment,&variable.Name, NULL) == STATUS_BUFFER_TOO_SMALL)
				{
					EnvVar.Type = SEnvVar::eUser;

					if (NT_SUCCESS(PhQueryEnvironmentVariable(UserDefaultEnvironment, &variable.Name,&variableValue)))
					{
						if (EnvVar.Value != CastPhString(variableValue))
						{
							EnvVar.Type = SEnvVar::eProcess;
						}
					}
				}
				
				EnvVars.insert(EnvVar.GetTypeName(), EnvVar);
			}

			PhFreePage(environment);
		}

		NtClose(processHandle);
	}

    if (DestroyEnvironmentBlock != NULL)
    {
        if (SystemDefaultEnvironment)
        {
            DestroyEnvironmentBlock(SystemDefaultEnvironment);
            SystemDefaultEnvironment = NULL;
        }

        if (UserDefaultEnvironment)
        {
            DestroyEnvironmentBlock(UserDefaultEnvironment);
            UserDefaultEnvironment = NULL;
        }
    }

	return EnvVars;
}

STATUS CWinProcess::EditEnvVariable(const QString& Name, const QString& Value)
{
	if (m->IsSuspended) 
	{
		return ERR(tr("Editing environment variable(s) of suspended processes is not supported."), ERROR_CONFIRM);
	}

	NTSTATUS status;
	HANDLE processHandle;
	LARGE_INTEGER timeout;

	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, m->UniqueProcessId)))
	{
		timeout.QuadPart = -(LONGLONG)UInt32x32To64(10, PH_TIMEOUT_SEC);

		wstring NameStr = Name.toStdWString();
		PH_STRINGREF NameRef;
		NameRef.Buffer = (PWCH)NameStr.c_str();
		NameRef.Length = NameStr.length() * sizeof(wchar_t);

		wstring ValueStr = Value.toStdWString();
		PH_STRINGREF ValueRef;
		ValueRef.Buffer = (PWCH)ValueStr.c_str();
		ValueRef.Length = ValueStr.length() * sizeof(wchar_t);
		
		status = PhSetEnvironmentVariableRemote(processHandle, &NameRef, ValueRef.Length == 0 ? NULL : &ValueRef, &timeout);

		NtClose(processHandle);
	}

	if (!NT_SUCCESS(status))
	{
		return ERR(tr("Unable to set the environment variable."), status);
	}
	if (status == STATUS_TIMEOUT)
	{
		return ERR(tr("Unable to delete the environment variable."), WAIT_TIMEOUT);
	}
	return OK;
}

QString CWinProcess::GetStatusString() const
{
	QStringList Status;

	QReadLocker Locker(&m_Mutex); 

    if (m->IsBeingDebugged)
        Status.append(tr("Debugged"));
    if (m->IsSuspended)
        Status.append(tr("Suspended"));
    if (m->IsProtectedHandle)
        Status.append(tr("HandleFiltered"));
    if (m->IsElevated)
        Status.append(tr("Elevated"));
    if (m->IsSubsystemProcess)
        Status.append(tr("Pico"));
    if (m->IsImmersive)
        Status.append(tr("Immersive"));
    if (m->IsDotNet)
        Status.append(tr("DotNet"));
    if (m->IsPacked)
        Status.append(tr("Packed"));
    if (m->IsWow64)
        Status.append(tr("Wow64"));

	Locker.unlock();

    if (IsJobProcess())
        Status.append(tr("Job"));
	if (IsServiceProcess())
        Status.append(tr("Service"));
	if (IsSystemProcess())
        Status.append(tr("System"));
	if (IsUserProcess())
        Status.append(tr("Owned"));

	return Status.join(tr(", "));
}

bool CWinProcess::HasDebugger() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsBeingDebugged;
}

STATUS CWinProcess::AttachDebugger()
{
	QWriteLocker Locker(&m_Mutex);

    static PH_STRINGREF aeDebugKeyName = PH_STRINGREF_INIT(L"Software\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug");
#ifdef _WIN64
    static PH_STRINGREF aeDebugWow64KeyName = PH_STRINGREF_INIT(L"Software\\Wow6432Node\\Microsoft\\Windows NT\\CurrentVersion\\AeDebug");
#endif
    NTSTATUS status;
    PH_STRING_BUILDER commandLineBuilder;
    HANDLE keyHandle;
    PPH_STRING debugger;
    PH_STRINGREF commandPart;
    PH_STRINGREF dummy;
	/*static*/ PPH_STRING DebuggerCommand = NULL;

    status = PhOpenKey(&keyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE,
#ifdef _WIN64
        m->IsWow64 ? &aeDebugWow64KeyName : &aeDebugKeyName,
#else
        &aeDebugKeyName,
#endif
        0);

    if (NT_SUCCESS(status))
    {
        if (debugger = (PPH_STRING)PH_AUTO(PhQueryRegistryString(keyHandle, L"Debugger")))
        {
            if (PhSplitStringRefAtChar(&debugger->sr, '"', &dummy, &commandPart) &&
                PhSplitStringRefAtChar(&commandPart, '"', &commandPart, &dummy))
            {
                DebuggerCommand = PhCreateString2(&commandPart);
            }
        }

        NtClose(keyHandle);
    }

    if (PhIsNullOrEmptyString(DebuggerCommand))
    {
		return ERR(tr("Unable to locate the debugger."));
    }

    PhInitializeStringBuilder(&commandLineBuilder, DebuggerCommand->Length + 30);

    PhAppendCharStringBuilder(&commandLineBuilder, '"');
    PhAppendStringBuilder(&commandLineBuilder, &DebuggerCommand->sr);
    PhAppendCharStringBuilder(&commandLineBuilder, '"');
    PhAppendFormatStringBuilder(&commandLineBuilder, L" -p %lu", HandleToUlong(m->UniqueProcessId));

    status = PhCreateProcessWin32(NULL, commandLineBuilder.String->Buffer, NULL, NULL, 0, NULL, NULL, NULL );

    PhDeleteStringBuilder(&commandLineBuilder);

    if (!NT_SUCCESS(status))
    {
        return ERR(tr("Failed to create debugger process"), status);
    }

    return OK;
}

STATUS CWinProcess::DetachDebugger()
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;
    HANDLE processHandle;
    HANDLE debugObjectHandle;

    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_SUSPEND_RESUME, m->UniqueProcessId)))
    {
        if (NT_SUCCESS(status = PhGetProcessDebugObject(processHandle,&debugObjectHandle)))
        {
            // Disable kill-on-close.
            if (NT_SUCCESS(status = PhSetDebugKillProcessOnExit(debugObjectHandle, FALSE)))
            {
                status = NtRemoveProcessDebug(processHandle, debugObjectHandle);
            }

            NtClose(debugObjectHandle);
        }

        NtClose(processHandle);
    }

    if (status == STATUS_PORT_NOT_SET)
    {
        return ERR(tr("The process is not being debugged."));
    }

    if (!NT_SUCCESS(status))
    {
		return ERR(tr("Failed to detach debugger"), status);
    }

    return OK;
}

bool RtlEqualSid(_In_ PSID Sid1, const CWinTokenPtr& token)
{
	if (!token)
		return false;
	QByteArray sid = token->GetUserSid();
	if (sid.isEmpty() || RtlLengthSid(Sid1) != sid.size())
		return false;
	return memcmp((char*)Sid1, sid.data(), sid.size()) == 0;
}

bool CWinProcess::IsSystemProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return (RtlEqualSid(&PhSeLocalSystemSid, m_pToken)) || PH_IS_FAKE_PROCESS_ID(m->UniqueProcessId);
}

bool CWinProcess::IsServiceProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return !m_ServiceList.isEmpty() || (RtlEqualSid(&PhSeServiceSid, m_pToken) || RtlEqualSid(&PhSeLocalServiceSid, m_pToken) || RtlEqualSid(&PhSeNetworkServiceSid, m_pToken));
}

bool CWinProcess::IsUserProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return RtlEqualSid(PhGetOwnTokenAttributes().TokenSid, m_pToken);
}

bool CWinProcess::IsElevated() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsElevated;
}

bool CWinProcess::IsJobProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsInSignificantJob;
}

bool CWinProcess::IsPicoProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsSubsystemProcess;
}

bool CWinProcess::IsImmersiveProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsImmersive;
}

bool CWinProcess::IsNetProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsDotNet;
}

QString CWinProcess::GetPackageName() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->PackageFullName;
}

QString CWinProcess::GetAppID() const
{
	QReadLocker Locker(&m_Mutex);
	if (m->AppID.isNull())
	{
		Locker.unlock();

		QWriteLocker Locker2(&m_Mutex); 
		m->AppID = ""; // isNull -> false

		PPH_STRING applicationUserModelId;
		if (m->IsSubsystemProcess)
		{
			NOTHING
		}
		else if (PhAppResolverGetAppIdForProcess(m->UniqueProcessId, &applicationUserModelId))
		{
			m->AppID = CastPhString(applicationUserModelId);
		}
		else
		{
			ULONG windowFlags;	
			if (m->QueryHandle)
			{
				if (NT_SUCCESS(PhGetProcessWindowTitle(m->QueryHandle, &windowFlags, &applicationUserModelId)))
				{
					if (windowFlags & STARTF_TITLEISAPPID)
						m->AppID = CastPhString(applicationUserModelId);
					else
						PhDereferenceObject(applicationUserModelId);
				}

				//if (WindowsVersion >= WINDOWS_8 && ProcessNode->ProcessItem->IsImmersive)
				//{
				//    HANDLE tokenHandle;
				//    PTOKEN_SECURITY_ATTRIBUTES_INFORMATION info;
				//
				//    if (NT_SUCCESS(PhOpenProcessToken(
				//        ProcessNode->ProcessItem->QueryHandle,
				//        TOKEN_QUERY,
				//        &tokenHandle
				//        )))
				//    {
				//        // rev from GetApplicationUserModelId
				//        if (NT_SUCCESS(PhQueryTokenVariableSize(tokenHandle, TokenSecurityAttributes, &info)))
				//        {
				//            for (ULONG i = 0; i < info->AttributeCount; i++)
				//            {
				//                static UNICODE_STRING attributeNameUs = RTL_CONSTANT_STRING(L"WIN://SYSAPPID");
				//                PTOKEN_SECURITY_ATTRIBUTE_V1 attribute = &info->Attribute.pAttributeV1[i];
				//
				//                if (RtlEqualUnicodeString(&attribute->Name, &attributeNameUs, FALSE))
				//                {
				//                    if (attribute->ValueType == TOKEN_SECURITY_ATTRIBUTE_TYPE_STRING)
				//                    {
				//                        PPH_STRING attributeValue1;
				//                        PPH_STRING attributeValue2;
				//
				//                        attributeValue1 = PH_AUTO(PhCreateStringFromUnicodeString(&attribute->Values.pString[1]));
				//                        attributeValue2 = PH_AUTO(PhCreateStringFromUnicodeString(&attribute->Values.pString[2]));
				//
				//                        ProcessNode->AppIdText = PhConcatStrings(
				//                            3, 
				//                            attributeValue2->Buffer,
				//                            L"!",
				//                            attributeValue1->Buffer
				//                            );
				//
				//                        break;
				//                    }
				//                }
				//            }
				//
				//            PhFree(info);
				//        }
				//
				//        NtClose(tokenHandle);
				//    }
				//}
			}
		}
		Locker2.unlock();

		Locker.relock();
	}
	return m->AppID;	
}

ulong CWinProcess::GetDPIAwareness() const
{
	QReadLocker Locker(&m_Mutex); 
	if (m->DpiAwareness == -1) {
		Locker.unlock();

		QWriteLocker Locker2(&m_Mutex); 
		m->DpiAwareness = GetProcessDpiAwareness(m->QueryHandle);
		Locker2.unlock();

		Locker.relock();
	}
	return m->DpiAwareness;
}

QString CWinProcess::GetDPIAwarenessString() const
{
    switch (GetDPIAwareness())
    {
	case 0:	return "";
    case 1: return tr("Unaware");
    case 2: return tr("System aware");
    case 3: return tr("Per-monitor aware");
    }
	return "";
}

ulong CWinProcess::GetProtection() const 
{
	QReadLocker Locker(&m_Mutex); 
	return m->Protection.Level;
}

QString CWinProcess::GetProtectionString() const
{
	QReadLocker Locker(&m_Mutex); 
    if (m->Protection.Level != UCHAR_MAX)
    {
        if (WindowsVersion >= WINDOWS_8_1)
        {
			static PWSTR ProtectedSignerStrings[] = { L"", L"(Authenticode)", L"(CodeGen)", L"(Antimalware)", L"(Lsa)", L"(Windows)", L"(WinTcb)", L"(WinSystem)", L"(StoreApp)" };

			QString Signer;
			if (m->Protection.Signer < sizeof(ProtectedSignerStrings) / sizeof(PWSTR))
				Signer = QString::fromWCharArray(ProtectedSignerStrings[m->Protection.Signer]);

            switch (m->Protection.Type)
            {
			case PsProtectedTypeNone:				return tr("None %1").arg(Signer);
            case PsProtectedTypeProtectedLight:		return tr("Light %1").arg(Signer);
            case PsProtectedTypeProtected:			return tr("Full %1").arg(Signer);
            default:								return tr("Unknown %1").arg(Signer);
            }
        }
        else
        {
            return m->IsProtectedProcess ? tr("Yes") : tr("None");
        }
    }
    return tr("N/A");
}


QString CWinProcess::GetMitigationString() const
{
	QReadLocker Locker(&m_Mutex); 
	if (!m->QueryHandle)
		return tr("N/A");

	QString MitigationString;
	
	NTSTATUS status;
	PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION information;
	if (NT_SUCCESS(status = PhGetProcessMitigationPolicy(m->QueryHandle, &information)))
	{
		PH_STRING_BUILDER sb;
        int policy;
        PPH_STRING shortDescription;

        PhInitializeStringBuilder(&sb, 100);

        for (policy = 0; policy < MaxProcessMitigationPolicy; policy++)
        {
            if (information.Pointers[policy] && PhDescribeProcessMitigationPolicy(
                (PROCESS_MITIGATION_POLICY)policy,
                information.Pointers[policy],
                &shortDescription,
                NULL
                ))
            {
                PhAppendStringBuilder(&sb, &shortDescription->sr);
                PhAppendStringBuilder2(&sb, L"; ");
                PhDereferenceObject(shortDescription);
            }
        }

        if (sb.String->Length != 0)
        {
            PhRemoveEndStringBuilder(&sb, 2);
			MitigationString = QString::fromWCharArray(sb.String->Buffer, sb.String->Length / sizeof(wchar_t));
        }
        else
        {
            MitigationString = tr("None");
        }

        PhDeleteStringBuilder(&sb);
	}

	return MitigationString;
}

quint64 CWinProcess::GetJobObjectID() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->JobObjectId;
}

quint64 CWinProcess::GetPeakPrivateBytes() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.PeakPagefileUsage;
}

quint64 CWinProcess::GetWorkingSetSize() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.WorkingSetSize;
}

quint64 CWinProcess::GetPeakWorkingSetSize() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.PeakWorkingSetSize;
}

quint64 CWinProcess::GetPrivateWorkingSetSize() const
{
	QReadLocker Locker(&m_Mutex); 
	return m_WorkingSetPrivateSize;
}

quint64 CWinProcess::GetSharedWorkingSetSize() const
{
	UpdateExtDataIfNeeded();
	QReadLocker Locker(&m_Mutex); 
	return m->WsCounters.NumberOfSharedPages * PAGE_SIZE;
}

quint64 CWinProcess::GetShareableWorkingSetSize() const
{
	UpdateExtDataIfNeeded();
	QReadLocker Locker(&m_Mutex); 
	return m->WsCounters.NumberOfShareablePages * PAGE_SIZE;
}

quint64 CWinProcess::GetVirtualSize() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.VirtualSize;
}

quint64 CWinProcess::GetPeakVirtualSize() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.PeakVirtualSize;
}

quint32 CWinProcess::GetPageFaultCount() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.PageFaultCount;
}


quint64 CWinProcess::GetPagedPool() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.QuotaPagedPoolUsage;
}

quint64 CWinProcess::GetPeakPagedPool() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.QuotaPeakPagedPoolUsage;
}

quint64 CWinProcess::GetNonPagedPool() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.QuotaNonPagedPoolUsage;
}

quint64 CWinProcess::GetPeakNonPagedPool() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->VmCounters.QuotaPeakNonPagedPoolUsage;
}

quint64 CWinProcess::GetMinimumWS() const
{
	UpdateExtDataIfNeeded();
	QReadLocker Locker(&m_Mutex); 
	return m->QuotaLimits.MinimumWorkingSetSize;
}

quint64 CWinProcess::GetMaximumWS() const
{
	UpdateExtDataIfNeeded();
	QReadLocker Locker(&m_Mutex); 
	return m->QuotaLimits.MaximumWorkingSetSize;
}

QString CWinProcess::GetDesktopInfo() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->DesktopInfo;
}

QString CWinProcess::GetPriorityString() const
{
	QReadLocker Locker(&m_Mutex); 

	switch (m_Priority)
    {
	case PROCESS_PRIORITY_CLASS_REALTIME:		return tr("Real time");
    case PROCESS_PRIORITY_CLASS_HIGH:			return tr("High");
    case PROCESS_PRIORITY_CLASS_ABOVE_NORMAL:	return tr("Above normal");
    case PROCESS_PRIORITY_CLASS_NORMAL:			return tr("Normal");
	case PROCESS_PRIORITY_CLASS_BELOW_NORMAL:	return tr("Below normal");
    case PROCESS_PRIORITY_CLASS_IDLE:			return tr("Idle");
	default:									return tr("Unknown");
    }
}

STATUS CWinProcess::SetPriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
    {
        if (m->UniqueProcessId != SYSTEM_PROCESS_ID)
        {
            PROCESS_PRIORITY_CLASS priorityClass;

            priorityClass.Foreground = FALSE;
            priorityClass.PriorityClass = (UCHAR)Value;

            status = PhSetProcessPriority(processHandle, priorityClass);
        }
        else
        {
            // Changing the priority of System can lead to a BSOD on some versions of Windows,
            // so disallow this.
            status = STATUS_UNSUCCESSFUL;
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "SetPriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set Process priority"), status);
    }
	return OK;
}

STATUS CWinProcess::SetPagePriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
    {
        if (m->UniqueProcessId != SYSTEM_PROCESS_ID)
        {
            status = PhSetProcessPagePriority(processHandle, Value);
        }
        else
        {
            // See comment in PhUiSetPriorityProcesses.
            status = STATUS_UNSUCCESSFUL;
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "SetPagePriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set Page priority"), status);
    }
	return OK;
}

STATUS CWinProcess::SetIOPriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
    {
        if (m->UniqueProcessId != SYSTEM_PROCESS_ID)
        {
            status = PhSetProcessIoPriority(processHandle, (IO_PRIORITY_HINT)Value);
        }
        else
        {
            // See comment in PhUiSetPriorityProcesses.
            status = STATUS_UNSUCCESSFUL;
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "SetIOPriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set I/O priority"), status);
    }
	return OK;
}

STATUS CWinProcess::SetAffinityMask(quint64 Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
	HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
    {
        status = PhSetProcessAffinityMask(processHandle, Value);
        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "SetAffinityMask", Value))
				return OK;
		}

		return ERR(tr("Failed to set CPU affinity"), status);
    }
	return OK;
}

STATUS CWinProcess::Terminate()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_TERMINATE, m->UniqueProcessId)))
    {
        // An exit status of 1 is used here for compatibility reasons:
        // 1. Both Task Manager and Process Explorer use 1.
        // 2. winlogon tries to restart explorer.exe if the exit status is not 1.

        status = PhTerminateProcess(processHandle, 1);
        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "Terminate"))
				return OK;
		}

		return ERR(tr("Failed to terminate process"), status);
    }
	return OK;
}

bool CWinProcess::IsSuspended() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsSuspended;
}

STATUS CWinProcess::Suspend()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, m->UniqueProcessId)))
    {
        status = NtSuspendProcess(processHandle);
        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "Suspend"))
				return OK;
		}

		return ERR(tr("Failed to suspend process"), status);
    }
	return OK;
}

STATUS CWinProcess::Resume()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, m->UniqueProcessId)))
    {
        status = NtResumeProcess(processHandle);
        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "Resume"))
				return OK;
		}

		return ERR(tr("Failed to resume process"), status);
    }
	return OK;
}

STATUS CWinProcess::SetCriticalProcess(bool bSet, bool bForce)
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    BOOLEAN breakOnTermination;

    status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_SET_INFORMATION, m->UniqueProcessId);
    if (NT_SUCCESS(status))
    {
        status = PhGetProcessBreakOnTermination(processHandle, &breakOnTermination);

        if (NT_SUCCESS(status))
        {
			if (bSet == false && breakOnTermination)
            {
				status = PhSetProcessBreakOnTermination(processHandle, FALSE);
            }
			else if (bSet == true && !breakOnTermination)
			{
#ifndef SAFE_MODE // in safe mode always check and fail
				if (!bForce)
#endif
				{
					NtClose(processHandle);

					return ERR(tr("If the process ends, the operating system will shut down immediately."), ERROR_CONFIRM);
				}

				status = PhSetProcessBreakOnTermination(processHandle, TRUE);
			} 
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
        return ERR(tr("Unable to change the process critical status."), status);
    }

	m_IsCritical = bSet;
	return OK;
}

STATUS CWinProcess::ReduceWS()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_QUOTA, m->UniqueProcessId)))
    {
        QUOTA_LIMITS quotaLimits;

        memset(&quotaLimits, 0, sizeof(QUOTA_LIMITS));
        quotaLimits.MinimumWorkingSetSize = -1;
        quotaLimits.MaximumWorkingSetSize = -1;

        status = PhSetProcessQuotaLimits(processHandle, quotaLimits);

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
		return ERR(tr("Unable to reduce the working set of a process"), status);
    }

	return OK;
}

STATUS CWinProcess::LoadModule(const QString& Path)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
	
	LARGE_INTEGER Timeout;
		Timeout.QuadPart = -(LONGLONG)UInt32x32To64(5, PH_TIMEOUT_SEC);
	wstring FileName = Path.toStdWString();

    HANDLE ProcessHandle;
	if (NT_SUCCESS(status = PhOpenProcess(&ProcessHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, m->UniqueProcessId)))
	{
#ifdef _WIN64
		static PVOID loadLibraryW32 = NULL;
#endif
		NTSTATUS status;
#ifdef _WIN64
		BOOLEAN isWow64 = FALSE;
		BOOLEAN isModule32 = FALSE;
		PH_MAPPED_IMAGE mappedImage;
#endif
		PVOID threadStart;
		PH_STRINGREF fileName;
		PVOID baseAddress = NULL;
		SIZE_T allocSize;
		HANDLE threadHandle;

#ifdef _WIN64
		PhGetProcessIsWow64(ProcessHandle, &isWow64);

		if (isWow64)
		{
			if (!NT_SUCCESS(status = PhLoadMappedImage((wchar_t*)FileName.c_str(), NULL, TRUE, &mappedImage)))
				goto FreeExit;

			isModule32 = mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
			PhUnloadMappedImage(&mappedImage);
		}

		if (!isModule32)
		{
#endif
			threadStart = PhGetDllProcedureAddress(L"kernel32.dll", "LoadLibraryW", 0);
#ifdef _WIN64
		}
		else
		{
			threadStart = loadLibraryW32;

			if (!threadStart)
			{
				PH_STRINGREF systemRoot;
				PPH_STRING kernel32FileName;

				PhGetSystemRoot(&systemRoot);
				kernel32FileName = PhConcatStringRefZ(&systemRoot, L"\\SysWow64\\kernel32.dll");

				status = PhGetProcedureAddressRemote(ProcessHandle, kernel32FileName->Buffer, "LoadLibraryW", 0, &loadLibraryW32, NULL);
				PhDereferenceObject(kernel32FileName);

				if (!NT_SUCCESS(status))
					goto FreeExit;

				threadStart = loadLibraryW32;
			}
		}
#endif

		PhInitializeStringRefLongHint(&fileName, (wchar_t*)FileName.c_str());
		allocSize = fileName.Length + sizeof(UNICODE_NULL);

		if (!NT_SUCCESS(status = NtAllocateVirtualMemory(ProcessHandle, &baseAddress, 0, &allocSize, MEM_COMMIT, PAGE_READWRITE)))
			goto FreeExit;

		if (!NT_SUCCESS(status = NtWriteVirtualMemory(ProcessHandle, baseAddress, fileName.Buffer, fileName.Length + sizeof(UNICODE_NULL), NULL)))
			goto FreeExit;

		if (!NT_SUCCESS(status = RtlCreateUserThread(ProcessHandle, NULL, FALSE, 0, 0, 0, (PUSER_THREAD_START_ROUTINE)threadStart, baseAddress, &threadHandle, NULL)))
			goto FreeExit;

		// Wait for the thread to finish.	
		status = NtWaitForSingleObject(threadHandle, FALSE, &Timeout);
		NtClose(threadHandle);

	FreeExit:
		// Size needs to be zero if we're freeing.	
		if (baseAddress)
		{
			allocSize = 0;
			NtFreeVirtualMemory(ProcessHandle, &baseAddress, &allocSize, MEM_RELEASE);
		}

		NtClose(ProcessHandle);
	}

	if (!NT_SUCCESS(status))
	{
		return ERR(tr("load the DLL into"), status);
	}
    return OK;
}

NTSTATUS PhpProcessGeneralOpenProcess(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenProcess(Handle, DesiredAccess, (HANDLE)Context);
}

void CWinProcess::OpenPermissions()
{
	QWriteLocker Locker(&m_Mutex); 

    PhEditSecurity(NULL, (wchar_t*)m_ProcessName.toStdWString().c_str(), L"Process", (PPH_OPEN_OBJECT)PhpProcessGeneralOpenProcess, NULL, (HANDLE)m->UniqueProcessId);
}

quint64 CWinProcess::GetPebBaseAddress(bool bWow64) const
{
	QReadLocker Locker(&m_Mutex); 
	return bWow64 ? m->PebBaseAddress32 : m->PebBaseAddress;
}

CWinJobPtr CWinProcess::GetJob() const
{
	QReadLocker Locker(&m_Mutex); 

	if (!m->QueryHandle)
		return CWinJobPtr();

	CWinJobPtr pJob = CWinJobPtr(new CWinJob());
	if (!pJob->InitStaticData(m->QueryHandle))
		return CWinJobPtr();
	return pJob;
}

QMap<quint64, CMemoryPtr> CWinProcess::GetMemoryMap() const
{
	ULONG Flags = PH_QUERY_MEMORY_REGION_TYPE | PH_QUERY_MEMORY_WS_COUNTERS;
	QMap<quint64, CMemoryPtr> MemoryMap;
	PhQueryMemoryItemList((HANDLE)GetProcessId(), Flags, MemoryMap);
	return MemoryMap;
}

