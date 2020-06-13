/*
 * Task Explorer -
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
extern "C" {
#include <kphuserp.h>
}

#include "WindowsAPI.h"
#include "WinProcess.h"
#include "WinThread.h"
#include "WinHandle.h"
#include "WinModule.h"
#include "WinWnd.h"
#include "ProcessHacker/memprv.h"
#include "ProcessHacker/appsup.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/Settings.h"
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

		Flags = NULL;
		AsyncFinished = false;

		ConsoleHostProcessId = NULL;

		//memset(&VmCounters, 0, sizeof(VM_COUNTERS_EX));
		//memset(&IoCounters, 0, sizeof(IO_COUNTERS));
		memset(&WsCounters, 0, sizeof(PH_PROCESS_WS_COUNTERS));
		LastWsCountersUpdate = 0;
		memset(&QuotaLimits, 0, sizeof(QUOTA_LIMITS));
		memset(&HandleInfo, 0, sizeof(PROCESS_HANDLE_INFORMATION));
		memset(&UptimeInfo, 0, sizeof(PROCESS_UPTIME_INFORMATION));

		// Other Fields
		Protection.Level = 0;
		ProcessSequenceNumber = -1;
		JobObjectId = 0;
		DpiAwareness = -1;

		// Signature, Packed
		//ImportFunctions;
		//ImportModules;

		// OS Context
		memset(&OsContextGuid, 0, sizeof(GUID));
		OsContextVersion = 0;

		// Misc.
		DepStatus = 0;

		KnownProcessType = -1;
	}

	
	// Handles
	HANDLE UniqueProcessId;
	HANDLE QueryHandle;

	// Basic
	quint64 SessionId;
	LARGE_INTEGER CreateTime;

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
			//ULONG IsInSignificantJob : 1;
			ULONG IsPacked : 1;
			ULONG IsHandleValid : 1; // if this is true we can querry handles
			ULONG IsSuspended : 1;
			ULONG IsWow64 : 1;
			ULONG IsImmersive : 1;
			ULONG IsWow64Valid : 1;
			ULONG IsPartiallySuspended : 1;
			ULONG IsProtectedHandle : 1;
			ULONG IsProtectedProcess : 1;
			ULONG IsProcessDeleting : 1;
			ULONG IsCrossSessionCreate : 1;
			ULONG IsFrozen : 1;
			ULONG IsBackground : 1;
			ULONG IsStronglyNamed : 1;
			ULONG IsSecureProcess : 1;
			ULONG IsSubsystemProcess : 1;
			ULONG IsControlFlowGuardEnabled : 1;
			ULONG IsOrWasRunning : 1;
			ULONG TokenHasChanged : 1;
			ULONG IsHiddenProcess : 1;
			ULONG IsSandBoxed : 1;
			ULONG Spare : 7;
		};
	};
	bool AsyncFinished;

	// Other
	HANDLE ConsoleHostProcessId;

	// Dynamic
	//VM_COUNTERS_EX VmCounters;
	//IO_COUNTERS IoCounters;
    PH_PROCESS_WS_COUNTERS WsCounters;
	volatile quint64 LastWsCountersUpdate;
	QUOTA_LIMITS QuotaLimits;

	PROCESS_HANDLE_INFORMATION HandleInfo;
	PROCESS_UPTIME_INFORMATION UptimeInfo;

	// Other Fields
	PS_PROTECTION Protection;
	quint64 ProcessSequenceNumber;
	quint32 JobObjectId;
	QString PackageFullName;
	QString AppID;
	ULONG DpiAwareness;

	// Signature, Packed
	//quint32 ImportFunctions;
	//quint32 ImportModules;

	// OS Context
    GUID OsContextGuid;
    quint32 OsContextVersion;

	// Misc.
	ULONG DepStatus;

	int KnownProcessType;

	QString DesktopInfo;
};


// CWinProcess Class members

CWinProcess::CWinProcess(QObject *parent) : CProcessInfo(parent)
{
	m_IsFullyInitialized = false;
	m_LastUpdateThreads = -1; // -1 never
	m_LastUpdateHandles = 0;

	// Dynamic
	m_QuotaPagedPoolUsage = 0;
	m_QuotaPeakPagedPoolUsage = 0;
	m_QuotaNonPagedPoolUsage = 0;
	m_QuotaPeakNonPagedPoolUsage = 0;

	// GDI, USER handles
	m_GdiHandles = 0;
    m_UserHandles = 0;
	m_WndHandles = 0;

	m_IsCritical = false;

	//m_lastExtUpdate = 0;

	// ph special
	m = new SWinProcess();
}

CWinProcess::~CWinProcess()
{
	UnInit(); // just in case we forgot to do that

	delete m;
}

bool CWinProcess::InitStaticData(quint64 ProcessId)
{
	QWriteLocker Locker(&m_Mutex);

	m->UniqueProcessId = (HANDLE)ProcessId;
	m_ProcessId = ProcessId;

	m_CreateTimeStamp = GetTime() * 1000;

	if (!InitStaticData())
	{
		m_ProcessName = tr("Unknown process PID: %1").arg(ProcessId);
		return false;
	}

	int pos = m_FileName.lastIndexOf("\\");
	m_ProcessName = m_FileName.mid(pos + 1);

	return true;
}

bool CWinProcess::InitStaticData(struct _SYSTEM_PROCESS_INFORMATION* Process, bool bFullProcessInfo)
{
	QWriteLocker Locker(&m_Mutex);

	m_IsFullyInitialized = true;

	// PhCreateProcessItem
	m->UniqueProcessId = Process->UniqueProcessId;
	m_ProcessId = (quint64)m->UniqueProcessId;

	// PhpFillProcessItem Begin
	m_ParentProcessId = (quint64)Process->InheritedFromUniqueProcessId;
	m->SessionId = (quint64)Process->SessionId;

	if (m_ProcessId != (quint64)SYSTEM_IDLE_PROCESS_ID)
	{
		if (bFullProcessInfo)
		{
			PPH_STRING fileName = PhCreateStringFromUnicodeString(&Process->ImageName);
			PPH_STRING newFileName = PhGetFileName(fileName);
			PhDereferenceObject(fileName);

			m_FileName = CastPhString(newFileName);

			int pos = m_FileName.lastIndexOf("\\");
			m_ProcessName = m_FileName.mid(pos+1);
		}
		else
			m_ProcessName = QString::fromWCharArray(Process->ImageName.Buffer, Process->ImageName.Length / sizeof(wchar_t));
	}
	else
		m_ProcessName = QString::fromWCharArray(SYSTEM_IDLE_PROCESS_NAME);

	m->CreateTime = Process->CreateTime;
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);

	if(m->QueryHandle == NULL) // we may already have opened the handle and initialized some data if the process was seen in an ETW or FW event
		InitStaticData(!bFullProcessInfo);

	// On Windows 8.1 and above, processes without threads are reflected processes
	// which will not terminate if we have a handle open. (wj32)
	if (Process->NumberOfThreads == 0 && m->QueryHandle)
	{
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}
	// PhpFillProcessItem End

	// UWP
	if (PH_IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		// Immersive
		if (m->QueryHandle && WindowsVersion >= WINDOWS_8 && !m->IsSubsystemProcess)
			m->IsImmersive = !!::IsImmersiveProcess(m->QueryHandle);

		if (bFullProcessInfo && WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64())
		{
			PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension = PH_EXTENDED_PROCESS_EXTENSION(Process);

			#define GET_PROCESS_EXTENSION_PROCESS(Process,Field) ( \
				((PSYSTEM_PROCESS_INFORMATION_EXTENSION)(Process))->Field ? \
				(void*)PTR_ADD_OFFSET((Process), \
				((PSYSTEM_PROCESS_INFORMATION_EXTENSION)(Process))->Field) : \
				NULL \
				)

			// User SID
			//PSID UserSid = (PSID)GET_PROCESS_EXTENSION_PROCESS(processExtension, UserSidOffset);
			//QByteArray UserSidArr = QByteArray((char*)UserSid, RtlLengthSid(UserSid));

			// Package Full Name
			wchar_t* PackageFullName = (wchar_t*)GET_PROCESS_EXTENSION_PROCESS(processExtension, PackageFullNameOffset);
			if (PackageFullName)
				m->PackageFullName = QString::fromWCharArray(PackageFullName);

			// App ID
			wchar_t* AppId = (wchar_t*)GET_PROCESS_EXTENSION_PROCESS(processExtension, AppIdOffset);
			if (AppId)
				m->AppID = QString::fromWCharArray(AppId);
		}
		else
		{
			// Package full name
			if (m->QueryHandle && WindowsVersion >= WINDOWS_8 && m->IsImmersive)
				m->PackageFullName = CastPhString(PhGetProcessPackageFullName(m->QueryHandle));

			// App ID
			if (!m->IsSubsystemProcess)
			{
				PPH_STRING applicationUserModelId;
				if (PhAppResolverGetAppIdForProcess(m->UniqueProcessId, &applicationUserModelId))
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
			}
		}

		m->DpiAwareness = GetProcessDpiAwareness(m->QueryHandle);
	}
	//

	return true;
}

bool CWinProcess::InitStaticData(bool bLoadFileName)
{
	// Open a handle to the Process for later usage.
	if (PH_IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, PROCESS_QUERY_INFORMATION, m->UniqueProcessId)))
		{
			m->IsHandleValid = TRUE;
		}

		if (!m->QueryHandle)
		{
			PhOpenProcess(&m->QueryHandle, PROCESS_QUERY_LIMITED_INFORMATION, m->UniqueProcessId);
		}

		if (!m->QueryHandle)
			qDebug() << "failed to open QueryHandle for" << m_ProcessId;
	}

	// Process flags
	if (m->QueryHandle)
	{
		PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;

		if (NT_SUCCESS(PhGetProcessExtendedBasicInformation(m->QueryHandle, &basicInfo)))
		{
			m->IsProtectedProcess = basicInfo.IsProtectedProcess;
			m->IsProcessDeleting = basicInfo.IsProcessDeleting;
			m->IsCrossSessionCreate = basicInfo.IsCrossSessionCreate;
			m->IsFrozen = basicInfo.IsFrozen;
			m->IsBackground = basicInfo.IsBackground;
			m->IsStronglyNamed = basicInfo.IsStronglyNamed;
			m->IsSecureProcess = basicInfo.IsSecureProcess;
			m->IsSubsystemProcess = basicInfo.IsSubsystemProcess;
			m->IsWow64 = basicInfo.IsWow64Process;
			m->IsWow64Valid = TRUE;
		}
	}

	// Process information

	// If we're dealing with System (PID 4), we need to get the
	// kernel file name. Otherwise, get the Module file name. (wj32)
	if (m->UniqueProcessId == SYSTEM_PROCESS_ID)
	{
		PPH_STRING fileName = PhGetKernelFileName();
		if (fileName)
		{
			m_FileName = CastPhString(PhGetFileName(fileName));
			PhDereferenceObject(fileName);
		}
	}
	else if (bLoadFileName)
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
			m_FileName = CastPhString(PhGetFileName(fileName));
			PhDereferenceObject(fileName);
		}
	}

	// Token information
	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID)
	{
		m_pToken = CWinTokenPtr(CWinToken::NewSystemToken()); // System token can't be opened (dmex)
	}
	// Note: See comment in UpdateDynamicData
	else if (m->QueryHandle)
	{
		m_pToken = CWinTokenPtr(CWinToken::TokenFromProcess(m->QueryHandle));

		//m->IsElevated = m_pToken->IsElevated(); // (m_pToken->GetElevationType() == TokenElevationTypeFull);
	}

	// Protection
	if (m->QueryHandle)
	{
		if (WindowsVersion >= WINDOWS_8_1)
		{
			// Note for WINDOWS_8_1 and above this is updated dynamically
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

	// PhpFillProcessItemExtension is done in UpdateDynamicData which is to be called right after InitStaticData

    // Add service names to the process item.
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
        PhGetProcessConsoleHostProcessId(m->QueryHandle, &m->ConsoleHostProcessId);

	// UWP...

    if (m->QueryHandle && m->IsHandleValid)
    {
        OBJECT_BASIC_INFORMATION basicInfo;
        if (NT_SUCCESS(PhGetHandleInformationEx(NtCurrentProcess(), m->QueryHandle, ULONG_MAX, 0, NULL, &basicInfo, NULL, NULL, NULL, NULL)))
        {
            if ((basicInfo.GrantedAccess & PROCESS_QUERY_INFORMATION) != PROCESS_QUERY_INFORMATION)
                m->IsProtectedHandle = TRUE;
        }
        else
            m->IsProtectedHandle = TRUE;
    }
	// PhpProcessQueryStage1 End

	CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)theAPI)->GetSandboxieAPI();
	m->IsSandBoxed = pSandboxieAPI ? pSandboxieAPI->IsSandBoxed(m_ProcessId) : false;

	if (!m->IsSubsystemProcess)
	{	
		HANDLE processHandle = NULL;
		if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
		{
			// OS context
			if (NT_SUCCESS(PhGetProcessSwitchContext(processHandle, &m->OsContextGuid)))
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

			// desktop
			PPH_STRING desktopinfo;
			if (NT_SUCCESS(PhGetProcessDesktopInfo(processHandle, &desktopinfo)))
			{
				m_UsedDesktop = CastPhString(desktopinfo);
			}

			NtClose(processHandle);
		}
	}

	m->KnownProcessType = PhGetProcessKnownTypeEx(m_ProcessId, m_FileName);

	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == DPCS_PROCESS_ID || m->UniqueProcessId == INTERRUPTS_PROCESS_ID)
		return true;

	if (!m_FileName.isEmpty() || IsHandleValid()) // Without the process still being running or a valid path this is pointless...
		UpdateModuleInfo();

	InitPresets();

	return IsHandleValid();
}

void CWinProcess::UpdateModuleInfo()
{
	CWinMainModule* pModule = new CWinMainModule();
	m_pModuleInfo = CModulePtr(pModule);
	connect(pModule, SIGNAL(AsyncDataDone(bool, quint32, quint32)), this, SLOT(OnAsyncDataDone(bool, quint32, quint32)));
	pModule->InitStaticData(m_ProcessId, m_FileName, m->IsSubsystemProcess, m->IsWow64);
	pModule->InitAsyncData(m->PackageFullName);
}

void CWinProcess::SetFileName(const QString& FileName)
{ 
	QWriteLocker Locker(&m_Mutex); 
	m_FileName = FileName; 

	if (m_pModuleInfo.isNull())
		UpdateModuleInfo();

	if(m->KnownProcessType == -1)
		m->KnownProcessType = PhGetProcessKnownTypeEx(m_ProcessId, m_FileName);
}

bool CWinProcess::IsHandleValid()
{
	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID) 
		return true;
	return m->QueryHandle != NULL;
}

void CWinProcess::OnAsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules)
{
	m->IsPacked = IsPacked;
	//m->ImportFunctions = ImportFunctions;
	//m->ImportModules = ImportModules;
	m->AsyncFinished = true;
}

bool CWinProcess::UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* Process, bool bFullProcessInfo, quint64 sysTotalTime)
{
	PSYSTEM_PROCESS_INFORMATION_EXTENSION processExtension = NULL;
	if (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64() && PH_IS_REAL_PROCESS_ID(m->UniqueProcessId))
	{
		processExtension = bFullProcessInfo ? PH_EXTENDED_PROCESS_EXTENSION(Process) : PH_PROCESS_EXTENSION(Process);
	}

	QWriteLocker Locker(&m_Mutex);

	bool modified = FALSE;

	if (m->AsyncFinished)
	{
		modified = TRUE;
		m->AsyncFinished = false;
	}

	BOOLEAN isSuspended = PH_IS_REAL_PROCESS_ID(Process->UniqueProcessId);
	BOOLEAN isPartiallySuspended = FALSE;
	ULONG contextSwitches = 0;

	// HACK: Minimal/Reflected processes don't have threads (TO-DO: Use PhGetProcessIsSuspended instead).
	if (Process->NumberOfThreads == 0)
		isSuspended = FALSE;
	else for (ULONG i = 0; i < Process->NumberOfThreads; i++)
	{
		_SYSTEM_THREAD_INFORMATION* Thread = bFullProcessInfo ? &((PSYSTEM_EXTENDED_THREAD_INFORMATION)Process->Threads)[i].ThreadInfo : &Process->Threads[i];

		if (Thread->ThreadState != Waiting || Thread->WaitReason != Suspended)
			isSuspended = FALSE;
		else
			isPartiallySuspended = TRUE;

		if (processExtension == NULL)
			contextSwitches += Thread->ContextSwitches;
	}
	if (processExtension != NULL)
		contextSwitches = processExtension->ContextSwitches;

    if (m->IsSuspended != isSuspended)
    {
        m->IsSuspended = isSuspended;
        modified = TRUE;
    }

    m->IsPartiallySuspended = isPartiallySuspended;

	//bool IsOrWasRunning = m->IsOrWasRunning
	// We want to detect if a process was already running or was CREATE_SUSPENDED and not resumed yet
	if (!m->IsOrWasRunning && (!m->IsSuspended || contextSwitches > 1))
	{
		m->IsOrWasRunning = true;
	}

	// PhpUpdateDynamicInfoProcessItem Begin
	m_BasePriority = Process->BasePriority;

	if (m->QueryHandle)
	{
		bool PriorityChanged = false;

		PROCESS_PRIORITY_CLASS PriorityClass;
		if (NT_SUCCESS(PhGetProcessPriority(m->QueryHandle, &PriorityClass)) && m_Priority != PriorityClass.PriorityClass)
		{
			PriorityChanged = true;
			m_Priority = PriorityClass.PriorityClass;
		}

		IO_PRIORITY_HINT IoPriority;
		if (NT_SUCCESS(PhGetProcessIoPriority(m->QueryHandle, &IoPriority)) && m_IOPriority != IoPriority)
		{
			PriorityChanged = true;
			m_IOPriority = IoPriority;
		}
                
		ULONG PagePriority;
		if (NT_SUCCESS(PhGetProcessPagePriority(m->QueryHandle, &PagePriority)) && m_PagePriority != PagePriority)
		{
			PriorityChanged = true;
			m_PagePriority = PagePriority;
		}

        PROCESS_BASIC_INFORMATION basicInfo;
		if (NT_SUCCESS(PhGetProcessBasicInformation(m->QueryHandle, &basicInfo)) && m_AffinityMask != basicInfo.AffinityMask)
		{
			PriorityChanged = true;
			m_AffinityMask = basicInfo.AffinityMask;
		}

		if (PriorityChanged)
		{
			modified = TRUE;

			if (!m_PersistentPreset.isNull())
				QTimer::singleShot(0, this, SLOT(ApplyPresets()));
		}
	}
	else
	{
		m_Priority = 0;
		m_IOPriority = 0;
		m_PagePriority = 0;
		m_AffinityMask = 0;
	}

	m_KernelTime = Process->KernelTime.QuadPart;
	m_UserTime = Process->UserTime.QuadPart;
	m_NumberOfHandles = Process->HandleCount;
	m_NumberOfThreads = Process->NumberOfThreads;
	m_PeakNumberOfThreads = Process->NumberOfThreadsHighWatermark;
	
	m_PeakPagefileUsage = Process->PeakPagefileUsage;
	m_WorkingSetSize = Process->WorkingSetSize;
	m_PeakWorkingSetSize = Process->PeakWorkingSetSize;
	m_WorkingSetPrivateSize = Process->WorkingSetPrivateSize.QuadPart;
	m_VirtualSize = Process->VirtualSize;
	m_PeakVirtualSize = Process->PeakVirtualSize;
	//m_PageFaultCount = Process->PageFaultCount;
	m_QuotaPagedPoolUsage = Process->QuotaPagedPoolUsage;
	m_QuotaPeakPagedPoolUsage = Process->QuotaPeakPagedPoolUsage;
	m_QuotaNonPagedPoolUsage = Process->QuotaNonPagedPoolUsage;
	m_QuotaPeakNonPagedPoolUsage = Process->QuotaPeakNonPagedPoolUsage;

	// todo: modifyed

	// Update VM and I/O counters.
	//m->VmCounters = *(PVM_COUNTERS_EX)&Process->PeakVirtualSize;
	//m->IoCounters = *(PIO_COUNTERS)&Process->ReadOperationCount;
	// PhpUpdateDynamicInfoProcessItem End


	if (processExtension)
	{
		m->JobObjectId = processExtension->JobObjectId;
		m->ProcessSequenceNumber = processExtension->ProcessSequenceNumber;
	}

	if (m->QueryHandle)
	{
		// Token information
		//UpdateTokenData();

		// Job
		if (!processExtension || m->JobObjectId != 0) // Note: if we don't have the processExtension we need to try every process
		{
			//BOOLEAN isInSignificantJob = FALSE;
			BOOLEAN isInJob = FALSE;

			/*if (KphIsConnected())
			{
				HANDLE jobHandle = NULL;

				status = KphOpenProcessJob(m->QueryHandle, JOB_OBJECT_QUERY, &jobHandle);

				if (NT_SUCCESS(status) && status != STATUS_PROCESS_NOT_IN_JOB)
				{
					isInJob = TRUE;

					// Process Explorer only recognizes processes as being in jobs if they don't have
					// the silent-breakaway-OK limit as their only limit. Emulate this behaviour.
					JOBOBJECT_BASIC_LIMIT_INFORMATION basicLimits;
					if (NT_SUCCESS(PhGetJobBasicLimits(jobHandle, &basicLimits)))
					{
						isInSignificantJob = basicLimits.LimitFlags != JOB_OBJECT_LIMIT_SILENT_BREAKAWAY_OK;
					}
				}

				if (jobHandle)
					NtClose(jobHandle);
			}
			else*/
			{
				NTSTATUS status = NtIsProcessInJob(m->QueryHandle, NULL);
				if (NT_SUCCESS(status))
					isInJob = (status == STATUS_PROCESS_IN_JOB);
			}

			/*if (m->IsInSignificantJob != isInSignificantJob)
			{
				m->IsInSignificantJob = isInSignificantJob;
				modified = TRUE;
			}*/

			if (m->IsInJob != isInJob)
			{
				m->IsInJob = isInJob;
				modified = TRUE;
			}
		}

		// Debugged
		if (!m->IsSubsystemProcess && !m->IsProtectedHandle)
		{
			BOOLEAN isBeingDebugged = FALSE;
			PhGetProcessIsBeingDebugged(m->QueryHandle, &isBeingDebugged);
			if (m->IsBeingDebugged != isBeingDebugged)
			{
				m->IsBeingDebugged = isBeingDebugged;
				modified = TRUE;
			}
		}

		// Note: The immersive state of a process can never change. No need to update it like Process Hacker does

		// GDI, USER handles
		m_GdiHandles = GetGuiResources(m->QueryHandle, GR_GDIOBJECTS);
		m_UserHandles = GetGuiResources(m->QueryHandle, GR_USEROBJECTS);

		// DEP Status
		ULONG depStatus = 0;
		if (NT_SUCCESS(PhGetProcessDepStatus(m->QueryHandle, &depStatus)))
			m->DepStatus = depStatus;

		// Protection
		if (WindowsVersion >= WINDOWS_8_1)
		{
			// Note: the protection state of a process shouldn't be able to change, but with the right kernel driver it can.
			PS_PROTECTION protection;
			if (NT_SUCCESS(PhGetProcessProtection(m->QueryHandle, &protection)))
			{
				m->Protection.Level = protection.Level;
				m->IsProtectedProcess = m->Protection.Level != 0;
			}
		}

		// update critical flag
		BOOLEAN breakOnTermination;
		if (NT_SUCCESS(PhGetProcessBreakOnTermination(m->QueryHandle, &breakOnTermination)))
			m_IsCritical = breakOnTermination;

		//if(PH_IS_REAL_PROCESS_ID(m->UniqueProcessId)) // WARNING: querying WsCounters causes very high CPU load !!!
		//	PhGetProcessWsCounters(m->QueryHandle, &m->WsCounters); 

		//PhGetProcessQuotaLimits(m->QueryHandle, &m->QuotaLimits); // this is by far not as cpu intensive but lets handle it the same

		PhGetProcessHandleCount(m->QueryHandle, &m->HandleInfo);

		if (WindowsVersion >= WINDOWS_10_RS3)
			PhGetProcessUptime(m->QueryHandle, &m->UptimeInfo);
	}
    else
    {
        m_GdiHandles = 0;
        m_UserHandles = 0;
    }

	// Note: dont keep the handle open for thereads we are not looking at.
	if (m_LastUpdateThreads != -1 && GetCurTick() - m_LastUpdateThreads > 5*1000)
	{
		m_LastUpdateThreads = -1; // means no handle open

		foreach(const CThreadPtr& pThread, GetThreadList())
			pThread.staticCast<CWinThread>()->CloseHandle();
	}

	QWriteLocker StatsLocker(&m_StatsMutex);

	// Update the deltas.
	m_CpuStats.CpuKernelDelta.Update(Process->KernelTime.QuadPart);
	m_CpuStats.CpuUserDelta.Update(Process->UserTime.QuadPart);
	m_CpuStats.CycleDelta.Update(Process->CycleTime);

	m_CpuStats.ContextSwitchesDelta.Update(contextSwitches);
	m_CpuStats.PageFaultsDelta.Update(Process->PageFaultCount);
	m_CpuStats.HardFaultsDelta.Update(Process->HardFaultCount);
	m_CpuStats.PrivateBytesDelta.Update(Process->PagefileUsage);

	m_CpuStats.UpdateStats(sysTotalTime);

	m_Stats.Io.SetRead(Process->ReadTransferCount.QuadPart, Process->ReadOperationCount.QuadPart);
	m_Stats.Io.SetWrite(Process->WriteTransferCount.QuadPart, Process->WriteOperationCount.QuadPart);
	m_Stats.Io.SetOther(Process->OtherTransferCount.QuadPart, Process->OtherOperationCount.QuadPart);

	if (((CWindowsAPI*)theAPI)->UseDiskCounters() && processExtension)
	{
		PPROCESS_DISK_COUNTERS diskCounters = &processExtension->DiskCounters;

		m_Stats.Disk.SetRead(diskCounters->BytesRead, diskCounters->ReadOperationCount);
		m_Stats.Disk.SetWrite(diskCounters->BytesWritten, diskCounters->WriteOperationCount);
		//diskCounters->FlushOperationCount // todo
	}

	m_Stats.UpdateStats();

	return modified;
}

bool CWinProcess::UpdateTokenData(bool MonitorChange)
{
	if (!m_pToken || m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID) // System token can't be opened (dmex)
		return false;
	
	if (!m_pToken->UpdateDynamicData(MonitorChange, m->IsOrWasRunning))
		return false;
	
	m->IsElevated = m_pToken->IsElevated();
	return true;
}

void CWinProcess::UpdateCPUCycles(quint64 sysTotalTime, quint64 sysTotalCycleTime)
{
	QWriteLocker StatsLocker(&m_StatsMutex);
	m_CpuStats.UpdateStats(sysTotalTime, sysTotalCycleTime);
}

/*bool CWinProcess::UpdateDynamicDataExt()
{
	m_lastExtUpdate = GetCurTick();

	if (!m->QueryHandle)
		return false;
	
	// ...

	return true;
}

void CWinProcess::UpdateExtDataIfNeeded() const
{
	if (GetCurTick() - m_lastExtUpdate < 1000)
		return;
	//((CWinProcess*)this)->UpdateDynamicDataExt();
	QMetaObject::invokeMethod(((CWinProcess*)this), "UpdateDynamicDataExt", Qt::BlockingQueuedConnection);
}*/

bool CWinProcess::UpdateThreadData(struct _SYSTEM_PROCESS_INFORMATION* Process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalCycleTime)
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

    // System Idle Process has one thread per CPU. They all have a TID of 0. We can't have duplicate
    // TIDs, so we'll assign unique TIDs.
    if (Process->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID)
    {
		for (int i = 0; i < Process->NumberOfThreads; i++)
		{
			_SYSTEM_THREAD_INFORMATION* Thread = bFullProcessInfo ? &((PSYSTEM_EXTENDED_THREAD_INFORMATION)Process->Threads)[i].ThreadInfo : &Process->Threads[i];
			Thread->ClientId.UniqueThread = UlongToHandle(i);
		}
    }

	// todo: changed removed etc events?

	QMap<quint64, CThreadPtr> OldThreads = GetThreadList();

	bool HaveFirst = OldThreads.count() > 0;

	HANDLE processHandle = NULL;
	NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)); // PROCESS_VM_READ needed to resolve service tag

	// handle threads
	for (int i = 0; i < Process->NumberOfThreads; i++)
	{
		_SYSTEM_THREAD_INFORMATION* Thread = bFullProcessInfo ? &((PSYSTEM_EXTENDED_THREAD_INFORMATION)Process->Threads)[i].ThreadInfo : &Process->Threads[i];

		quint64 ThreadID = (quint64)Thread->ClientId.UniqueThread;
		
		QSharedPointer<CWinThread> pWinThread = OldThreads.take(ThreadID).staticCast<CWinThread>();
		bool bAdd = false;
		if (pWinThread.isNull())
		{
			pWinThread = QSharedPointer<CWinThread>(new CWinThread());
			bAdd = pWinThread->InitStaticData(processHandle, Thread);
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

		bool bChanged = pWinThread->UpdateDynamicData(Thread, sysTotalTime, sysTotalCycleTime);

		if (bAdd)
			Added.insert(ThreadID);
		else if (bChanged)
			Changed.insert(ThreadID);
	}

	if(processHandle != NULL)
		NtClose(processHandle);

	//if (!HaveFirst)
	//	qDebug() << "No Main ThreadIn:" << m_ProcessName;

	QWriteLocker Locker(&m_ThreadMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 ThreadID, OldThreads.keys())
	{
		QSharedPointer<CWinThread> pWinThread = m_ThreadList.value(ThreadID).staticCast<CWinThread>();
		if (pWinThread->CanBeRemoved())
		{
			m_ThreadList.remove(ThreadID);
			Removed.insert(ThreadID); 
		}
		else if (!pWinThread->IsMarkedForRemoval())
		{
			pWinThread->MarkForRemoval();
			pWinThread->UnInit();
			Changed.insert(ThreadID); 
		}
	}
	Locker.unlock();

	m_LastUpdateThreads = GetCurTick();

	emit ThreadsUpdated(Added, Changed, Removed);

	return true;
}

bool CWinProcess::UpdateThreads()
{
	// The API calls the Above function with cached process info data to perform the update
	return ((CWindowsAPI*)theAPI)->UpdateThreads(this);
}

void CWinProcess::UnInit()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->QueryHandle != NULL) {
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}

	QWriteLocker StatsLocker(&m_StatsMutex);

	// Update the deltas.
	m_CpuStats.CpuKernelDelta.Delta = 0;
	m_CpuStats.CpuUserDelta.Delta = 0;
	m_CpuStats.CycleDelta.Delta = 0;

	m_CpuStats.ContextSwitchesDelta.Delta = 0;
	m_CpuStats.PageFaultsDelta.Delta = 0;
	m_CpuStats.HardFaultsDelta.Delta = 0;
	m_CpuStats.PrivateBytesDelta.Delta = 0;

	m_CpuStats.CpuUsage = 0;
	m_CpuStats.CpuKernelUsage = 0;
	m_CpuStats.CpuUserUsage = 0;


	m_Stats.Net.ReceiveDelta.Delta = 0;
	m_Stats.Net.SendDelta.Delta = 0;
	m_Stats.Net.ReceiveRawDelta.Delta = 0;
	m_Stats.Net.SendRawDelta.Delta = 0;
	m_Stats.Net.ReceiveRate.Clear();
	m_Stats.Net.SendRate.Clear();

	m_Stats.Io.ReadDelta.Delta = 0;
	m_Stats.Io.WriteDelta.Delta = 0;
	m_Stats.Io.OtherDelta.Delta = 0;
	m_Stats.Io.ReadRawDelta.Delta = 0;
	m_Stats.Io.WriteRawDelta.Delta = 0;
	m_Stats.Io.OtherRawDelta.Delta = 0;
	m_Stats.Io.ReadRate.Clear();
	m_Stats.Io.WriteRate.Clear();
	m_Stats.Io.OtherRate.Clear();

	m_Stats.Disk.ReadDelta.Delta = 0;
	m_Stats.Disk.WriteDelta.Delta = 0;
	m_Stats.Disk.ReadRawDelta.Delta = 0;
	m_Stats.Disk.WriteRawDelta.Delta = 0;
	m_Stats.Disk.ReadRate.Clear();
	m_Stats.Disk.WriteRate.Clear();
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

	// Note: since we are not continously monitoring all handles for all processes 
	//			when updating them on demand we waht to prevent all aprrearing as new once a proces got selected
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

		quint64 HandleID = CWinHandle::MakeID(handle->HandleValue, handle->UniqueProcessId);

		QSharedPointer<CWinHandle> pWinHandle = OldHandles.take(HandleID).staticCast<CWinHandle>();

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
			ASSERT(!m_HandleList.contains(HandleID));
			m_HandleList.insert(HandleID, pWinHandle);
		}
		
		if (WasReused)
			Removed.insert(HandleID);

		if (bAdd || WasReused)
		{
			pWinHandle->InitStaticData(handle, TimeStamp);
			Added.insert(HandleID);

			// When we don't have KPH, query handle information in parallel to take full advantage of the
            // PhCallWithTimeout functionality.
            if (useWorkQueue && handle->ObjectTypeIndex == g_fileObjectTypeIndex)
				Watchers.append(pWinHandle->InitExtDataAsync(handle,(quint64)ProcessHandle));
			else
				pWinHandle->InitExtData(handle,(quint64)ProcessHandle);
		}
		
		if (pWinHandle->UpdateDynamicData(handle,(quint64)ProcessHandle))
			Changed.insert(HandleID);
	}

	// wait for all watchers to finish
	while (!Watchers.isEmpty())
	{
		QFutureWatcher<bool>* pWatcher = Watchers.takeFirst();
		pWatcher->waitForFinished();
		pWatcher->deleteLater();
	}

	// we have to wait with free untill all async updates finished
	PhFree(handleInfo);

	QWriteLocker Locker(&m_HandleMutex);
	// purle all handles left as thay are not longer valid
	foreach(quint64 HandleID, OldHandles.keys())
	{
		QSharedPointer<CWinHandle> pWinHandle = OldHandles.value(HandleID).staticCast<CWinHandle>();
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
		if (I.value().staticCast<CWinModule>()->GetBaseAddress() == BaseAddress)
			return I;
	}

	// no matching entry
	return Modules.end();
}*/

bool CWinProcess::UpdateModules()
{
	HANDLE ProcessId = (HANDLE)GetProcessId();

	HANDLE ProcessHandle = NULL;
    // Try to get a handle with query information + vm read access.
    if (!NT_SUCCESS(PhOpenProcess(&ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, ProcessId)))
    {
        // Try to get a handle with query limited information + vm read access.
        PhOpenProcess(&ProcessHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, ProcessId);
    }

	// If we didn't get a handle when we created the provider,
    // abort (unless this is the System process - in that case
    // we don't need a handle).
    if (!ProcessHandle && ProcessId != SYSTEM_PROCESS_ID)
        return false;

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	PPH_LIST modules;
	modules = PhCreateList(100);

	PhEnumGenericModules(ProcessId, ProcessHandle, PH_ENUM_GENERIC_MAPPED_FILES | PH_ENUM_GENERIC_MAPPED_IMAGES, EnumModulesCallback, modules);

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
			bAdd = pModule->InitStaticData(module, (quint64)ProcessHandle);
			
			if (pModule->GetType() != PH_MODULE_TYPE_ELF_MAPPED_IMAGE)
				pModule->InitAsyncData();

			// todo: this should be refreshed
			if (theConf->GetBool("Options/GetServicesRefModule", true))
				pModule->ResolveRefServices();

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
			pModule = I.value().staticCast<CWinModule>();
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


	if (theConf->GetBool("Options/TraceUnloadedModules", false))
	{
		QVariantList UnloadedDLLs;
#ifdef _WIN64
		BOOLEAN isWow64;
		PhGetProcessIsWow64(ProcessHandle, &isWow64);

		if (isWow64)
		{
			QString SocketName = CTaskService::RunWorker(false, true);

			if (!SocketName.isEmpty())
			{
				QVariantMap Parameters;
				Parameters["ProcessId"] = GetProcessId();

				QVariantMap Request;
				Request["Command"] = "GetProcessUnloadedDlls";
				Request["Parameters"] = Parameters;

				QVariant Response = CTaskService::SendCommand(SocketName, Request);
				UnloadedDLLs = Response.toList();
			}
		}
		else
		{
#endif
			UnloadedDLLs = GetProcessUnloadedDlls(GetProcessId());
#ifdef _WIN64
		}
#endif

		foreach(const QVariant& vModule, UnloadedDLLs)
		{
			QVariantMap Module = vModule.toMap();

			quint64 BaseAddress = Module["BaseAddress"].toULongLong();

			/*Module["Sequence"].toInt();
			Module["Checksum"].toByteArray();*/

			QSharedPointer<CWinModule> pModule;

			bool bAdd = false;
			bool bChanged = false;
			//QMap<quint64, CModulePtr>::iterator I = FindModuleEntry(OldModules, FileName, BaseAddress);
			QMap<quint64, CModulePtr>::iterator I = OldModules.find(BaseAddress);
			if (I == OldModules.end())
			{
				pModule = QSharedPointer<CWinModule>(new CWinModule(m_ProcessId, m->IsSubsystemProcess));
				bAdd = pModule->InitStaticData(Module);
				QWriteLocker Locker(&m_ModuleMutex);
				ASSERT(!m_ModuleList.contains(pModule->GetBaseAddress()));
				m_ModuleList.insert(pModule->GetBaseAddress(), pModule);
			}
			else
			{
				pModule = I.value().staticCast<CWinModule>();
				OldModules.erase(I);

				if (pModule->IsLoaded())
				{
					pModule->SetLoaded(false);
					bChanged = true;
				}
			}

			if (bAdd)
				Added.insert(pModule->GetBaseAddress());
			else if (bChanged)
				Changed.insert(pModule->GetBaseAddress());
		}
	}


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

	NtClose(ProcessHandle);

	emit ModulesUpdated(Added, Changed, Removed);

	return true;
}

QVariantList GetProcessUnloadedDlls(quint64 ProcessId)
{
	QVariantList List;

	ULONG capturedElementSize;
	ULONG capturedElementCount;
	PVOID capturedEventTrace = NULL;
	if (NT_SUCCESS(PhGetProcessUnloadedDlls((HANDLE)ProcessId, &capturedEventTrace, &capturedElementSize, &capturedElementCount)))
	{
		PVOID currentEvent = capturedEventTrace;

		for (ULONG i = 0; i < capturedElementCount; i++)
		{
			PRTL_UNLOAD_EVENT_TRACE rtlEvent = (PRTL_UNLOAD_EVENT_TRACE)currentEvent;
			if (!rtlEvent->BaseAddress)
				continue;

			QVariantMap Module;
			Module["Sequence"] = (quint32)rtlEvent->Sequence;
			Module["ImageName"] = QString::fromWCharArray(rtlEvent->ImageName);
			Module["BaseAddress"] = (quint64)rtlEvent->BaseAddress;
			Module["Size"] = (quint64)rtlEvent->SizeOfImage;
			Module["TimeStamp"] = (quint64)rtlEvent->TimeDateStamp;
			Module["Checksum"] = QByteArray::fromRawData((char*)&rtlEvent->CheckSum, sizeof(rtlEvent->CheckSum));

			List.append(Module);

			currentEvent = PTR_ADD_OFFSET(currentEvent, capturedElementSize);
		}

		PhFree(capturedEventTrace);
	}

	return List;
}

bool CWinProcess::UpdateWindows()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	quint64 ProcessId = GetProcessId();
	QString ProcessName = GetName();
	QMap<quint64, CWndPtr> OldWindows = GetWindowList();

	QMultiMap<quint64, quint64> Windows = ((CWindowsAPI*)theAPI)->GetWindowByPID(m_ProcessId);

	for (QMultiMap<quint64, quint64>::iterator I = Windows.begin(); I != Windows.end(); I++)
	{
		quint64 ThreadID = I.key();
		QSharedPointer<CWinWnd> pWinWnd = OldWindows.take(I.value()).staticCast<CWinWnd>();

		bool bAdd = false;
		if (pWinWnd.isNull())
		{
			pWinWnd = QSharedPointer<CWinWnd>(new CWinWnd());
			pWinWnd->InitStaticData(ProcessId, I.key(), I.value(), m->QueryHandle, ProcessName);
			bAdd = true;
			QWriteLocker Locker(&m_WindowMutex);
			ASSERT(!m_WindowList.contains(I.value()));
			m_WindowList.insert(I.value(), pWinWnd);
		}

		bool bChanged = false;
		bChanged = pWinWnd->UpdateDynamicData();
			
		if (bAdd)
			Added.insert(I.value());
		else if (bChanged)
			Changed.insert(I.value());
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

void CWinProcess::AddNetworkIO(int Type, quint32 TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtwNetworkReceiveType:		m_Stats.Net.AddReceive(TransferSize); break;
	case EtwNetworkSendType:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

void CWinProcess::AddDiskIO(int Type, quint32 TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtwDiskReadType:			m_Stats.Disk.AddRead(TransferSize); break;
	case EtwDiskWriteType:			m_Stats.Disk.AddWrite(TransferSize); break;
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

quint16 CWinProcess::GetSubsystem() const
{
	QReadLocker Locker(&m_Mutex); 
	QSharedPointer<CWinMainModule> pModule = m_pModuleInfo.staticCast<CWinMainModule>();
	return pModule ? pModule->GetImageSubsystem() : 0;
}

QString CWinProcess::GetSubsystemString() const
{
    switch (GetSubsystem())
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

QString CWinProcess::GetUserName() const
{
	QReadLocker Locker(&m_Mutex); 
	return m_pToken ? m_pToken->GetUserName() : QString();
}

quint64 CWinProcess::GetProcessSequenceNumber() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m->ProcessSequenceNumber; 
}

void CWinProcess::SetRawCreateTime(quint64 TimeStamp)
{
	QWriteLocker Locker(&m_Mutex); 
	m->CreateTime.QuadPart = TimeStamp;
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);
}

quint64 CWinProcess::GetRawCreateTime() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->CreateTime.QuadPart; 
}

QString CWinProcess::GetWorkingDirectory() const
{
	QReadLocker Locker(&m_Mutex); 

	HANDLE processHandle = NULL;
	if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
	{
		// Note: this call is a bit slow updating it for all processes all the time is not nececery 
		//		if we only show this in the process general tab

		int pebOffset = PhpoCurrentDirectory;
#ifdef _WIN64
		// Tell the function to get the WOW64 current directory, because that's the one that
		// actually gets updated.
		if (m->IsWow64)
			pebOffset |= PhpoWow64;
#endif

		PPH_STRING curDir = NULL;
		PhGetProcessPebString(processHandle, (PH_PEB_OFFSET)pebOffset, &curDir);
		return CastPhString(curDir);

		NtClose(processHandle);
	}
	return QString();
}

/*QString CWinProcess::GetAppDataDirectory() const
{
	QReadLocker Locker(&m_Mutex); 

	PPH_STRING dataPath = PhGetPackageAppDataPath(m->QueryHandle);
	if (dataPath)
	{
		PPH_STRING fullDataPath = PhExpandEnvironmentStrings(&dataPath->sr);
		PhDereferenceObject(dataPath);
		return CastPhString(fullDataPath);
	}
	return QString();
}*/

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
		quint64 uParentSN = qobject_cast<CWinProcess*>(pParent)->GetProcessSequenceNumber();
		if (uParentSN != -1 && m->ProcessSequenceNumber != -1)
		{
			if (uParentSN <= m->ProcessSequenceNumber)
				return true;
			return false;
		}
	}
	
	// We make sure that the process item we found is actually the parent process - its start time
	// must not be larger than the supplied time.
	quint64 uParentCreationTime = qobject_cast<CWinProcess*>(pParent)->GetRawCreateTime();
	if (uParentCreationTime <= m->CreateTime.QuadPart)
		return true;
	return false;
}

// Flags
void  CWinProcess::MarkAsHidden()
{
	QWriteLocker Locker(&m_Mutex);
	m->IsHiddenProcess = true;
}

bool CWinProcess::IsSubsystemProcess() const
{
	QReadLocker Locker(&m_Mutex);
	return (int)m->IsSubsystemProcess;
}

QString CWinProcess::GetWindowTitle() const
{
	CWndPtr pWnd = GetMainWindow();
	return pWnd ? pWnd->GetWindowTitle() : QString();
}

QString CWinProcess::GetWindowStatusString() const
{
	CWndPtr pWnd = GetMainWindow();
	return pWnd.isNull() ? QString() : pWnd->IsHung() ? tr("Not responding") : tr("Running");
}

// OS context
quint32 CWinProcess::GetOsContextVersion() const
{
	QReadLocker Locker(&m_Mutex);
	return m->OsContextVersion;
}

QString CWinProcess::GetOsContextString() const
{
	quint32 OsContext = GetOsContextVersion();
    switch (OsContext)
    {
	case WINDOWS_ANCIENT:	return QString();
    case WINDOWS_10:		return tr("10");
    case WINDOWS_8_1:		return tr("8.1");
    case WINDOWS_8:			return tr("8");
    case WINDOWS_7:			return tr("7");
    case WINDOWS_VISTA:		return tr("Vista");
    case WINDOWS_XP:		return tr("XP");
	default:				return QString::number(OsContext);
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

	if(IsHiddenProcess())
		Status.append(tr("Hidden (!)"));
	else if(m_RemoveTimeStamp != 0)
		Status.append(tr("Terminated"));

	QReadLocker Locker(&m_Mutex); 

    if (m_IsCritical)
        Status.append(tr("Critical"));
	if (m->IsSandBoxed)
        Status.append(tr("Sandboxed"));
    if (m->IsBeingDebugged)
        Status.append(tr("Debugged"));
    if (m->IsSuspended)
        Status.append(tr("Suspended"));
    if (m->IsProtectedHandle)
        Status.append(tr("Handle Filtered"));
    if (m->IsElevated)
	//if (m_pToken && m_pToken->IsElevated())
        Status.append(tr("Elevated"));
    if (m->IsSubsystemProcess) // Set when the type of the process subsystem is other than Win32 (like *NIX, such as Ubuntu.)
        Status.append(tr("Pico"));
	if (m->IsCrossSessionCreate) // Process was created across terminal sessions. Ex: Read CreateProcessAsUser for details.
		Status.append(tr("Cross Session"));
	if (m->IsFrozen) // Immersive process is suspended (applies only to UWP processes.)
		Status.append(tr("Frozen"));
	if (m->IsBackground) // Immersive process is in the Background task mode. UWP process may temporarily switch into performing a background task
		Status.append(tr("Background"));
	if (m->IsStronglyNamed) // UWP Strongly named process. The UWP package is digitally signed. Any modifications to files inside the package can be tracked.
		Status.append(tr("Strongly Named"));
	if (m->IsSecureProcess) // Isolated User Mode process -- new security mode in Windows 10, with more stringent restrictions on what can "tap" into this process
		Status.append(tr("Secure"));
    if (m->IsImmersive)
        Status.append(tr("Immersive"));
    if (m->IsDotNet)
        Status.append(tr("DotNet"));
    if (m->IsPacked)
        Status.append(tr("Packed"));
    if (m->IsWow64)
        Status.append(tr("Wow64"));

	Locker.unlock();

    //if (IsJobProcess())
	if (IsInJob())
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
        if (debugger = PhQueryRegistryString(keyHandle, L"Debugger"))
        {
            if (PhSplitStringRefAtChar(&debugger->sr, '"', &dummy, &commandPart) &&
                PhSplitStringRefAtChar(&commandPart, '"', &commandPart, &dummy))
            {
                DebuggerCommand = PhCreateString2(&commandPart);
            }
			PhDereferenceObject(debugger);
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
	QByteArray sid = token->GetUserSid(true);
	if (sid.isEmpty() || RtlLengthSid(Sid1) != sid.size())
		return false;
	return memcmp((char*)Sid1, sid.data(), sid.size()) == 0;
}

int CWinProcess::GetKnownProcessType() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->KnownProcessType;
}

bool CWinProcess::IsWindowsProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	if(m->KnownProcessType != -1)
	{
		PH_KNOWN_PROCESS_TYPE KnownProcessType = (PH_KNOWN_PROCESS_TYPE)(m->KnownProcessType & KnownProcessTypeMask);
		if(KnownProcessType >= SystemProcessType && KnownProcessType <= WindowsOtherType)
			return true;
	}
	return false;
}

bool CWinProcess::IsSystemProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	if(PH_IS_FAKE_PROCESS_ID(m->UniqueProcessId) || m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID)
		return true;
	return RtlEqualSid(&PhSeLocalSystemSid, m_pToken);
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
	//return m_pToken && m_pToken->IsElevated();
}

bool CWinProcess::TokenHasChanged() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->TokenHasChanged;
}

bool CWinProcess::IsHiddenProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsHiddenProcess;
}

bool CWinProcess::CheckIsRunning() const
{
	QReadLocker Locker(&m_Mutex); 

	PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;
	if (m->QueryHandle && NT_SUCCESS(PhGetProcessExtendedBasicInformation(m->QueryHandle, &basicInfo)))
	{
		return !basicInfo.IsProcessDeleting;
	}
	return false;
}

/*bool CWinProcess::IsJobProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsInSignificantJob;
}*/

bool CWinProcess::IsInJob() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsInJob && !m->IsSandBoxed; // Note: sandboxie uses the job mechanism to drop process rights
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

quint64 CWinProcess::GetConsoleHostId() const
{
	QReadLocker Locker(&m_Mutex); 
	return (quint64)m->ConsoleHostProcessId;
}

QString CWinProcess::GetPackageName() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->PackageFullName;
}

QString CWinProcess::GetAppID() const
{
	QReadLocker Locker(&m_Mutex);
	if(m_pToken) {
		QString AppID = m_pToken->GetContainerName();
		if(!AppID.isEmpty()) {
			if(m->AppID != "App")
				AppID += QString(" (%1)").arg(m->AppID);
			return AppID;
		}
	}
	return m->AppID;
}

quint32 CWinProcess::GetDPIAwareness() const
{
	QReadLocker Locker(&m_Mutex); 
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

quint8 CWinProcess::GetProtection() const
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

STATUS CWinProcess::SetProtectionFlag(quint8 Flag, bool bForce)
{
	if ((((CWindowsAPI*)theAPI)->GetDriverFeatures() & (1 << 31)) == 0)
		return ERR(tr("The loaded driver does not support this feature."), STATUS_NOT_SUPPORTED);

	if (!KphIsVerified())
		return ERR(tr("The client must be verifyed by the driver in order to unlock this feature."), STATUS_ACCESS_DENIED);

	if (!bForce)
		return ERR(tr("Changing Process Protection Flags may impact system stability!"), ERROR_CONFIRM);

	if (Flag == (quint8)-1)
	{
		PS_PROTECTION Protection;
		Protection.Level = 0;
		Protection.Type = PsProtectedTypeProtected;
		Protection.Signer = 6; // WinTcb
		Flag = Protection.Level;
	}

	struct
	{
		HANDLE UniqueProcessId;
		UCHAR Flag;
		KPH_KEY Key;
	} input = { m->UniqueProcessId, Flag , 0};

	//NTSTATUS status = KphpWithKey(KphKeyLevel2, KphpSetProcessProtectionContinuation, &input);
	KphpGetL1Key(&input.Key);
	NTSTATUS status = KphpDeviceIoControl(
		XPH_SETPROCESSPROTECTION,
		&input,
		sizeof(input)
	);

	if (!NT_SUCCESS(status))
		return ERR(tr("Failed to Clear Process Protection flag"), status);
	return OK;
}

QList<QPair<QString, QString>> CWinProcess::GetMitigationDetails() const
{
	QList<QPair<QString, QString>> List;

	HANDLE ProcessId = (HANDLE)GetProcessId();

	HANDLE processHandle = NULL;
	if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
	{
		if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION, ProcessId)))
			return List;
	}

	PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION information;
    if (NT_SUCCESS(PhGetProcessMitigationPolicy(processHandle, &information)))
    {
        for (int policy = 0; policy < MaxProcessMitigationPolicy; policy++)
        {
			PPH_STRING shortDescription;
			PPH_STRING longDescription;
            if (information.Pointers[policy] && PhDescribeProcessMitigationPolicy((PROCESS_MITIGATION_POLICY)policy, information.Pointers[policy], &shortDescription, &longDescription))
            {
				List.append(qMakePair(CastPhString(shortDescription), CastPhString(longDescription)));
            }
        }
    }

	PPS_SYSTEM_DLL_INIT_BLOCK systemDllInitBlock = NULL; // this requiers PROCESS_VM_READ
    if (NT_SUCCESS(PhGetProcessSystemDllInitBlock(processHandle, &systemDllInitBlock)))
    {
		if (systemDllInitBlock && RTL_CONTAINS_FIELD(systemDllInitBlock, systemDllInitBlock->Size, MitigationOptionsMap))
        {
            if (systemDllInitBlock->MitigationOptionsMap.Map[0] & PROCESS_CREATION_MITIGATION_POLICY2_LOADER_INTEGRITY_CONTINUITY_ALWAYS_ON)
            {
				List.append(qMakePair(tr("Loader Integrity"),tr("OS signing levels for dependent module loads are enabled.")));
            }

            if (systemDllInitBlock->MitigationOptionsMap.Map[0] & PROCESS_CREATION_MITIGATION_POLICY2_MODULE_TAMPERING_PROTECTION_ALWAYS_ON)
            {
				List.append(qMakePair(tr("Module Tampering"),tr("Module Tampering protection is enabled.")));
            }

            if (systemDllInitBlock->MitigationOptionsMap.Map[0] & PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON)
            {
				List.append(qMakePair(tr("Indirect branch prediction"),tr("Protects against sibling hardware threads (hyperthreads) from interfering with indirect branch predictions.")));
            }

            if (systemDllInitBlock->MitigationOptionsMap.Map[0] & PROCESS_CREATION_MITIGATION_POLICY2_ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY_ALWAYS_ON)
            {
				List.append(qMakePair(tr("Dynamic code (downgrade)"),tr("Allows a broker to downgrade the dynamic code policy for a process.")));
            }

            if (systemDllInitBlock->MitigationOptionsMap.Map[0] & PROCESS_CREATION_MITIGATION_POLICY2_SPECULATIVE_STORE_BYPASS_DISABLE_ALWAYS_ON)
            {
				List.append(qMakePair(tr("Speculative store bypass"),tr("Disables spectre mitigations for the process.")));
            }
        }

		PhFree(systemDllInitBlock);
    }

    NtClose(processHandle);

	return List;
}

quint64 CWinProcess::GetJobObjectID() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->JobObjectId;
}

void CWinProcess::UpdateWsCounters() const
{
	if (GetCurTick() - m->LastWsCountersUpdate < 3000)
		return;

	QWriteLocker Locker(&m_Mutex); 
	m->LastWsCountersUpdate = GetCurTick();

	if(PH_IS_REAL_PROCESS_ID(m->UniqueProcessId)) // WARNING: querying WsCounters causes very high CPU load !!!
		PhGetProcessWsCounters(m->QueryHandle, &m->WsCounters); 

	PhGetProcessQuotaLimits(m->QueryHandle, &m->QuotaLimits);
}

quint64 CWinProcess::GetSharedWorkingSetSize() const
{
	UpdateWsCounters();
	QReadLocker Locker(&m_Mutex); 
	return m->WsCounters.NumberOfSharedPages * PAGE_SIZE;
}

quint64 CWinProcess::GetShareableWorkingSetSize() const
{
	UpdateWsCounters();
	QReadLocker Locker(&m_Mutex); 
	return m->WsCounters.NumberOfShareablePages * PAGE_SIZE;
}

quint64 CWinProcess::GetMinimumWS() const
{
	UpdateWsCounters();
	QReadLocker Locker(&m_Mutex); 
	return m->QuotaLimits.MinimumWorkingSetSize;
}

quint64 CWinProcess::GetMaximumWS() const
{
	UpdateWsCounters();
	QReadLocker Locker(&m_Mutex); 
	return m->QuotaLimits.MaximumWorkingSetSize;
}

quint32 CWinProcess::GetPeakNumberOfHandles() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->HandleInfo.HandleCountHighWatermark;
}

quint64 CWinProcess::GetUpTime() const 
{
	QReadLocker Locker(&m_Mutex); 
	return m->UptimeInfo.Uptime / PH_TICKS_PER_SEC;
}

quint64 CWinProcess::GetSuspendTime() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->UptimeInfo.SuspendedTime / PH_TICKS_PER_SEC;
}

int CWinProcess::GetHangCount() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->UptimeInfo.HangCount;
}

int CWinProcess::GetGhostCount() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->UptimeInfo.GhostCount;
}

QString CWinProcess::GetPriorityString(quint32 value)
{
	switch (value)
    {
	case PROCESS_PRIORITY_CLASS_REALTIME:		return tr("Real time");
    case PROCESS_PRIORITY_CLASS_HIGH:			return tr("High");
    case PROCESS_PRIORITY_CLASS_ABOVE_NORMAL:	return tr("Above normal");
    case PROCESS_PRIORITY_CLASS_NORMAL:			return tr("Normal");
	case PROCESS_PRIORITY_CLASS_BELOW_NORMAL:	return tr("Below normal");
    case PROCESS_PRIORITY_CLASS_IDLE:			return tr("Idle");
	default:									return tr("Unknown %1").arg(value);
    }
}

QString CWinProcess::GetBasePriorityString(quint32 value)
{
	return QString::number(value);
}

QString CWinProcess::GetPagePriorityString(quint32 value)
{
	switch (value)
    {
	case MEMORY_PRIORITY_NORMAL:		return tr("Normal");
    case MEMORY_PRIORITY_BELOW_NORMAL:	return tr("Below normal");
    case MEMORY_PRIORITY_MEDIUM:		return tr("Medium");
    case MEMORY_PRIORITY_LOW:			return tr("Low");
	case MEMORY_PRIORITY_VERY_LOW:		return tr("Very low");
    case MEMORY_PRIORITY_LOWEST:		return tr("Lowest");
	default:							return tr("Unknown %1").arg(value);
    }
}

QString CWinProcess::GetIOPriorityString(quint32 value)
{
	switch (value)
    {
	case IoPriorityCritical:	return tr("Critical");
    case IoPriorityHigh:		return tr("High");
    case IoPriorityNormal:		return tr("Normal");
    case IoPriorityLow:			return tr("Low");
	case IoPriorityVeryLow:		return tr("Very low");
	default:					return tr("Unknown %1").arg(value);
    }
}

STATUS CWinProcess::SetPriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	CPersistentPresetPtr PersistentPreset = m_PersistentPreset;
	if (!PersistentPreset.isNull())
		PersistentPreset->SetPriority(Value);

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

	if (NT_SUCCESS(status))
		m_Priority = Value;
	else
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

	CPersistentPresetPtr PersistentPreset = m_PersistentPreset;
	if (!PersistentPreset.isNull())
		PersistentPreset->SetPagePriority(Value);

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

    if (NT_SUCCESS(status))
		m_PagePriority = Value;
	else
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

	CPersistentPresetPtr PersistentPreset = m_PersistentPreset;
	if (!PersistentPreset.isNull())
		PersistentPreset->SetIOPriority(Value);

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

    if (NT_SUCCESS(status))
		m_IOPriority = Value;
	else
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

	CPersistentPresetPtr PersistentPreset = m_PersistentPreset;
	if (!PersistentPreset.isNull())
		PersistentPreset->SetAffinityMask(Value);

	NTSTATUS status;
	HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
    {
        status = PhSetProcessAffinityMask(processHandle, Value);
        NtClose(processHandle);
    }

	if (NT_SUCCESS(status))
		m_AffinityMask = Value;
	else
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

STATUS CWinProcess::Terminate(bool bForce)
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE processHandle;
    if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_TERMINATE, m->UniqueProcessId)))
    {
#ifndef SAFE_MODE // in safe mode always check and fail
		if (!bForce)
#endif
		{
			BOOLEAN breakOnTermination;
			PhGetProcessBreakOnTermination(processHandle, &breakOnTermination);

			if (breakOnTermination /*m_IsCritical*/)
			{
				NtClose(processHandle);
				return ERR(tr("You are about to terminate one or more critical processes. This will shut down the operating system immediately."), ERROR_CONFIRM);
			}
		}

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

bool CWinProcess::IsCriticalProcess() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m_IsCritical; 
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

bool CWinProcess::IsSandBoxed() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsSandBoxed;
}

QString CWinProcess::GetSandBoxName() const
{
	CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)theAPI)->GetSandboxieAPI();
	return pSandboxieAPI ? pSandboxieAPI->GetSandBoxName(GetProcessId()) : QString();
}

NTSTATUS CWinProcess__LoadModule(HANDLE ProcessHandle, const QString& Path)
{
	LARGE_INTEGER Timeout;
		Timeout.QuadPart = -(LONGLONG)UInt32x32To64(5, PH_TIMEOUT_SEC);
	wstring FileName = Path.toStdWString();

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

	return status;
}

STATUS CWinProcess::LoadModule(const QString& Path)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;

    HANDLE ProcessHandle;
	if (NT_SUCCESS(status = PhOpenProcess(&ProcessHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, m->UniqueProcessId)))
	{
		status = CWinProcess__LoadModule(ProcessHandle, Path);

		NtClose(ProcessHandle);
	}

	if (!NT_SUCCESS(status))
	{
		return ERR(tr("load the DLL into"), status);
	}
    return OK;
}

NTSTATUS NTAPI CWinProcess_OpenProcessPermissions(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenProcess(Handle, DesiredAccess, (HANDLE)Context);
}

void CWinProcess::OpenPermissions()
{
    PhEditSecurity(NULL, (wchar_t*)m_ProcessName.toStdWString().c_str(), L"Process", CWinProcess_OpenProcessPermissions, NULL, (HANDLE)GetProcessId());
}

CWinJobPtr CWinProcess::GetJob() const
{
	QReadLocker Locker(&m_Mutex); 

	if (!m->QueryHandle)
		return CWinJobPtr();
	return CWinJobPtr(CWinJob::JobFromProcess(m->QueryHandle));
}

QMap<quint64, CMemoryPtr> CWinProcess::GetMemoryMap() const
{
	ULONG Flags = PH_QUERY_MEMORY_REGION_TYPE | PH_QUERY_MEMORY_WS_COUNTERS;
	QMap<quint64, CMemoryPtr> MemoryMap;
	PhQueryMemoryItemList((HANDLE)GetProcessId(), Flags, MemoryMap);
	return MemoryMap;
}

QList<CWndPtr> CWinProcess::GetWindows() const
{
	bool IsImmersive = IsImmersiveProcess();

	QList<quint64> Windows;
	QList<quint64> ImmersiveWindows;

	QMultiMap<quint64, quint64> WindowList = ((CWindowsAPI*)theAPI)->GetWindowByPID(GetProcessId());
	foreach(quint64 hWnd, WindowList)
	{
		HWND WindowHandle = (HWND)hWnd;
		if (!IsWindowVisible(WindowHandle))
			continue;

		HWND parentWindow = GetParent(WindowHandle);
		if (parentWindow && IsWindowVisible(parentWindow)) // skip windows with a visible parent
			continue;

		if (PhGetWindowTextEx(WindowHandle, PH_GET_WINDOW_TEXT_INTERNAL | PH_GET_WINDOW_TEXT_LENGTH_ONLY, NULL) == 0) // skip windows with no title
			continue;

		if (IsImmersive && GetProp(WindowHandle, L"Windows.ImmersiveShell.IdentifyAsMainCoreWindow"))
			ImmersiveWindows.append(hWnd);

		WINDOWINFO windowInfo;
        windowInfo.cbSize = sizeof(WINDOWINFO);
        if (GetWindowInfo(WindowHandle, &windowInfo) && (windowInfo.dwStyle & WS_DLGFRAME))
            Windows.append(hWnd);
	}

	if (!ImmersiveWindows.isEmpty())
		Windows = ImmersiveWindows;

	((CWinProcess*)this)->UpdateWindows();

	QList<CWndPtr> WindowObjects;
	foreach(quint64 hWnd, Windows)
		WindowObjects.append(GetWindowByHwnd(hWnd));
	return WindowObjects;
}

CWndPtr	CWinProcess::GetMainWindow() const
{
	QReadLocker Locker(&m_Mutex); 
	CWndPtr pMainWnd = m_pMainWnd;
	Locker.unlock();

	// m_pMainWnd gets invalidated on each proces enumeration so no need to update here anything
	/*if (!pMainWnd.isNull())
	{
		QSharedPointer<CWinWnd> pWinWnd = pMainWnd.staticCast<CWinWnd>();
		if (pWinWnd->IsWindowValid())
			pWinWnd->UpdateDynamicData();
		else
			pMainWnd.clear();
	}*/

	if (pMainWnd.isNull() && m_WndHandles > 0)
	{
		QList<CWndPtr> Windows = GetWindows();
		if (!Windows.isEmpty())
			pMainWnd = Windows.first();

		QWriteLocker WriteLocker(&m_Mutex); 
		((CWinProcess*)this)->m_pMainWnd = pMainWnd;
	}
	return pMainWnd;
}

void CWinProcess::UpdateDns(const QString& HostName, const QList<QHostAddress>& Addresses)
{
	CProcessInfo::UpdateDns(HostName, Addresses);

	foreach(const CSocketPtr& pSocket, GetSocketList())
	{
		QSharedPointer<CWinSocket> pWinSocket = pSocket.staticCast<CWinSocket>();
		if (pWinSocket->HasDnsHostName())
			continue;
		if (Addresses.contains(pWinSocket->GetRemoteAddress()))
			pWinSocket->SetDnsHostName(HostName);
	}
}

#include <taskschd.h>

QList<CWinProcess::STask> CWinProcess::GetTasks() const
{
	QList<STask> Tasks;
	
    // Initialization code
	HRESULT result = -1;
	if(QThread::currentThread() != theAPI->thread())
		result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

	static CLSID CLSID_TaskScheduler_I = { 0x0f87369f, 0xa4e5, 0x4cfc, { 0xbd, 0x3e, 0x73, 0xe6, 0x15, 0x45, 0x72, 0xdd } };
    static IID IID_ITaskService_I = { 0x2faba4c7, 0x4da9, 0x4013, { 0x96, 0x97, 0x20, 0xcc, 0x3f, 0xd4, 0x0f, 0x85 } };

    ITaskService *taskService;

    if (SUCCEEDED(CoCreateInstance(*(_GUID*)&CLSID_TaskScheduler_I, NULL, CLSCTX_INPROC_SERVER, *(_GUID*)&IID_ITaskService_I, (PVOID*)&taskService)))
    {
        VARIANT empty = { 0 };

        if (SUCCEEDED(ITaskService_Connect(taskService, empty, empty, empty, empty)))
        {
            IRunningTaskCollection *runningTasks;

            if (SUCCEEDED(ITaskService_GetRunningTasks(
                taskService,
                TASK_ENUM_HIDDEN,
                &runningTasks
                )))
            {
                LONG count;
                LONG i;
                VARIANT index;

                index.vt = VT_INT;

                if (SUCCEEDED(IRunningTaskCollection_get_Count(runningTasks, &count)))
                {
                    for (i = 1; i <= count; i++) // collections are 1-based
                    {
                        IRunningTask *runningTask;

                        index.lVal = i;

                        if (SUCCEEDED(IRunningTaskCollection_get_Item(runningTasks, index, &runningTask)))
                        {
                            ULONG pid;
                            BSTR action = NULL;
                            BSTR path = NULL;

                            if (
                                SUCCEEDED(IRunningTask_get_EnginePID(runningTask, &pid)) &&
                                pid == GetProcessId()
                                )
                            {
                                IRunningTask_get_CurrentAction(runningTask, &action);
                                IRunningTask_get_Path(runningTask, &path);

								STask Task;
								Task.Name = action ? QString::fromWCharArray(action) : tr("Unknown action");
								Task.Path = path ? QString::fromWCharArray(path) : tr("Unknown path");
								Tasks.append(Task);

                                if (action)
                                    SysFreeString(action);
                                if (path)
                                    SysFreeString(path);
                            }

                            IRunningTask_Release(runningTask);
                        }
                    }
                }

                IRunningTaskCollection_Release(runningTasks);
            }
        }

        ITaskService_Release(taskService);
    }

    // De-initialization code
    if (result == S_OK || result == S_FALSE)
        CoUninitialize();

	return Tasks;
}

QList<CWinProcess::SDriver> CWinProcess::GetUmdfDrivers() const
{
	QList<SDriver> Drivers;

    static PH_STRINGREF activeDevices = PH_STRINGREF_INIT(L"ACTIVE_DEVICES");
    static PH_STRINGREF currentControlSetEnum = PH_STRINGREF_INIT(L"System\\CurrentControlSet\\Enum\\");

    HANDLE processHandle;
    ULONG flags = 0;
    PVOID environment;
    ULONG environmentLength;
    ULONG enumerationKey;
    PH_ENVIRONMENT_VARIABLE variable;

    if (!NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, (HANDLE)GetProcessId())))
        return Drivers;

#ifdef _WIN64
    // Just in case.
    if (IsWoW64())
        flags |= PH_GET_PROCESS_ENVIRONMENT_WOW64;
#endif

    if (NT_SUCCESS(PhGetProcessEnvironment(processHandle, flags, &environment, &environmentLength)))
    {
        enumerationKey = 0;

        while (PhEnumProcessEnvironmentVariables(environment, environmentLength, &enumerationKey, &variable))
        {
            PH_STRINGREF part;
            PH_STRINGREF remainingPart;

            if (!PhEqualStringRef(&variable.Name, &activeDevices, TRUE))
                continue;

            remainingPart = variable.Value;

            while (remainingPart.Length != 0)
            {
                PhSplitStringRefAtChar(&remainingPart, ';', &part, &remainingPart);

                if (part.Length != 0)
                {
                    HANDLE driverKeyHandle;
                    PPH_STRING driverKeyPath;

                    driverKeyPath = PhConcatStringRef2(&currentControlSetEnum, &part);

                    if (NT_SUCCESS(PhOpenKey(&driverKeyHandle, KEY_READ, PH_KEY_LOCAL_MACHINE, &driverKeyPath->sr, 0)))
                    {
                        PPH_STRING deviceDesc;
                        PH_STRINGREF deviceName;
                        PPH_STRING hardwareId;

                        if (deviceDesc = PhQueryRegistryString(driverKeyHandle, L"DeviceDesc"))
                        {
                            PH_STRINGREF firstPart;
                            PH_STRINGREF secondPart;

                            if (PhSplitStringRefAtLastChar(&deviceDesc->sr, ';', &firstPart, &secondPart))
                                deviceName = secondPart;
                            else
                                deviceName = deviceDesc->sr;
                        }
                        else
                        {
                            PhInitializeStringRef(&deviceName, L"Unknown Device");
                        }

                        hardwareId = PhQueryRegistryString(driverKeyHandle, L"HardwareID");

						SDriver Driver;
						Driver.Name = QString::fromWCharArray(deviceName.Buffer, deviceName.Length / sizeof(wchar_t));
                        if (hardwareId)
                        {
                            PhTrimToNullTerminatorString(hardwareId);
                            if (hardwareId->Length != 0)
								Driver.Path = QString::fromWCharArray(hardwareId->sr.Buffer, hardwareId->sr.Length / sizeof(wchar_t));
                        }

						Drivers.append(Driver);

                        PhClearReference((PVOID*)&hardwareId);
                        PhClearReference((PVOID*)&deviceDesc);
                        NtClose(driverKeyHandle);
                    }

                    PhDereferenceObject(driverKeyPath);
                }
            }

            break;
        }

        PhFreePage(environment);
    }

    NtClose(processHandle);

	return Drivers;
}

QString CWinProcess__QueryWmiFileName(const QString& ProviderNameSpace, const QString& ProviderName)
{
	HRESULT status;
    PPH_STRING fileName = NULL;
    PPH_STRING queryString = NULL;
    PPH_STRING clsidString = NULL;
    IWbemLocator* wbemLocator = NULL;
    IWbemServices* wbemServices = NULL;
    IEnumWbemClassObject* wbemEnumerator = NULL;
    IWbemClassObject *wbemClassObject = NULL;
    ULONG count = 0;

    if (FAILED(status = CoCreateInstance(*(_GUID*)&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, *(_GUID*)&IID_IWbemLocator, (PVOID*)&wbemLocator)))
        goto CleanupExit;

    if (FAILED(status = IWbemLocator_ConnectServer(wbemLocator, (wchar_t*)ProviderNameSpace.toStdWString().c_str(), NULL, NULL, NULL, 0, 0, NULL, &wbemServices)))
        goto CleanupExit;

    queryString = PhFormatString(L"SELECT clsid FROM __Win32Provider WHERE Name = '%s'", (wchar_t*)ProviderName.toStdWString().c_str());

    if (FAILED(status = IWbemServices_ExecQuery(wbemServices, L"WQL", queryString->Buffer, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &wbemEnumerator)))
        goto CleanupExit;

    if (SUCCEEDED(status = IEnumWbemClassObject_Next(wbemEnumerator, WBEM_INFINITE,  1,  &wbemClassObject,  &count)))
    {
        VARIANT variant;

        if (SUCCEEDED(IWbemClassObject_Get(wbemClassObject, L"CLSID", 0, &variant, 0, 0)))
        {
            if (variant.bstrVal) // returns NULL for some host processes (dmex)
            {
                clsidString = PhCreateString(variant.bstrVal);
            }

            VariantClear(&variant);
        }

        IWbemClassObject_Release(wbemClassObject);
    }

    // Lookup the GUID in the registry to determine the name and file name.

    if (!PhIsNullOrEmptyString(clsidString))
    {
        HANDLE keyHandle;
        PPH_STRING keyPath;

        // Note: String pooling optimization.
        keyPath = PhConcatStrings(4, L"CLSID\\", clsidString->Buffer, L"\\", L"InprocServer32");

        if (SUCCEEDED(status = HRESULT_FROM_NT(PhOpenKey(&keyHandle, KEY_READ, PH_KEY_CLASSES_ROOT, &keyPath->sr, 0))))
        {
            if (fileName = PhQueryRegistryString(keyHandle, NULL))
            {
                PPH_STRING expandedString;

                if (expandedString = PhExpandEnvironmentStrings(&fileName->sr))
                {
                    PhMoveReference((PVOID*)&fileName, expandedString);
                }
            }

            NtClose(keyHandle);
        }

        PhDereferenceObject(keyPath);
    }

CleanupExit:
    if (clsidString)
        PhDereferenceObject(clsidString);
    if (queryString)
        PhDereferenceObject(queryString);
    if (wbemEnumerator)
        IEnumWbemClassObject_Release(wbemEnumerator);
    if (wbemServices)
        IWbemServices_Release(wbemServices);
    if (wbemLocator)
        IWbemLocator_Release(wbemLocator);

    if (SUCCEEDED(status))
        return CastPhString(fileName);
	return QString();
}

QList<CWinProcess::SWmiProvider> CWinProcess::QueryWmiProviders() const
{
	QList<SWmiProvider> Providers;

    // Initialization code
	HRESULT result = -1;
	if (QThread::currentThread() != theAPI->thread())
		result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HRESULT status;
    PPH_STRING queryString = NULL;
    IWbemLocator* wbemLocator = NULL;
    IWbemServices* wbemServices = NULL;
    IEnumWbemClassObject* wbemEnumerator = NULL;
    IWbemClassObject *wbemClassObject;

    if (FAILED(status = CoCreateInstance(*(_GUID*)&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, *(_GUID*)&IID_IWbemLocator, (PVOID*)&wbemLocator)))
        goto CleanupExit;

    if (FAILED(status = IWbemLocator_ConnectServer(wbemLocator, L"root\\CIMV2", NULL, NULL, NULL, 0, 0, NULL, &wbemServices)))
        goto CleanupExit;

    queryString = PhConcatStrings2(L"SELECT Namespace,Provider,User FROM Msft_Providers WHERE HostProcessIdentifier = ", (wchar_t*)QString::number(GetProcessId()).toStdWString().c_str());

    if (FAILED(status = IWbemServices_ExecQuery(wbemServices, L"WQL", queryString->Buffer, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &wbemEnumerator)))
        goto CleanupExit;

    while (TRUE)
    {
        ULONG count = 0;
        VARIANT variant;

        if (FAILED(IEnumWbemClassObject_Next(wbemEnumerator, WBEM_INFINITE, 1, &wbemClassObject, &count)))
            break;

        if (count == 0)
            break;

		SWmiProvider Provider;

        if (SUCCEEDED(IWbemClassObject_Get(wbemClassObject, L"Namespace", 0, &variant, 0, 0)))
        {
            Provider.NamespacePath = QString::fromWCharArray(variant.bstrVal);
            VariantClear(&variant);
        }

        if (SUCCEEDED(IWbemClassObject_Get(wbemClassObject, L"Provider", 0, &variant, 0, 0)))
        {
            Provider.ProviderName = QString::fromWCharArray(variant.bstrVal);
            VariantClear(&variant);
        }

        if (SUCCEEDED(IWbemClassObject_Get(wbemClassObject, L"User", 0, &variant, 0, 0)))
        {
            Provider.UserName = QString::fromWCharArray(variant.bstrVal);
            VariantClear(&variant);
        }

        IWbemClassObject_Release(wbemClassObject);

        if (!Provider.NamespacePath.isEmpty() && !Provider.ProviderName.isEmpty())
            Provider.FileName = CWinProcess__QueryWmiFileName(Provider.NamespacePath, Provider.ProviderName);

		Providers.append(Provider);
    }

CleanupExit:
    if (queryString)
        PhDereferenceObject(queryString);
    if (wbemEnumerator)
        IEnumWbemClassObject_Release(wbemEnumerator);
    if (wbemServices)
        IWbemServices_Release(wbemServices);
    if (wbemLocator)
        IWbemLocator_Release(wbemLocator);

    // De-initialization code
    if (result == S_OK || result == S_FALSE)
        CoUninitialize();

	return Providers;
}