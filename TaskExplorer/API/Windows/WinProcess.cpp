/*
 * Task Explorer -
 *   qt wrapper and support functions based on procprv.c
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2022 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "ProcessHacker.h"
#include "ProcessHacker/ProcMtgn.h"
#include <lsasup.h>
#include <userenv.h>

#include "WindowsAPI.h"
#include "WinProcess.h"
#include "../../SVC/WndAgents.h"
#include "WinSecurityEditor.h"
#include "ProcessHacker/AssemblyEnum.h"
#include "WinThread.h"
#include "WinHandle.h"
#include "WinModule.h"
#include "WinWnd.h"
#include "WinHeap.h"
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
		LxssProcessId = NULL;
		QueryHandle = NULL;

		SessionId = -1;
		CreateTime.QuadPart = 0;

		Flags = 0;
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
		SharedCommitCharge = 0;
		DpiAwareness = -1;

		Architecture = IMAGE_FILE_MACHINE_UNKNOWN;

		// Signature, Packed
		//ImportFunctions;
		//ImportModules;

		// OS Context
		memset(&OsContextGuid, 0, sizeof(GUID));
		OsContextVersion = 0;

		// Misc.
		DepStatus = 0;

		CodePage = 0;
		TlsBitmapCount = 0;
		ErrorMode = 0;

		KnownProcessType = -1;

		KphProcessState = (KPH_PROCESS_STATE)0;

		ReferenceCount = 0;
		AccessMask = 0;

		FreezeHandle = NULL;
	}

	
	// Handles
	HANDLE UniqueProcessId;
	ULONG LxssProcessId;
	HANDLE QueryHandle;

	// Basic
	quint64 SessionId;
	LARGE_INTEGER CreateTime;

	// Flags
	union
	{
		quint64 Flags;
		struct
		{
			quint64
			//UpdateIsDotNet : 1,
			IsBeingDebugged : 1,
			IsDotNet : 1,
			IsElevated : 1,
			IsInJob : 1,
			IsInSignificantJob : 1,
			IsPacked : 1,
			IsHandleFull : 1, // IsHandleValid - if this is true we can querry handles
			IsSuspended : 1,
			IsWow64Process : 1,
			IsImmersive : 1,
			IsPartiallySuspended : 1,
			IsProtectedHandle : 1,
			IsProtectedProcess : 1,
			IsSecureProcess : 1,
			IsSubsystemProcess : 1,
			IsPackagedProcess : 1,
			IsBackgroundProcess : 1,
			IsCrossSessionProcess : 1,
			IsReflectedProcess : 1,
			IsFrozenProcess : 1,
			IsUIAccessEnabled : 1,
			IsControlFlowGuardEnabled : 1,
			IsCetEnabled : 1,
			IsXfgEnabled : 1,
			IsXfgAuditEnabled : 1,
			IsPowerThrottling : 1, 
			IsSystemProcess : 1,
			IsSecureSystem : 1,
			Spare1 : 4,


			IsHandleVmRead : 1,
			IsProcessDeleting : 1,
			IsOrWasRunning : 1,
			TokenHasChanged : 1,
			IsHiddenProcess : 1,
			IsSandBoxed : 1,
			IsCetStrictModeEnabled : 1,
			ModPagesLoaded : 1,
			Spare2 : 24;
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
	SIZE_T SharedCommitCharge;
	QString PackageFullName;
	QString AppID;
	ULONG DpiAwareness;

	quint16	Architecture;

	// Signature, Packed
	//quint32 ImportFunctions;
	//quint32 ImportModules;

	// OS Context
    GUID OsContextGuid;
    quint32 OsContextVersion;

	// Misc.
	ULONG DepStatus;

	USHORT CodePage;
	USHORT TlsBitmapCount;
	ULONG ErrorMode;

	int KnownProcessType;

	KPH_PROCESS_STATE KphProcessState;

	ULONG ReferenceCount;
	ULONG AccessMask;

	QString DesktopInfo;

	HANDLE FreezeHandle;
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

	m_StartKey = 0;

	m_MiscStates = 0;

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
	m_CreateTimeStamp = GetTime() * 1000; // InitStaticData should overwrite it with correct value

	if (!InitStaticData())
	{
		m_ProcessName = MakePlaceholder(TE_NAME_UNKNOWN_PROCESS, ProcessId);
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
			//m_FileName = CastPhString(PhGetFileName(fileName));
			m_FileNameNt = CastPhString(fileName);

			int pos = m_FileNameNt.lastIndexOf("\\");
			m_ProcessName = m_FileNameNt.mid(pos+1);
		}
		else
			m_ProcessName = QString::fromWCharArray(Process->ImageName.Buffer, Process->ImageName.Length / sizeof(wchar_t));
	}
	else
		m_ProcessName = MakePlaceholder(TE_NAME_SYSTEM_IDLE_PROCESS);

	m->CreateTime = Process->CreateTime;
	m_ProcessUId = SProcessUID(m_ProcessId, m->CreateTime.QuadPart);
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);

	if(m->QueryHandle == NULL) // we may already have opened the handle and initialized some data if the process was seen in an ETW or FW event
		InitStaticData(!bFullProcessInfo);

	// On Windows 8.1 and above, processes without threads are reflected processes
	// which will not terminate if we have a handle open. (wj32)
	if (Process->NumberOfThreads == 0 && m->QueryHandle)
	{
		m->IsReflectedProcess = TRUE;

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
				if (SUCCEEDED(PhAppResolverGetAppIdForProcess(m->UniqueProcessId, &applicationUserModelId)))
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
		// READ_CONTROL required for PhGetProcessMandatoryPolicy
		if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | READ_CONTROL, m->UniqueProcessId))
		 || NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
		{
			m->IsHandleFull = TRUE;
			m->IsHandleVmRead = TRUE;
		}
		else if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ | READ_CONTROL, m->UniqueProcessId))
			|| NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_VM_READ, m->UniqueProcessId)))
		{
			m->IsHandleVmRead = TRUE;
		}
		else if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_INFORMATION | READ_CONTROL, m->UniqueProcessId))
		      || NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_INFORMATION, m->UniqueProcessId)))
		{
			m->IsHandleFull = TRUE;
		}
		else if (NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_LIMITED_INFORMATION | READ_CONTROL, m->UniqueProcessId))
			  || NT_SUCCESS(PhOpenProcess(&m->QueryHandle, m->AccessMask = PROCESS_QUERY_LIMITED_INFORMATION, m->UniqueProcessId)))
		{

		}
		else
			qDebug() << "failed to open QueryHandle for" << m_ProcessId;
	}

	if (m->QueryHandle && !m->CreateTime.QuadPart)
	{
		KERNEL_USER_TIMES times;
		if (NT_SUCCESS(PhGetProcessTimes(m->QueryHandle, &times))) {
			m->CreateTime = times.CreateTime;
			m_ProcessUId = SProcessUID(m_ProcessId, m->CreateTime.QuadPart);
			m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);
		}
	}

	// Process flags
	if (m->QueryHandle)
	{
		PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;

		if (NT_SUCCESS(PhGetProcessExtendedBasicInformation(m->QueryHandle, &basicInfo)))
		{
			m->IsProtectedProcess = basicInfo.IsProtectedProcess;
			m->IsProcessDeleting = basicInfo.IsProcessDeleting;
			m->IsCrossSessionProcess = basicInfo.IsCrossSessionCreate;
			m->IsFrozenProcess = basicInfo.IsFrozen;
			m->IsBackgroundProcess = basicInfo.IsBackground;
			m->IsPackagedProcess= basicInfo.IsStronglyNamed;
			m->IsSecureProcess = basicInfo.IsSecureProcess;
			m->IsSubsystemProcess = basicInfo.IsSubsystemProcess;
			m->IsWow64Process = basicInfo.IsWow64Process;
		}

		USHORT processArchitecture;

		if (NT_SUCCESS(PhGetProcessArchitecture(m->QueryHandle, &processArchitecture)))
		{
			m->Architecture = processArchitecture;
		}
	}

	ULONGLONG processStartKey;
	if (NT_SUCCESS(PhGetProcessStartKey(m->QueryHandle, &processStartKey)))
	{
		m_StartKey = processStartKey;
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
			m_FileNameNt = CastPhString(fileName);
		}
	}
	else
	{
		PPH_STRING fileNameWin32 = NULL;
		if (m->QueryHandle) {
			if(NT_SUCCESS(PhGetProcessImageFileNameWin32(m->QueryHandle, &fileNameWin32)))
				m_FileName = CastPhString(fileNameWin32);
		}

		if (bLoadFileName)
		{
			NTSTATUS status = STATUS_UNSUCCESSFUL;

			PPH_STRING fileName;
			if (m->QueryHandle && !m->IsSubsystemProcess)
				status = PhGetProcessImageFileName(m->QueryHandle, &fileName);
			if (!NT_SUCCESS(status))
				status = PhGetProcessImageFileNameByProcessId(m->UniqueProcessId, &fileName);
			if (NT_SUCCESS(status))
			{
				if(fileNameWin32 == NULL)
					m_FileName = CastPhString(PhGetFileName(fileName));
				m_FileNameNt = CastPhString(fileName);
			}
		}
	}

	// Token information
	if (m->UniqueProcessId == SYSTEM_IDLE_PROCESS_ID || m->UniqueProcessId == SYSTEM_PROCESS_ID)
	{
		m_pToken = CWinTokenPtr(CWinToken::NewSystemToken(GetSystem())); // System token can't be opened (dmex)
	}
	// Note: See comment in UpdateDynamicData
	else if (m->QueryHandle)
	{
		m_pToken = CWinTokenPtr(CWinToken::TokenFromProcess(GetSystem(), m->QueryHandle));

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

		if (WindowsVersion >= WINDOWS_11)
		{
			BOOLEAN xfguardEnabled;
			BOOLEAN xfguardAuditEnabled;

			if (NT_SUCCESS(PhGetProcessIsXFGuardEnabled(m->QueryHandle, &xfguardEnabled, &xfguardAuditEnabled)))
			{
				m->IsXfgEnabled = xfguardEnabled;
				m->IsXfgAuditEnabled = xfguardAuditEnabled;
			}
		}
	}

	// CET
	if (WindowsVersion >= WINDOWS_10_20H1)
	{
		if (m_ProcessId == (quint64)SYSTEM_PROCESS_ID)
		{
			SYSTEM_SHADOW_STACK_INFORMATION shadowStackInformation;

			if (NT_SUCCESS(PhGetSystemShadowStackInformation(&shadowStackInformation)))
			{
				m->IsCetEnabled = shadowStackInformation.KernelCetEnabled; // Kernel CET is always strict (TheEragon)
			}
		}
		else
		{
			if (m->QueryHandle)
			{
				BOOLEAN cetEnabled;
				BOOLEAN cetStrictModeEnabled;

				if (NT_SUCCESS(PhGetProcessIsCetEnabled(m->QueryHandle, &cetEnabled, &cetStrictModeEnabled)))
				{
					m->IsCetEnabled = cetEnabled;
					m->IsCetStrictModeEnabled = cetStrictModeEnabled;
				}
			}
		}
	}

	// codepage, very slow on win 10 !!!
	if (m->IsHandleVmRead)
	{
		USHORT codePage;
		if (NT_SUCCESS(PhGetProcessCodePage(m->QueryHandle, &codePage)))
		{
			m->CodePage = codePage;
		}
	}

	// WSL
	if (WindowsVersion >= WINDOWS_10_22H2 && m->QueryHandle)
	{
		if (m->IsSubsystemProcess && KsiLevel() >= KphLevelMed)
		{
			ULONG lxssProcessId;
			if (NT_SUCCESS(KphQueryInformationProcess(m->QueryHandle, KphProcessWSLProcessId, &lxssProcessId, sizeof(ULONG), NULL)))
			{
				m->LxssProcessId = lxssProcessId;
			}
		}
	}

	// PhpFillProcessItemExtension is done in UpdateDynamicData which is to be called right after InitStaticData

    // Add service names to the process item.
	m_ServiceList = ((CWindowsAPI*)GetSystem().data())->GetServicesByPID(m_ProcessId);

	// Note: on the first listing ProcessHacker does this asynchroniusly, on subsequent listings synchroniusly

	// PhpProcessQueryStage1 Begin
	NTSTATUS status;

    // Command line, .NET
    if (m->QueryHandle && !m->IsSubsystemProcess)
    {
        BOOLEAN isDotNet = FALSE;
        ULONG processQueryFlags = 0;

        if (WindowsVersion >= WINDOWS_8_1)
        {
            processQueryFlags |= PH_CLR_USE_SECTION_CHECK;
            status = STATUS_SUCCESS;
        }
        else if(!m->IsHandleVmRead)
            status = STATUS_UNSUCCESSFUL;

        if (NT_SUCCESS(status))
        {
            PPH_STRING commandLine;

            if (NT_SUCCESS(PhGetProcessCommandLine(m->QueryHandle, &commandLine)))
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
            PhGetProcessIsDotNetEx(m->UniqueProcessId, m->QueryHandle,
#ifdef _WIN64
                processQueryFlags | PH_CLR_NO_WOW64_CHECK | (m->IsWow64Process ? PH_CLR_KNOWN_IS_WOW64 : 0),
#else
                processQueryFlags,
#endif
                &isDotNet, NULL);
            m->IsDotNet = isDotNet;
        }
    }

    // Job
    // Note: this is done during dynamic update

    // Console host process
    if (m->QueryHandle)
        PhGetProcessConsoleHostProcessId(m->QueryHandle, &m->ConsoleHostProcessId);

	// UWP...

    if (m->QueryHandle && m->IsHandleFull)
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

	CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)GetSystem().data())->GetSandboxieAPI();
	m->IsSandBoxed = pSandboxieAPI ? pSandboxieAPI->IsSandBoxed(m_ProcessId) : false;

	if (!m->IsSubsystemProcess)
	{	
		if (m->IsHandleVmRead)
		{
			// OS context
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

			// desktop
			PPH_STRING desktopinfo;
			if (NT_SUCCESS(PhGetProcessDesktopInfo(m->QueryHandle, &desktopinfo)))
			{
				m_UsedDesktop = CastPhString(desktopinfo);
			}
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
	pModule->SetSystem(GetSystem());
	m_pModuleInfo = CModulePtr(pModule);
	connect(pModule, SIGNAL(AsyncDataDone(bool, quint32, quint32)), this, SLOT(OnAsyncDataDone(bool, quint32, quint32)));
	pModule->InitStaticData(m_ProcessId, (quint64)m->QueryHandle, m_FileName, m_FileNameNt, m->IsSubsystemProcess, m->IsWow64Process);
	pModule->InitAsyncData(m->PackageFullName);
}

void CWinProcess::SetFileName(const QString& FileName, const QString& FileNameNt)
{ 
	QWriteLocker Locker(&m_Mutex); 
	m_FileName = FileName; 
	m_FileNameNt = FileNameNt;

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

bool CWinProcess::UpdateDynamicData(struct _SYSTEM_PROCESS_INFORMATION* Process, bool bFullProcessInfo, quint64 sysTotalTime, quint64 sysTotalTimePerCPU)
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

	_SYSTEM_THREAD_INFORMATION* max_Thread = NULL;

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

		if(max_Thread == NULL || (max_Thread->KernelTime.QuadPart + max_Thread->UserTime.QuadPart) < (Thread->KernelTime.QuadPart + Thread->UserTime.QuadPart))
			max_Thread = Thread;
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

		UCHAR PriorityClass;
		if (NT_SUCCESS(PhGetProcessPriorityClass(m->QueryHandle, &PriorityClass)) && m_Priority != PriorityClass)
		{
			PriorityChanged = true;
			m_Priority = PriorityClass;
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
	m_WorkingSetPrivateSize = Process->WorkingSetPrivateSize;
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
		m->SharedCommitCharge = processExtension->SharedCommitCharge;
		m->ProcessSequenceNumber = processExtension->ProcessSequenceNumber;
		m->IsSystemProcess = processExtension->Classification != SystemProcessClassificationNormal;
		m->IsSecureSystem = processExtension->Classification == SystemProcessClassificationSecureSystem;
	}

	if (m->QueryHandle)
	{
		// Token information
		//UpdateTokenData();

		// Job
		if (!processExtension || m->JobObjectId != 0) // Note: if we don't have the processExtension we need to try every process
		{
			BOOLEAN isInSignificantJob = FALSE;
			BOOLEAN isInJob = FALSE;

			if (KsiLevel() >= KphLevelMed)
			{
				HANDLE jobHandle = NULL;

				NTSTATUS status = KphOpenProcessJob(m->QueryHandle, JOB_OBJECT_QUERY, &jobHandle);

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
			else
			{
				NTSTATUS status = NtIsProcessInJob(m->QueryHandle, NULL);
				if (NT_SUCCESS(status))
					isInJob = (status == STATUS_PROCESS_IN_JOB);
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

		if (KphCommsIsConnected()) {
			KPH_PROCESS_BASIC_INFORMATION info = { 0 };
			if (NT_SUCCESS(KphQueryInformationProcess(m->QueryHandle, KphProcessBasicInformation, &info, sizeof(info), NULL)))
				m->KphProcessState = info.ProcessState;
		}


		//if(PH_IS_REAL_PROCESS_ID(m->UniqueProcessId)) // WARNING: querying WsCounters causes very high CPU load !!!
		//	PhGetProcessWsCounters(m->QueryHandle, &m->WsCounters); 

		//PhGetProcessQuotaLimits(m->QueryHandle, &m->QuotaLimits); // this is by far not as cpu intensive but lets handle it the same

		PhGetProcessHandleCount(m->QueryHandle, &m->HandleInfo);

		if (WindowsVersion >= WINDOWS_10_RS3)
			PhGetProcessUptime(m->QueryHandle, &m->UptimeInfo);

		BOOLEAN priorityBoostDisabled;
		if (NT_SUCCESS(PhGetProcessPriorityBoost(m->QueryHandle, &priorityBoostDisabled))) 
		{
			m_PriorityBoost = priorityBoostDisabled;
		}

		if (WindowsVersion >= WINDOWS_11_22H2 && !m->IsSubsystemProcess)
		{
			POWER_THROTTLING_PROCESS_STATE powerThrottlingState;
			if (NT_SUCCESS(PhGetProcessPowerThrottlingState(m->QueryHandle, &powerThrottlingState)))
			{
				m->IsPowerThrottling = FALSE;

				if (FlagOn(powerThrottlingState.ControlMask, POWER_THROTTLING_PROCESS_EXECUTION_SPEED) &&
					FlagOn(powerThrottlingState.StateMask, POWER_THROTTLING_PROCESS_EXECUTION_SPEED))
				{
					m->IsPowerThrottling = TRUE;
				}

				if (FlagOn(powerThrottlingState.ControlMask, POWER_THROTTLING_PROCESS_DELAYTIMERS) &&
					FlagOn(powerThrottlingState.StateMask, POWER_THROTTLING_PROCESS_DELAYTIMERS))
				{
					m->IsPowerThrottling = TRUE;
				}

				if (FlagOn(powerThrottlingState.ControlMask, POWER_THROTTLING_PROCESS_IGNORE_TIMER_RESOLUTION) &&
					FlagOn(powerThrottlingState.StateMask, POWER_THROTTLING_PROCESS_IGNORE_TIMER_RESOLUTION))
				{
					m->IsPowerThrottling = TRUE;
				}
			}
		}

		ULONG errorMode;
		if (NT_SUCCESS(PhGetProcessErrorMode(m->QueryHandle, &errorMode)) && errorMode > 0)
		{
			m->ErrorMode = errorMode;
		}

		if (m->IsHandleVmRead)
		{
			ULONG bitmapCount;
			ULONG bitmapExpansionCount;
			if (NT_SUCCESS(PhGetProcessTlsBitMapCounters(m->QueryHandle, &bitmapCount, &bitmapExpansionCount)))
			{
				m->TlsBitmapCount = (USHORT)(bitmapCount + bitmapExpansionCount);
			}
		}

		m->ReferenceCount = 0;
		OBJECT_BASIC_INFORMATION basicInfo;
		if (NT_SUCCESS(PhQueryObjectBasicInformation(m->QueryHandle, &basicInfo)))
		{
			m->ReferenceCount = basicInfo.HandleCount;
		}
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

	if(max_Thread)
	{
		m_CpuStats2.CpuKernelDelta.Update(max_Thread->KernelTime.QuadPart);
		m_CpuStats2.CpuUserDelta.Update(max_Thread->UserTime.QuadPart);
		//m_CpuStats2.CycleDelta.Update(); // todo

		m_CpuStats2.UpdateStats(sysTotalTimePerCPU);
	}

	m_Stats.Io.SetRead(Process->ReadTransferCount.QuadPart, Process->ReadOperationCount.QuadPart);
	m_Stats.Io.SetWrite(Process->WriteTransferCount.QuadPart, Process->WriteOperationCount.QuadPart);
	m_Stats.Io.SetOther(Process->OtherTransferCount.QuadPart, Process->OtherOperationCount.QuadPart);

	if (((CWindowsAPI*)GetSystem().data())->UseDiskCounters() && processExtension)
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

	// handle threads
	for (int i = 0; i < Process->NumberOfThreads; i++)
	{
		_SYSTEM_THREAD_INFORMATION* Thread = bFullProcessInfo ? &((PSYSTEM_EXTENDED_THREAD_INFORMATION)Process->Threads)[i].ThreadInfo : &Process->Threads[i];

		quint64 ThreadID = (quint64)Thread->ClientId.UniqueThread;
		
		QSharedPointer<CWinThread> pWinThread = OldThreads.take(ThreadID).staticCast<CWinThread>();
		bool bAdd = false;
		if (pWinThread.isNull())
		{
			// Destroyed through deleteLater; see CWindowsAPI::UpdateProcessList.
			pWinThread = QSharedPointer<CWinThread>(new CWinThread(), &QObject::deleteLater);
			pWinThread->SetSystem(GetSystem());
			if (m->IsSandBoxed)
				pWinThread->SetSandboxed();
			bAdd = pWinThread->InitStaticData(m->QueryHandle, Thread);
			GetSystem()->AddThread(pWinThread);
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
	return ((CWindowsAPI*)GetSystem().data())->UpdateThreads(this);
}

void CWinProcess::CloseHandle()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->QueryHandle != NULL) {
		NtClose(m->QueryHandle);
		m->QueryHandle = NULL;
	}
}

void CWinProcess::UnInit()
{
	CloseHandle();

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

	BOOLEAN useWorkQueue = !KphCommsIsConnected();
	QList<QFutureWatcher<bool>*> Watchers;

	for (int i = 0; i < handleInfo->NumberOfHandles; i++)
	{
		PSYSTEM_HANDLE_TABLE_ENTRY_INFO_EX handle = &handleInfo->Handles[i];

		// Skip irrelevant handles.
		if (filterNeeded && handle->UniqueProcessId != m->UniqueProcessId)
			continue;

		quint64 HandleID = CWinHandle::MakeID((quint64)handle->HandleValue, (quint64)handle->UniqueProcessId);

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
			pWinHandle->SetSystem(GetSystem());
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

typedef struct _PH_IMAGE_MAPPED_BASE_ENTRY
{
	PVOID ImageBase;
	SIZE_T SizeOfImage;
	//QString FileName;
} PH_IMAGE_MAPPED_BASE_ENTRY, *PPH_IMAGE_MAPPED_BASE_ENTRY;

typedef struct _PH_IMAGE_MAPPED_FAILURE_ENTRY
{
	PVOID BaseAddress;
	SIZE_T SizeOfImage;
	PVOID VirtualAddress;
	union
	{
		ULONG Flags;
		struct
		{
			ULONG Valid : 1;
			ULONG Unused : 31;
		};
	};
} PH_IMAGE_MAPPED_FAILURE_ENTRY, *PPH_IMAGE_MAPPED_FAILURE_ENTRY;

struct SEnumImagesForTamperingContext
{
	QList<PH_IMAGE_MAPPED_BASE_ENTRY> ImageAddressList;
	QList<PH_IMAGE_MAPPED_FAILURE_ENTRY> PageTamperingList;
	int ImageQueryFailures = 0;
};

NTSTATUS NTAPI PhEnumImagesForTamperingCallback(
	_In_ HANDLE ProcessHandle,
	_In_ PMEMORY_BASIC_INFORMATION BasicInformation,
	_In_ PVOID Context
)
{
	SEnumImagesForTamperingContext* context = (SEnumImagesForTamperingContext*)Context;

	if (
		BasicInformation->Type == MEM_IMAGE &&
		BasicInformation->AllocationBase == BasicInformation->BaseAddress
		)
	{
		PVOID imageBase;
		SIZE_T imageSize;

		if (NT_SUCCESS(PhGetProcessMappedImageBaseFromAddress(
			ProcessHandle,
			BasicInformation->BaseAddress,
			&imageBase,
			&imageSize
		)))
		{
			BOOLEAN found = FALSE;

			for (ULONG i = 0; i < context->ImageAddressList.count(); i++)
			{
				PPH_IMAGE_MAPPED_BASE_ENTRY entry = &context->ImageAddressList[i];

				if (entry->ImageBase == imageBase)
				{
					found = TRUE;
					break;
				}
			}

			if (!found)
			{
				PH_IMAGE_MAPPED_BASE_ENTRY entry;
				entry.ImageBase = imageBase;
				entry.SizeOfImage = imageSize;

				//PPH_STRING fileName;
				//if (NT_SUCCESS(PhGetProcessMappedFileName(ProcessHandle, imageBase, &fileName)))
				//{
				//	entry.FileName = CastPhString(fileName);
				//}

				context->ImageAddressList.append(entry);
			}
		}
		else
		{
			context->ImageQueryFailures++;
		}
	}

	return STATUS_SUCCESS;
}

NTSTATUS NTAPI PhpEnumVirtualMemoryAttributesCallback(
	_In_ HANDLE ProcessHandle,
	_In_ PVOID BaseAddress,
	_In_ SIZE_T SizeOfImage,
	_In_ ULONG_PTR NumberOfEntries,
	_In_ PMEMORY_WORKING_SET_EX_INFORMATION Blocks,
	_In_ PVOID Context
)
{
	SEnumImagesForTamperingContext* context = (SEnumImagesForTamperingContext*)Context;

	for (ULONG_PTR i = 0; i < NumberOfEntries; i++)
	{
		PMEMORY_WORKING_SET_EX_INFORMATION page = &Blocks[i];
		PMEMORY_WORKING_SET_EX_BLOCK block = &page->VirtualAttributes;

		if (!block->SharedOriginal)
		{
			PH_IMAGE_MAPPED_FAILURE_ENTRY entry;
			entry.BaseAddress = BaseAddress;
			entry.SizeOfImage = SizeOfImage;
			entry.VirtualAddress = page->VirtualAddress;
			entry.Valid = !!block->Valid;

			context->PageTamperingList.append(entry);
		}
	}

	return STATUS_SUCCESS;
}

bool CWinProcess::UpdateModulesList(bool bWithModPages)
{
	HANDLE ProcessId = (HANDLE)GetProcessId();

	// If we didn't get a handle when we created the provider,
    // abort (unless this is the System process - in that case
    // we don't need a handle).
    if (!m->IsHandleVmRead && ProcessId != SYSTEM_PROCESS_ID)
        return false;

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	PPH_LIST modules;
	modules = PhCreateList(100);

	PhEnumGenericModules(ProcessId, m->QueryHandle, PH_ENUM_GENERIC_MAPPED_FILES | PH_ENUM_GENERIC_MAPPED_IMAGES, EnumModulesCallback, modules);

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
			pModule->SetSystem(GetSystem());
			bAdd = pModule->InitStaticData(module, (quint64)m->QueryHandle);
			
			if (pModule->GetType() != -1)
				pModule->InitAsyncData();

			// todo: this should be refreshed
			if (theConf->GetBool("Options/GetServicesRefModule", true))
				pModule->ResolveRefServices();

			// remove CF Guard flag if CFG mitigation is not enabled for the process
			if (!m->IsControlFlowGuardEnabled)
				pModule->ClearControlFlowGuardEnabled();

			// Add CET flag when strict mode is enabled for the process.
			if (!m->IsCetStrictModeEnabled)
				pModule->SetCetEnabled();

			// Remove CET flag when CET is not enabled for the process.
			if (!m->IsCetEnabled)
				pModule->ClearCetEnabled();

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
		if (m->IsWow64Process)
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
				pModule->SetSystem(GetSystem());
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

	if (bWithModPages)
	{
		SEnumImagesForTamperingContext context;

		NTSTATUS status = PhEnumVirtualMemory(m->QueryHandle, PhEnumImagesForTamperingCallback, &context);

		if (NT_SUCCESS(status))
		{
			m->ModPagesLoaded = true;

			for (ULONG i = 0; i < context.ImageAddressList.count(); i++)
			{
				PPH_IMAGE_MAPPED_BASE_ENTRY entry = &context.ImageAddressList[i];

				// PhCheckImagePagesForTampering
				status = PhEnumVirtualMemoryAttributes(m->QueryHandle, entry->ImageBase, entry->SizeOfImage, PhpEnumVirtualMemoryAttributesCallback, &context);
				if (!NT_SUCCESS(status))
					context.ImageQueryFailures++;
			}

			for (ULONG i = 0; i < context.PageTamperingList.count(); i++)
			{
				PPH_IMAGE_MAPPED_FAILURE_ENTRY entry = &context.PageTamperingList[i];

				/*for (ULONG j = 0; j < context.ImageAddressList.count(); j++)
				{
					PPH_IMAGE_MAPPED_BASE_ENTRY image = &context.ImageAddressList[j];
					if (!image->FileName.isEmpty() && image->ImageBase == entry->BaseAddress && image->SizeOfImage == entry->SizeOfImage )
					{
						break;
					}
				}*/

				QReadLocker Locker(&m_ModuleMutex);
				auto pWinModule = m_ModuleList[(quint64)entry->BaseAddress].objectCast<CWinModule>();
				if (pWinModule)
				{
					//quint64 Offset = PTR_SUB_OFFSET(entry->VirtualAddress, entry->BaseAddress)
					pWinModule->SetModifiedPage((quint64)entry->VirtualAddress);
				}
				else
					qDebug() << "Module not found for modified page";
			}
		}
	}
	else if(m->ModPagesLoaded)
	{
		m->ModPagesLoaded = false;
		foreach(const CModulePtr & pModule, m_ModuleList)
		{
			auto pWinModule = pModule.objectCast<CWinModule>();
			pWinModule->ClearModifiedPages();
		}
	}

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

	QMultiMap<quint64, quint64> Windows = ((CWindowsAPI*)GetSystem().data())->GetWindowByPID(m_ProcessId);

	//
	// What this process could enumerate for itself, so that an agent standing
	// in the same session does not report the same window a second time. Both
	// sources are right; only one of them should own the row, and the local one
	// wins because it can answer questions this one cannot.
	//
	QSet<quint64> Seen;

	for (QMultiMap<quint64, quint64>::iterator I = Windows.begin(); I != Windows.end(); I++)
	{
		quint64 ThreadID = I.key();
		QSharedPointer<CWinWnd> pWinWnd = OldWindows.take(I.value()).staticCast<CWinWnd>();

		bool bAdd = false;
		if (pWinWnd.isNull())
		{
			pWinWnd = QSharedPointer<CWinWnd>(new CWinWnd());
			pWinWnd->SetSystem(GetSystem());
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

		Seen.insert(I.value());
	}

	//
	// And the ones an agent saw for us.
	//
	// A window on another session's desktop cannot be asked anything from here -
	// its handle means nothing in this session - so it arrives as a record and
	// becomes a CAgentWnd, which answers from that record and sends its actions
	// back to the agent. The view cannot tell the two apart, which is the point.
	//
	foreach(const CWndAgents::SWindow& Window, CWndAgents::Instance()->GetWindows(ProcessId))
	{
		if (Seen.contains(Window.hWnd))
			continue;

		QSharedPointer<CAgentWnd> pAgentWnd = OldWindows.take(Window.hWnd).objectCast<CAgentWnd>();

		if (pAgentWnd.isNull())
		{
			pAgentWnd = QSharedPointer<CAgentWnd>(new CAgentWnd());
			pAgentWnd->SetSystem(GetSystem());
			pAgentWnd->Set(Window, ProcessName);

			Added.insert(Window.hWnd);

			QWriteLocker AgentLocker(&m_WindowMutex);
			m_WindowList.insert(Window.hWnd, pAgentWnd);
		}
		else if (pAgentWnd->Update(Window))
			Changed.insert(Window.hWnd);
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
	return m->IsWow64Process;
}

//
// The machine and the ARM64X flag, in place of the reading they used to make.
//
quint16 CWinProcess::GetArchitecture() const
{
	QReadLocker Locker(&m_Mutex);

	QSharedPointer<CWinMainModule> pModule = m_pModuleInfo.staticCast<CWinMainModule>();

	static_assert(CModuleInfo::eMachineI386  == IMAGE_FILE_MACHINE_I386,  "machine i386");
	static_assert(CModuleInfo::eMachineAmd64 == IMAGE_FILE_MACHINE_AMD64, "machine amd64");
	static_assert(CModuleInfo::eMachineArmNt == IMAGE_FILE_MACHINE_ARMNT, "machine armnt");
	static_assert(CModuleInfo::eMachineArm64 == IMAGE_FILE_MACHINE_ARM64, "machine arm64");

	return (!pModule || (m->Architecture != IMAGE_FILE_MACHINE_UNKNOWN)) ? m->Architecture : pModule->GetImageMachine();
}

//
// An ARM64X binary carries both an ARM64 and an x64 view of itself, which the
// machine field alone cannot say.
//
bool CWinProcess::IsArm64X() const
{
	QReadLocker Locker(&m_Mutex);

	QSharedPointer<CWinMainModule> pModule = m_pModuleInfo.staticCast<CWinMainModule>();
	if (!pModule || !pModule->GetImageCHPEVersion())
		return false;

	const quint16 Machine = (m->Architecture != IMAGE_FILE_MACHINE_UNKNOWN) ? m->Architecture : pModule->GetImageMachine();
	return Machine == IMAGE_FILE_MACHINE_AMD64 || Machine == IMAGE_FILE_MACHINE_ARM64;
}

quint64 CWinProcess::GetSessionID() const
{ 
	QReadLocker Locker(&m_Mutex); 
	return m->SessionId; 
}

quint16 CWinProcess::GetSubsystem() const
{
	static_assert(eSubsystemNative     == IMAGE_SUBSYSTEM_NATIVE,      "subsystem native");
	static_assert(eSubsystemWindowsGui == IMAGE_SUBSYSTEM_WINDOWS_GUI, "subsystem gui");
	static_assert(eSubsystemWindowsCui == IMAGE_SUBSYSTEM_WINDOWS_CUI, "subsystem cui");
	static_assert(eSubsystemOs2Cui     == IMAGE_SUBSYSTEM_OS2_CUI,     "subsystem os2");
	static_assert(eSubsystemPosixCui   == IMAGE_SUBSYSTEM_POSIX_CUI,   "subsystem posix");

	QReadLocker Locker(&m_Mutex); 
	QSharedPointer<CWinMainModule> pModule = m_pModuleInfo.staticCast<CWinMainModule>();
	return pModule ? pModule->GetImageSubsystem() : 0;
}

//
// The SID as text, which is the same string on every machine and in every
// language - unlike the name beside it. Taken from the token because that is
// where the identity actually lives; a process whose token could not be opened
// has no answer, and says so by returning nothing.
//
//
// Whether this is one of the invented processes - see GetUserName below.
//
static bool IsPseudoProcessId(quint64 ProcessId)
{
	return PH_IS_FAKE_PROCESS_ID((HANDLE)(LONG_PTR)ProcessId);
}
QString CWinProcess::GetUserKey() const
{
	QReadLocker Locker(&m_Mutex);
	if (m_pToken.isNull())
	{
		//
		// The same account the name above resolves to, so the two agree and the
		// row groups under it rather than under nothing. See GetUserName.
		//
		if (IsPseudoProcessId(m_ProcessId))
			return QString("S-1-5-18");
		return QString();
	}

	//
	// The *real* owner, which is what GetUserSid(true) means.
	//
	// A packaged app runs in an app container whose token user is the package's
	// own SID - one per package - while the name resolves to the person the
	// package runs for. Keying on the package SID puts every packaged app in a
	// branch of its own, all of them labelled with the same user name, which
	// looks like the grouping is broken. CWinToken resolves the name from
	// m_OwnerSid for exactly these tokens; this asks for the same one, so the
	// key and the name always describe the same account.
	//
	const QByteArray Sid = m_pToken->GetUserSid(true);
	if (Sid.isEmpty())
		return QString();

	QString Key;
	if (PPH_STRING pStr = PhSidToStringSid((PSID)Sid.constData()))
	{
		Key = QString::fromWCharArray(pStr->Buffer, pStr->Length / sizeof(wchar_t));
		PhDereferenceObject(pStr);
	}
	return Key;
}

//
// The account the two invented processes belong to.
//
// They have no token because they have no process - DPCs and Interrupts are
// structures this program fills in so that the CPU time the kernel spends
// servicing them has a row to be listed on. Left to the token they had no
// user at all, which put them in a nameless branch of their own when grouping
// by account and left the User column blank.
//
// The work they stand for is done in kernel mode on the system's behalf, which
// is the account the System process runs under and the one they are shown
// beside. Resolved from the well-known SID rather than written out, because
// the name is localised - NT-AUTORITÄT\SYSTEM on this machine - and because a
// SID is what the grouping key has to be anyway.
//

QString CWinProcess::GetUserName() const
{
	QReadLocker Locker(&m_Mutex); 
	if (m_pToken)
		return m_pToken->GetUserName();

	if (IsPseudoProcessId(m_ProcessId))
	{
		static QString SystemName;
		if (SystemName.isNull())
		{
			if (PPH_STRING pStr = PhGetSidFullName((PSID)&PhSeLocalSystemSid, TRUE, NULL))
			{
				SystemName = QString::fromWCharArray(pStr->Buffer, pStr->Length / sizeof(wchar_t));
				PhDereferenceObject(pStr);
			}
			else
				SystemName = QString("");	// asked once; do not ask again every second
		}
		return SystemName;
	}

	return QString();
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
	m_ProcessUId = SProcessUID(m_ProcessId, m->CreateTime.QuadPart);
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

	if (m->IsHandleVmRead)
	{
		// Note: this call is a bit slow updating it for all processes all the time is not nececery 
		//		if we only show this in the process general tab

		int pebOffset = PhpoCurrentDirectory;
#ifdef _WIN64
		// Tell the function to get the WOW64 current directory, because that's the one that
		// actually gets updated.
		if (m->IsWow64Process)
			pebOffset |= PhpoWow64;
#endif

		PPH_STRING curDir = NULL;
		PhGetProcessPebString(m->QueryHandle, (PH_PEB_OFFSET)pebOffset, &curDir);
		return CastPhString(curDir);
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

//
// What the process tree says about a process in its tooltip.
//
// Decoding a svchost group, a rundll target or a COM surrogate means reading
// the command line against the machine's own registry and image files, so it
// belongs with the collector rather than with the view that displays it. Only
// the values come back; the headings are written by whoever is reading.
//
QList<CProcessInfo::SToolTipSection> CWinProcess::GetToolTipSections() const
{
	QList<SToolTipSection> Sections;

	PH_KNOWN_PROCESS_TYPE KnownProcessType = (PH_KNOWN_PROCESS_TYPE)this->GetKnownProcessType();

	// Known command line information
	PH_KNOWN_PROCESS_COMMAND_LINE knownCommandLine;
	if (KnownProcessType != UnknownProcessType)
	{
		PH_AUTO_POOL autoPool;
		PhInitializeAutoPool(&autoPool);

		PPH_STRING commandLine = (PPH_STRING)PH_AUTO(CastQString(GetCommandLineStr()));

		if (PhaGetProcessKnownCommandLine(commandLine, KnownProcessType, &knownCommandLine))
		{
			switch (KnownProcessType & KnownProcessTypeMask)
			{
				case ServiceHostProcessType:
				{
					SToolTipSection Section(SToolTipSection::eServiceGroup);
					Section.Items.append(qMakePair(CastPhString(knownCommandLine.ServiceHost.GroupName, false), QString()));
					Sections.append(Section);
					break;
				}
				case RunDllAsAppProcessType:
				{
					PH_IMAGE_VERSION_INFO versionInfo;
					if (PhInitializeImageVersionInfo(&versionInfo, knownCommandLine.RunDllAsApp.FileName->Buffer))
					{
						SToolTipSection Section(SToolTipSection::eRunDllTarget);
						Section.Items.append(qMakePair(CastPhString(versionInfo.FileDescription, false), CastPhString(versionInfo.FileVersion, false)));
						Section.Items.append(qMakePair(CastPhString(versionInfo.CompanyName, false), QString()));
						Sections.append(Section);

						PhDeleteImageVersionInfo(&versionInfo);
					}
					break;
				}
				case ComSurrogateProcessType:
				{
					SToolTipSection Section(SToolTipSection::eComTarget);

					if (knownCommandLine.ComSurrogate.Name)
						Section.Items.append(qMakePair(CastPhString(knownCommandLine.ComSurrogate.Name, false), QString()));

					PPH_STRING guidString = PhFormatGuid(&knownCommandLine.ComSurrogate.Guid);
					if (guidString)
						Section.Items.append(qMakePair(CastPhString(guidString), QString()));

					Sections.append(Section);

					PH_IMAGE_VERSION_INFO versionInfo;
					if (knownCommandLine.ComSurrogate.FileName && PhInitializeImageVersionInfo(&versionInfo, knownCommandLine.ComSurrogate.FileName->Buffer))
					{
						SToolTipSection FileSection(SToolTipSection::eComTargetFile);
						FileSection.Items.append(qMakePair(CastPhString(versionInfo.FileDescription, false), CastPhString(versionInfo.FileVersion, false)));
						FileSection.Items.append(qMakePair(CastPhString(versionInfo.CompanyName, false), QString()));
						Sections.append(FileSection);

						PhDeleteImageVersionInfo(&versionInfo);
					}
					break;
				}
			}
		}

		PhDeleteAutoPool(&autoPool);
	}

	// Services
	QStringList ServiceList = this->GetServiceList();
	if (!ServiceList.isEmpty())
	{
		SToolTipSection Section(SToolTipSection::eServices);
		foreach(const QString& Service, ServiceList)
		{
			CServicePtr pService = GetSystem()->GetService(Service);
			Section.Items.append(qMakePair(Service, pService ? pService->GetDisplayName() : QString()));
		}
		Sections.append(Section);
	}

	// Tasks, Drivers
	switch (KnownProcessType & KnownProcessTypeMask)
	{
		case TaskHostProcessType:
		{
			QList<STask> Tasks = this->GetTasks();
			if (!Tasks.isEmpty())
			{
				SToolTipSection Section(SToolTipSection::eTasks);
				foreach(const STask& Task, Tasks)
					Section.Items.append(qMakePair(Task.Name, Task.Path));
				Sections.append(Section);
			}
		}
		break;
		case UmdfHostProcessType:
		{
			QList<SDriver> Drivers = this->GetUmdfDrivers();
			if (!Drivers.isEmpty())
			{
				SToolTipSection Section(SToolTipSection::eDrivers);
				foreach(const SDriver& Driver, Drivers)
					Section.Items.append(qMakePair(Driver.Name, Driver.Path));
				Sections.append(Section);
			}
		}
		break;
		case EdgeProcessType:
		{
			CTokenInfoPtr pToken = this->GetToken();
			if (!pToken)
				break;

			//
			// The AppContainer SIDs Edge uses for each of its parts. Compared
			// for equality - the earlier form of this test used the sign of
			// QString::compare, which is zero when the strings match, so every
			// branch was inverted and the first one caught nearly everything.
			//
			static const struct { const char* Sid; int Role; } EdgeSids[] =
			{
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194",
				  SToolTipSection::eEdgeManager },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-1206159417-1570029349-2913729690-1184509225",
				  SToolTipSection::eEdgeBrowserExtensions },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-3513710562-3729412521-1863153555-1462103995",
				  SToolTipSection::eEdgeUserInterfaceService },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-1821068571-1793888307-623627345-1529106238",
				  SToolTipSection::eEdgeChakraJitCompiler },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-3859068477-1314311106-1651661491-1685393560",
				  SToolTipSection::eEdgeFlashPlayer },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-4256926629-1688279915-2739229046-3928706915",
				  SToolTipSection::eEdgeBackgroundTabPool },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-2385269614-3243675-834220592-3047885450",
				  SToolTipSection::eEdgeBackgroundTabPool },
				{ "S-1-15-2-3624051433-2125758914-1423191267-1740899205-1073925389-3782572162-737981194-355265979-2879959831-980936148-1241729999",
				  SToolTipSection::eEdgeBackgroundTabPool },
			};

			const QString SidString = pToken->GetSidString();
			for (size_t i = 0; i < sizeof(EdgeSids) / sizeof(EdgeSids[0]); i++)
			{
				if (SidString.compare(QLatin1String(EdgeSids[i].Sid), Qt::CaseInsensitive) != 0)
					continue;

				SToolTipSection Section(SToolTipSection::eEdgeRole);
				Section.Role = EdgeSids[i].Role;
				Sections.append(Section);
				break;
			}
		}
		break;
		case WmiProviderHostType:
		{
			QList<SWmiProvider> Providers = this->QueryWmiProviders();
			if (!Providers.isEmpty())
			{
				SToolTipSection Section(SToolTipSection::eWmiProviders);
				foreach(const SWmiProvider& Provider, Providers)
					Section.Items.append(qMakePair(Provider.ProviderName, Provider.FileName));
				Sections.append(Section);
			}
		}
		break;
	}

	return Sections;
}

//
// Mandatory-policy bits, translated between the abstract flags and the
// SYSTEM_MANDATORY_LABEL_* ones the kernel uses.
//
static quint32 MandatoryFromNt(ACCESS_MASK Mask)
{
	quint32 Policy = 0;
	if (FlagOn(Mask, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP))	Policy |= CProcessInfo::eNoWriteUp;
	if (FlagOn(Mask, SYSTEM_MANDATORY_LABEL_NO_READ_UP))	Policy |= CProcessInfo::eNoReadUp;
	if (FlagOn(Mask, SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP))	Policy |= CProcessInfo::eNoExecuteUp;
	return Policy;
}

static ACCESS_MASK MandatoryToNt(quint32 Policy)
{
	ACCESS_MASK Mask = 0;
	if (Policy & CProcessInfo::eNoWriteUp)		SetFlag(Mask, SYSTEM_MANDATORY_LABEL_NO_WRITE_UP);
	if (Policy & CProcessInfo::eNoReadUp)		SetFlag(Mask, SYSTEM_MANDATORY_LABEL_NO_READ_UP);
	if (Policy & CProcessInfo::eNoExecuteUp)	SetFlag(Mask, SYSTEM_MANDATORY_LABEL_NO_EXECUTE_UP);
	return Mask;
}

quint64 CWinProcess::GetPebBaseAddress(bool bWow64) const
{
	//
	// Held by the main image rather than the process record, because that is
	// what carries the parsed headers it is read from.
	//
	QReadLocker Locker(&m_Mutex);
	QSharedPointer<CWinMainModule> pModule = m_pModuleInfo.staticCast<CWinMainModule>();
	return pModule.isNull() ? 0 : pModule->GetPebBaseAddress(bWow64);
}

quint32 CWinProcess::GetMandatoryPolicy() const
{
	ACCESS_MASK mandatoryPolicy = 0;
	if (!NT_SUCCESS(PhGetProcessMandatoryPolicy(GetQueryHandle(), &mandatoryPolicy)))
		return 0;
	return MandatoryFromNt(mandatoryPolicy);
}

STATUS CWinProcess::SetMandatoryPolicy(quint32 Policy)
{
	//
	// Needs its own handle: the cached query handle is not opened for
	// WRITE_OWNER, which changing the label requires.
	//
	HANDLE processHandle = NULL;
	NTSTATUS status = PhOpenProcess(&processHandle, READ_CONTROL | WRITE_OWNER, (HANDLE)m_ProcessId);
	if (!NT_SUCCESS(status))
		return CStatus::Native(status);

	status = PhSetProcessMandatoryPolicy(processHandle, MandatoryToNt(Policy));

	NtClose(processHandle);

	if (!NT_SUCCESS(status))
		return CStatus::Native(status);
	return OK;
}
bool CWinProcess::IsExecutionRequired() const
{
	return PhIsProcessExecutionRequired((HANDLE)m_ProcessId) ? true : false;
}

STATUS CWinProcess::SetExecutionRequired(bool bSet)
{
	NTSTATUS status = bSet
		? PhProcessExecutionRequiredEnable((HANDLE)m_ProcessId)
		: PhProcessExecutionRequiredDisable((HANDLE)m_ProcessId);

	if (!NT_SUCCESS(status))
		return ERR(TE_SetProcExecution, status);
	return OK;
}

//
// Working-set watch. Moved down from CWsWatchDialog, which opened the process
// and walked the kernel buffer itself.
//
// Deliberately stateless: the handle and buffer live for the duration of one
// call rather than for the life of the dialog. At the once-a-second poll this
// costs nothing, it cannot leak, and it is the shape a remote implementation
// wants anyway - one request, one answer.
//
STATUS CWinProcess::EnableWsWatch()
{
	HANDLE processHandle;
	NTSTATUS status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, (HANDLE)m_ProcessId);
	if (!NT_SUCCESS(status))
		return ERR(TE_OpenProc2, status);

	status = NtSetInformationProcess(processHandle, ProcessWorkingSetWatchEx, NULL, 0);
	NtClose(processHandle);

	if (!NT_SUCCESS(status))
		return ERR(TE_EnableWorkingSet, status);
	return OK;
}

STATUS CWinProcess::GetWsWatchFaults(QList<quint64>& Faults, bool& bEnabled)
{
	bEnabled = false;

	HANDLE processHandle;
	NTSTATUS status = PhOpenProcess(&processHandle, PROCESS_QUERY_INFORMATION, (HANDLE)m_ProcessId);
	if (!NT_SUCCESS(status))
		return ERR(TE_OpenProc2, status);

	ULONG BufferSize = 0x2000;
	PVOID Buffer = PhAllocate(BufferSize);
	ULONG returnLength = 0;

	status = NtQueryInformationProcess(processHandle, ProcessWorkingSetWatchEx, Buffer, BufferSize, &returnLength);

	if (status == STATUS_BUFFER_TOO_SMALL || status == STATUS_INFO_LENGTH_MISMATCH)
	{
		PhFree(Buffer);
		BufferSize = returnLength;
		Buffer = PhAllocate(BufferSize);
		status = NtQueryInformationProcess(processHandle, ProcessWorkingSetWatchEx, Buffer, BufferSize, &returnLength);
	}

	NtClose(processHandle);

	// the watch has not been turned on for this process
	if (status == STATUS_UNSUCCESSFUL)
	{
		PhFree(Buffer);
		return OK;
	}

	bEnabled = true;

	// running, but nothing faulted since the last poll
	if (status == STATUS_NO_MORE_ENTRIES)
	{
		PhFree(Buffer);
		return OK;
	}

	if (!NT_SUCCESS(status))
	{
		PhFree(Buffer);
		return ERR(TE_ReadWorkingSet, status);
	}

	//
	// One record per fault, so the same address appearing twice means it
	// faulted twice - the caller does the tallying.
	//
	PPROCESS_WS_WATCH_INFORMATION_EX wsWatchInfo = (PPROCESS_WS_WATCH_INFORMATION_EX)Buffer;
	while (wsWatchInfo->BasicInfo.FaultingPc)
	{
		Faults.append((quint64)wsWatchInfo->BasicInfo.FaultingPc);
		wsWatchInfo++;
	}

	PhFree(Buffer);
	return OK;
}
CTokenInfoPtr CWinProcess::GetOriginalToken() const
{
	return CTokenInfoPtr(CWinToken::OriginalToken(GetSystem(), m_ProcessId));
}

bool CWinProcess::ValidateParent(CProcessInfo* pParent) const
{ 
	QReadLocker Locker(&m_Mutex); 

	if (!pParent || pParent->GetProcessId() == m_ProcessId) // for cases where the parent PID = PID (e.g. System Idle Process)
        return false;

	//
	// The candidate has to be a process of *this* kind before anything below can
	// ask it anything.
	//
	// It used to be cast twice without checking, on the reasonable assumption
	// that a Windows process's parent is a Windows process. That stopped being
	// true when a viewer could hold several machines at once: a remote process is
	// a CRemoteProcess, the cast yields null, and the next line dereferences it.
	// CProcessModel keeps them apart before it gets here, which is why this has
	// never fired - but the guard belongs with the cast, not with the one caller
	// that happens to remember.
	//
	// False rather than an error: parentage between two machines is not a
	// question with an answer.
	//
	CWinProcess* pWinParent = qobject_cast<CWinProcess*>(pParent);
	if (!pWinParent)
		return false;

	if (m_ProcessId == (quint64)SYSTEM_PROCESS_ID && pParent->GetProcessId() == (quint64)SYSTEM_IDLE_PROCESS_ID)
		return true;

	if (WindowsVersion >= WINDOWS_10_RS3 && !PhIsExecutingInWow64())
	{
		// We make sure that the process item we found is actually the parent process - its sequence number
		// must not be higher than the supplied sequence.
		quint64 uParentSN = pWinParent->GetProcessSequenceNumber();
		if (uParentSN != -1 && m->ProcessSequenceNumber != -1)
		{
			if (uParentSN <= m->ProcessSequenceNumber)
				return true;
			return false;
		}
	}
	
	// We make sure that the process item we found is actually the parent process - its start time
	// must not be larger than the supplied time.
	quint64 uParentCreationTime = pWinParent->GetRawCreateTime();
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

// OS context
quint32 CWinProcess::GetOsContextVersion() const
{
	static_assert(eOsContextNone  == WINDOWS_ANCIENT, "os context none");
	static_assert(eOsContextXp    == WINDOWS_XP,      "os context xp");
	static_assert(eOsContextVista == WINDOWS_VISTA,   "os context vista");
	static_assert(eOsContext7     == WINDOWS_7,       "os context 7");
	static_assert(eOsContext8     == WINDOWS_8,       "os context 8");
	static_assert(eOsContext81    == WINDOWS_8_1,     "os context 8.1");
	static_assert(eOsContext10    == WINDOWS_10,      "os context 10");

	QReadLocker Locker(&m_Mutex);
	return m->OsContextVersion;
}

quint32 CWinProcess::GetMitigationFlags() const
{
	quint32 Flags = 0;

	if (m_pModuleInfo)
	{
		QReadLocker Locker(&m_pModuleInfo->m_Mutex);
		if (m_pModuleInfo.objectCast<CWinModule>()->m_ImageDllCharacteristics & IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE)
			Flags |= eMitigationAslr;
	}

	QReadLocker Locker(&m_Mutex);

	if (m->DepStatus & PH_PROCESS_DEP_ENABLED)
	{
		Flags |= eMitigationDep;
		if (m->DepStatus & PH_PROCESS_DEP_PERMANENT)
			Flags |= eMitigationDepPermanent;
	}

	if (m->IsControlFlowGuardEnabled)	Flags |= eMitigationCfg;
	if (m->IsXfgEnabled)				Flags |= eMitigationXfg;
	if (m->IsXfgAuditEnabled)			Flags |= eMitigationXfgAudit;
	if (m->IsCetEnabled)				Flags |= eMitigationCet;
	if (m->IsCetStrictModeEnabled)		Flags |= eMitigationCetStrict;

	return Flags;
}

QMap<QString, CWinProcess::SEnvVar>	CWinProcess::GetEnvVariables() const
{
	QMap<QString, SEnvVar> EnvVars;

    PVOID SystemDefaultEnvironment = NULL;
    PVOID UserDefaultEnvironment = NULL;

	PVOID environment;
    ULONG environmentLength;
	ULONG enumerationKey;
    PH_ENVIRONMENT_VARIABLE variable;

	if (m->IsHandleVmRead)
	{

		HANDLE tokenHandle;

		if (CreateEnvironmentBlock != NULL)
		{
			CreateEnvironmentBlock(&SystemDefaultEnvironment, NULL, FALSE);

			if (NT_SUCCESS(PhOpenProcessToken(m->QueryHandle, TOKEN_QUERY | TOKEN_DUPLICATE, &tokenHandle)))
			{
				CreateEnvironmentBlock(&UserDefaultEnvironment, tokenHandle, FALSE);
				NtClose(tokenHandle);
			}
		}

		if (NT_SUCCESS(PhGetProcessEnvironment(m->QueryHandle, m->IsWow64Process, &environment, &environmentLength)))
		{
			enumerationKey = 0;
			while (NT_SUCCESS(PhEnumProcessEnvironmentVariables(environment, environmentLength, &enumerationKey, &variable)))
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
		return ERR(TE_ConfirmEditEnvSuspended, ERROR_CONFIRM);
	}

	NTSTATUS status;
	HANDLE processHandle;
	LARGE_INTEGER timeout;

	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_VM_READ | PROCESS_VM_WRITE, m->UniqueProcessId)))
	{
		timeout.QuadPart = -(LONGLONG)UInt32x32To64(10, PH_TIMEOUT_SEC);

		std::wstring NameStr = Name.toStdWString();
		PH_STRINGREF NameRef;
		NameRef.Buffer = (PWCH)NameStr.c_str();
		NameRef.Length = NameStr.length() * sizeof(wchar_t);

		std::wstring ValueStr = Value.toStdWString();
		PH_STRINGREF ValueRef;
		ValueRef.Buffer = (PWCH)ValueStr.c_str();
		ValueRef.Length = ValueStr.length() * sizeof(wchar_t);
		
		status = PhSetEnvironmentVariableRemote(processHandle, &NameRef, ValueRef.Length == 0 ? NULL : &ValueRef, &timeout);

		NtClose(processHandle);
	}

	if (!NT_SUCCESS(status))
	{
		return ERR(TE_SetEnvironmentVariable, status);
	}
	if (status == STATUS_TIMEOUT)
	{
		return ERR(TE_DeleteEnvironmentVariable, WAIT_TIMEOUT);
	}
	return OK;
}

quint32 CWinProcess::GetStatusFlags() const
{
	quint32 Flags = 0;

	if (IsHiddenProcess())
		Flags |= eStatusHidden;
	else if (m_RemoveTimeStamp != 0)
		Flags |= eStatusTerminated;

	QReadLocker Locker(&m_Mutex);

	if (m_IsCritical)				Flags |= eStatusCritical;
	if (m->IsSandBoxed)				Flags |= eStatusSandboxed;
	if (m->IsBeingDebugged)			Flags |= eStatusDebugged;
	if (m->IsSuspended)				Flags |= eStatusSuspended;
	if (m->IsProtectedHandle)		Flags |= eStatusHandleFiltered;
	if (m->IsElevated)				Flags |= eStatusElevated;
	if (m->IsSubsystemProcess)		Flags |= eStatusPico;			// a subsystem other than Win32, such as WSL
	if (m->IsCrossSessionProcess)	Flags |= eStatusCrossSession;
	if (m->IsFrozenProcess)			Flags |= eStatusFrozen;			// a suspended UWP process
	if (m->IsBackgroundProcess)		Flags |= eStatusBackground;		// a UWP process performing a background task
	if (m->IsPackagedProcess)		Flags |= eStatusPackaged;
	if (m->IsSecureProcess)			Flags |= eStatusSecure;			// isolated user mode
	if (m->IsImmersive)				Flags |= eStatusImmersive;
	if (m->IsDotNet)				Flags |= eStatusDotNet;
	if (m->IsPacked)				Flags |= eStatusPacked;
	if (m->IsWow64Process)			Flags |= eStatusWow64;
	if (m->IsInSignificantJob)		Flags |= eStatusInSignificantJob;
	if (m->IsReflectedProcess)		Flags |= eStatusReflected;
	if (m->IsSystemProcess)			Flags |= eStatusSystemProcess;
	if (m->IsSecureSystem)			Flags |= eStatusSecureSystem;

	Locker.unlock();

	if (IsInJob())					Flags |= eStatusInJob;
	if (IsServiceProcess())			Flags |= eStatusService;
	if (IsSystemProcess())			Flags |= eStatusSystem;
	if (IsUserProcess())			Flags |= eStatusOwned;

	return Flags;
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
        m->IsWow64Process ? &aeDebugWow64KeyName : &aeDebugKeyName,
#else
        &aeDebugKeyName,
#endif
        0);

    if (NT_SUCCESS(status))
    {
        if (debugger = PhQueryRegistryStringZ(keyHandle, L"Debugger"))
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
		return ERR(TE_LocateDebugger);
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
        return ERR(TE_CreateDebuggerProc, status);
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
        return ERR(TE_ProcDebugged);
    }

    if (!NT_SUCCESS(status))
    {
		return ERR(TE_DetachDebugger, status);
    }

    return OK;
}

bool MyEqualSid(const SID* Sid1, const CWinTokenPtr& token)
{
	if (!token)
		return false;
	QByteArray sid = token->GetUserSid(true);
	if (sid.isEmpty() || RtlLengthSid((PSID)Sid1) != sid.size())
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
	return MyEqualSid(&PhSeLocalSystemSid, m_pToken);
}

bool CWinProcess::IsServiceProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return !m_ServiceList.isEmpty() || (MyEqualSid(&PhSeServiceSid, m_pToken) || MyEqualSid(&PhSeLocalServiceSid, m_pToken) || MyEqualSid(&PhSeNetworkServiceSid, m_pToken));
}

bool CWinProcess::IsUserProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return MyEqualSid((const SID*)PhGetOwnTokenAttributes().TokenSid, m_pToken);
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

bool CWinProcess::IsPackagedProcess() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->IsPackagedProcess;
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

quint8 CWinProcess::GetProtection() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->Protection.Level;
}

//
// The kernel packs the protection level and the signer behind it into one
// byte. UCHAR_MAX means it did not report one at all, which is not the same as
// reporting that a process is unprotected.
//
quint8 CWinProcess::GetProtectionType() const
{
	QReadLocker Locker(&m_Mutex);

	static_assert(eProtectionNone  == PsProtectedTypeNone,           "protection none");
	static_assert(eProtectionLight == PsProtectedTypeProtectedLight, "protection light");
	static_assert(eProtectionFull  == PsProtectedTypeProtected,      "protection full");

	if (m->Protection.Level == UCHAR_MAX)
		return eProtectionUnknown;

	//
	// Before 8.1 there was a single flag and no type to go with it.
	//
	if (WindowsVersion < WINDOWS_8_1)
		return m->IsProtectedProcess ? eProtectionLegacy : eProtectionNone;

	return m->Protection.Type;
}

quint8 CWinProcess::GetProtectionSigner() const
{
	QReadLocker Locker(&m_Mutex);

	static_assert(eSignerAuthenticode == PsProtectedSignerAuthenticode, "signer authenticode");
	static_assert(eSignerCodeGen      == PsProtectedSignerCodeGen,      "signer codegen");
	static_assert(eSignerAntimalware  == PsProtectedSignerAntimalware,  "signer antimalware");
	static_assert(eSignerLsa          == PsProtectedSignerLsa,          "signer lsa");
	static_assert(eSignerWindows      == PsProtectedSignerWindows,      "signer windows");
	static_assert(eSignerWinTcb       == PsProtectedSignerWinTcb,       "signer wintcb");
	static_assert(eSignerWinSystem    == PsProtectedSignerWinSystem,    "signer winsystem");
	static_assert(eSignerStoreApp     == PsProtectedSignerApp,          "signer storeapp");

	if (m->Protection.Level == UCHAR_MAX || WindowsVersion < WINDOWS_8_1)
		return eSignerNone;

	return m->Protection.Signer;
}

//
// Which combination of observations adds up to which level is the driver's
// own business and moves between its versions, so the level is worked out here
// rather than left for a viewer to recompute from the flags.
//
qint8 CWinProcess::GetKphLevel() const
{
	QReadLocker Locker(&m_Mutex);

	if (!(m->KphProcessState & KPH_PROCESS_VERIFIED_PROCESS))
		return eKphNotVerified;

	if ((m->KphProcessState & KPH_PROCESS_STATE_MAXIMUM) == KPH_PROCESS_STATE_MAXIMUM)	return eKphMaximum;
	if ((m->KphProcessState & KPH_PROCESS_STATE_HIGH)    == KPH_PROCESS_STATE_HIGH)		return eKphHigh;
	if ((m->KphProcessState & KPH_PROCESS_STATE_MEDIUM)  == KPH_PROCESS_STATE_MEDIUM)	return eKphMedium;
	if ((m->KphProcessState & KPH_PROCESS_STATE_LOW)     == KPH_PROCESS_STATE_LOW)		return eKphLow;
	if ((m->KphProcessState & KPH_PROCESS_STATE_MINIMUM) == KPH_PROCESS_STATE_MINIMUM)	return eKphMinimum;

	return eKphNone;
}

quint32 CWinProcess::GetKphState() const
{
	QReadLocker Locker(&m_Mutex);

	static_assert(eKphSecurelyCreated            == KPH_PROCESS_SECURELY_CREATED,            "kph securely created");
	static_assert(eKphVerifiedProcess            == KPH_PROCESS_VERIFIED_PROCESS,            "kph verified");
	static_assert(eKphProtectedProcess           == KPH_PROCESS_PROTECTED_PROCESS,           "kph protected");
	static_assert(eKphNoUntrustedImages          == KPH_PROCESS_NO_UNTRUSTED_IMAGES,         "kph no untrusted images");
	static_assert(eKphHasFileObject              == KPH_PROCESS_HAS_FILE_OBJECT,             "kph has file object");
	static_assert(eKphHasSectionObjectPointers   == KPH_PROCESS_HAS_SECTION_OBJECT_POINTERS, "kph has section pointers");
	static_assert(eKphNoUserWritableReferences   == KPH_PROCESS_NO_USER_WRITABLE_REFERENCES, "kph no writable refs");
	static_assert(eKphNoFileTransaction          == KPH_PROCESS_NO_FILE_TRANSACTION,         "kph no file transaction");
	static_assert(eKphNotBeingDebugged           == KPH_PROCESS_NOT_BEING_DEBUGGED,          "kph not debugged");

	return m->KphProcessState;
}

/*STATUS CWinProcess::SetProtectionFlag(quint8 Flag, bool bForce) // todo: xxxx si
{
	if ((((CWindowsAPI*)GetSystem().data())->GetDriverFeatures() & (1 << 31)) == 0)
		return ERR(TE_DriverFeatureUnsupported, STATUS_NOT_SUPPORTED);

	if (!KphIsVerified())
		return ERR(TE_ClientNotVerified, STATUS_ACCESS_DENIED);

	if (!bForce)
		return ERR(TE_ConfirmChangeProtection, ERROR_CONFIRM);

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
		return ERR(TE_ClearProcProtection, status);
	return OK;
}*/

QList<CProcessInfo::SMitigationDetail> CWinProcess::GetMitigationDetails() const
{
	QList<SMitigationDetail> List;
	if(!m->IsHandleFull)
		return List;

	//
	// The policies the platform knows how to describe, in its own words.
	//
	PH_PROCESS_MITIGATION_POLICY_ALL_INFORMATION information;
	if (NT_SUCCESS(PhGetProcessMitigationPolicy(m->QueryHandle, &information)))
	{
		for (int policy = 0; policy < MaxProcessMitigationPolicy; policy++)
		{
			PPH_STRING shortDescription;
			PPH_STRING longDescription;
			if (information.Pointers[policy] && PhDescribeProcessMitigationPolicy((PROCESS_MITIGATION_POLICY)policy, information.Pointers[policy], &shortDescription, &longDescription))
			{
				SMitigationDetail Detail;
				Detail.Name = CastPhString(shortDescription);
				Detail.Description = CastPhString(longDescription);
				List.append(Detail);
			}
		}
	}

	//
	// A few more that are not in that table, recognised here and worded by the
	// viewer.
	//
	PS_SYSTEM_DLL_INIT_BLOCK sysDllInitBlock = {0};
	PPS_SYSTEM_DLL_INIT_BLOCK systemDllInitBlock = &sysDllInitBlock; // this requiers PROCESS_VM_READ
	if (NT_SUCCESS(PhGetProcessSystemDllInitBlock(m->QueryHandle, &sysDllInitBlock)))
	{
		if (systemDllInitBlock && RTL_CONTAINS_FIELD(systemDllInitBlock, systemDllInitBlock->Size, MitigationOptionsMap))
		{
			static const struct { ULONG64 Flag; int Extra; } Extras[] =
			{
				{ PROCESS_CREATION_MITIGATION_POLICY2_LOADER_INTEGRITY_CONTINUITY_ALWAYS_ON,		SMitigationDetail::eExtraLoaderIntegrity },
				{ PROCESS_CREATION_MITIGATION_POLICY2_MODULE_TAMPERING_PROTECTION_ALWAYS_ON,		SMitigationDetail::eExtraModuleTampering },
				{ PROCESS_CREATION_MITIGATION_POLICY2_RESTRICT_INDIRECT_BRANCH_PREDICTION_ALWAYS_ON,	SMitigationDetail::eExtraIndirectBranchPrediction },
				{ PROCESS_CREATION_MITIGATION_POLICY2_ALLOW_DOWNGRADE_DYNAMIC_CODE_POLICY_ALWAYS_ON,	SMitigationDetail::eExtraDynamicCodeDowngrade },
				{ PROCESS_CREATION_MITIGATION_POLICY2_SPECULATIVE_STORE_BYPASS_DISABLE_ALWAYS_ON,	SMitigationDetail::eExtraSpeculativeStoreBypass },
			};

			for (size_t i = 0; i < sizeof(Extras) / sizeof(Extras[0]); i++)
			{
				if (systemDllInitBlock->MitigationOptionsMap.Map[0] & Extras[i].Flag)
				{
					SMitigationDetail Detail;
					Detail.Extra = Extras[i].Extra;
					List.append(Detail);
				}
			}
		}
	}

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

quint64 CWinProcess::GetShareableCommitSize() const
{
	QReadLocker Locker(&m_Mutex);
	return m->SharedCommitCharge;
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

STATUS CWinProcess::SetPriorityBoost(bool Value)
{
	NTSTATUS status;
	HANDLE processHandle;

	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, m->UniqueProcessId)))
	{
		status = PhSetProcessPriorityBoost(processHandle, Value);
		NtClose(processHandle);
	}

	if (NT_SUCCESS(status))
		m_PriorityBoost = Value;
	else
	{
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, "SetPriorityBoost", Value))
				return OK;
		}

		return ERR(TE_SetProcPriorityBoost, status);
	}
	return OK;
}

STATUS CWinProcess::SetPowerThrottled(bool Value)
{
	NTSTATUS status;
	HANDLE processHandle;
	POWER_THROTTLING_PROCESS_STATE powerThrottlingState;

	status = PhOpenProcess( &processHandle, PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_INFORMATION, m->UniqueProcessId);

	if (NT_SUCCESS(status))
	{
		status = PhGetProcessPowerThrottlingState(processHandle, &powerThrottlingState);

		if (NT_SUCCESS(status))
		{
			if (!(
				FlagOn(powerThrottlingState.ControlMask, POWER_THROTTLING_PROCESS_EXECUTION_SPEED) &&
				FlagOn(powerThrottlingState.StateMask, POWER_THROTTLING_PROCESS_EXECUTION_SPEED)
				))
			{

				// Taskmgr sets the process priority to idle before enabling 'Eco mode'. (dmex)
				PhSetProcessPriorityClass(processHandle, PROCESS_PRIORITY_CLASS_IDLE);

				//
				// Turn PROCESS_EXECUTION_SPEED throttling on.
				//
				status = PhSetProcessPowerThrottlingState(
					processHandle,
					POWER_THROTTLING_PROCESS_EXECUTION_SPEED,
					POWER_THROTTLING_PROCESS_EXECUTION_SPEED
				);
			}
			else
			{
				// Taskmgr does not properly restore the original priority after it has exited
				// and you later decide to disable 'Eco mode', so we'll restore normal priority
				// which isn't quite correct but still way better than what taskmgr does. (dmex)
				PhSetProcessPriorityClass(processHandle, PROCESS_PRIORITY_CLASS_NORMAL);

				//
				// Let system manage all power throttling.
				//
				status = PhSetProcessPowerThrottlingState(processHandle, 0, 0);
			}
		}

		NtClose(processHandle);
	}

	if (!NT_SUCCESS(status))
		return ERR(TE_SetProcEfficiency, status);
	return OK;
}

STATUS CWinProcess::SetPriority(qint32 Value)
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
            status = PhSetProcessPriorityClass(processHandle, (UCHAR)Value);
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

		return ERR(TE_SetProcPriority, status);
    }
	return OK;
}

STATUS CWinProcess::SetPagePriority(qint32 Value)
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

		return ERR(TE_SetPagePriority, status);
    }
	return OK;
}

STATUS CWinProcess::SetIOPriority(qint32 Value)
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

		return ERR(TE_SetIoPriority, status);
    }
	return OK;
}

bool CWinProcess::IsPowerThrottled() const
{
	QReadLocker Locker(&m_Mutex);
	return m->IsPowerThrottling;
}

quint16 CWinProcess::GetCodePage() const
{
	QReadLocker Locker(&m_Mutex);
	return m->CodePage;
}

quint16 CWinProcess::GetTlsBitmapCount() const
{
	static_assert(eTlsMinimumAvailable == TLS_MINIMUM_AVAILABLE, "tls minimum available");
	static_assert(eTlsExpansionSlots   == TLS_EXPANSION_SLOTS,   "tls expansion slots");

	QReadLocker Locker(&m_Mutex);
	return m->TlsBitmapCount;
}

quint32 CWinProcess::GetErrorMode() const
{
	static_assert(eErrModeFailCriticalErrors     == SEM_FAILCRITICALERRORS,     "sem fail critical");
	static_assert(eErrModeNoGpFaultErrorBox      == SEM_NOGPFAULTERRORBOX,      "sem no gp fault box");
	static_assert(eErrModeNoAlignmentFaultExcept == SEM_NOALIGNMENTFAULTEXCEPT, "sem no alignment fault");
	static_assert(eErrModeNoOpenFileErrorBox     == SEM_NOOPENFILEERRORBOX,     "sem no openfile box");

	QReadLocker Locker(&m_Mutex);
	return m->ErrorMode;
}

quint32 CWinProcess::GetReferenceCount()
{
	QReadLocker Locker(&m_Mutex);
	return m->ReferenceCount;
}

quint32 CWinProcess::GetAccessMask()
{
	QReadLocker Locker(&m_Mutex);
	return m->AccessMask;
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

		return ERR(TE_SetCpuAffinity, status);
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
				return ERR(TE_ConfirmTerminateCriticalProc, ERROR_CONFIRM);
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

		return ERR(TE_TerminateProc, status);
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

		return ERR(TE_SuspendProc, status);
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

		return ERR(TE_ResumeProc, status);
    }
	return OK;
}

bool CWinProcess::IsFrozen() const
{
	QReadLocker Locker(&m_Mutex);
	return m->FreezeHandle != NULL;
}

STATUS CWinProcess::Freeze()
{
	if (ReadPointerAcquire(&m->FreezeHandle))
		return ERR(TE_ProcAlreadyFrozen, STATUS_UNSUCCESSFUL);

	HANDLE freezeHandle;
	NTSTATUS status = PhFreezeProcess(&freezeHandle, m->UniqueProcessId);
	if (!NT_SUCCESS(status))
		return ERR(TE_FreezeProc, status);

	InterlockedExchangePointer(&m->FreezeHandle, freezeHandle);

	return OK;
}

STATUS CWinProcess::UnFreeze()
{
	if (!ReadPointerAcquire(&m->FreezeHandle))
		return ERR(TE_ProcFrozen, STATUS_UNSUCCESSFUL);
	
	NTSTATUS status = PhThawProcess(m->FreezeHandle, m->UniqueProcessId);
	if (!NT_SUCCESS(status))
		return ERR(TE_UnFreezeProc, status);

	if (HANDLE freezeHandle = InterlockedExchangePointer(&m->FreezeHandle, NULL))
		NtClose(freezeHandle);

	return OK;
}

bool CWinProcess::IsReflectedProcess() const
{
	QReadLocker Locker(&m_Mutex);
	return m->IsReflectedProcess;
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

					return ERR(TE_ConfirmCriticalProcShutdown, ERROR_CONFIRM);
				}

				status = PhSetProcessBreakOnTermination(processHandle, TRUE);
			} 
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
        return ERR(TE_ChangeProcCritical, status);
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
		return ERR(TE_ReduceWorkingSet, status);
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
	CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)GetSystem().data())->GetSandboxieAPI();
	return pSandboxieAPI ? pSandboxieAPI->GetSandBoxName(GetProcessId()) : QString();
}

NTSTATUS CWinProcess__LoadModule(HANDLE ProcessHandle, const QString& Path)
{
	LARGE_INTEGER Timeout;
		Timeout.QuadPart = -(LONGLONG)UInt32x32To64(5, PH_TIMEOUT_SEC);
	std::wstring FileName = Path.toStdWString();

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
		if (!NT_SUCCESS(status = PhLoadMappedImage((wchar_t*)FileName.c_str(), NULL, &mappedImage)))
			goto FreeExit;

		isModule32 = mappedImage.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC;
		PhUnloadMappedImage(&mappedImage);
	}

	if (!isModule32)
	{
#endif
		static PH_STRINGREF kernel32_dll = PH_STRINGREF_INIT(L"kernel32.dll");
		threadStart = PhGetDllProcedureAddress(&kernel32_dll, "LoadLibraryW", 0);
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

			status = PhGetProcedureAddressRemoteZ(ProcessHandle, kernel32FileName->Buffer, "LoadLibraryW", &loadLibraryW32, NULL);
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
		return ERR(TE_LoadDllInto, status);
	}
    return OK;
}

NTSTATUS NTAPI CWinProcess_OpenProcessPermissions(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenProcess(Handle, DesiredAccess, (HANDLE)Context);
}

CSecurityEditablePtr CWinProcess::GetSecurityObject() const
{
	return CSecurityEditablePtr(new CWinSecurityObject(
		m_ProcessName, "Process",
		(CWinSecurityObject::POpenObject)CWinProcess_OpenProcessPermissions, GetProcessId(), GetProcessId()));
}

CJobInfoPtr CWinProcess::GetJob() const
{
	QReadLocker Locker(&m_Mutex); 

	if (!m->QueryHandle)
		return CWinJobPtr();
	return CWinJobPtr(CWinJob::JobFromProcess(GetSystem(), m->QueryHandle));
}

QMap<quint64, CMemoryPtr> CWinProcess::GetMemoryMap() const
{
	ULONG Flags = PH_QUERY_MEMORY_REGION_TYPE | PH_QUERY_MEMORY_WS_COUNTERS;
	QMap<quint64, CMemoryPtr> MemoryMap;
	PhQueryMemoryItemList((HANDLE)GetProcessId(), Flags, MemoryMap);
	for (QMap<quint64, CMemoryPtr>::iterator I = MemoryMap.begin(); I != MemoryMap.end(); ++I)
		I.value()->SetSystem(GetSystem()); // phlib builds these, so stamp them on the way out
	return MemoryMap;
}

QMap<quint64, CGdiPtr> CWinProcess::GetGdiList() const
{
	QMap<quint64, CGdiPtr> List;

	//
	// The table is a system-wide section the kernel maps into a process the
	// first time that process touches win32k - not when gdi32 is merely loaded.
	// A viewer has always touched it long before anyone opens this tab, so this
	// used to be a null check that never fired. A daemon answering the same
	// question for a viewer on another machine may never have drawn anything,
	// and came back with an empty list that looked like a process holding no
	// graphics objects at all.
	//
	// One call is enough, and the cheapest one that goes through win32k. Done
	// here rather than at startup so that a server nobody asks stays a console
	// program.
	//
	PGDI_SHARED_MEMORY gdiShared = (PGDI_SHARED_MEMORY)NtCurrentPeb()->GdiSharedHandleTable;
	if (!gdiShared)
	{
		if (HDC hScreen = GetDC(NULL))
			ReleaseDC(NULL, hScreen);
		gdiShared = (PGDI_SHARED_MEMORY)NtCurrentPeb()->GdiSharedHandleTable;
	}
	if (!gdiShared)
		return List;

	const QString ProcessName = GetName();
	const USHORT ProcessId = (USHORT)GetProcessId();

	for (ULONG i = 0; i < GDI_MAX_HANDLE_COUNT; i++)
	{
		PGDI_HANDLE_ENTRY handle = &gdiShared->Handles[i];
		if (handle->Owner.ProcessId != ProcessId)
			continue;

		QSharedPointer<CWinGDI> pWinGDI = QSharedPointer<CWinGDI>(new CWinGDI());
		pWinGDI->SetSystem(GetSystem());
		pWinGDI->InitData(i, handle, ProcessName);
		List.insert(GDI_MAKE_HANDLE(i, handle->Unique), pWinGDI);
	}
	return List;
}

QMap<quint64, CHeapPtr> CWinProcess::GetHeapList() const
{
	QVariantList Heaps;
#ifdef _WIN64
	if (m->IsWow64Process)
	{
		QString SocketName = CTaskService::RunWorker(false, true);

		if (!SocketName.isEmpty())
		{
			QVariantMap Parameters;
			Parameters["ProcessId"] = GetProcessId();

			QVariantMap Request;
			Request["Command"] = "GetProcessHeaps";
			Request["Parameters"] = Parameters;

			QVariant Response = CTaskService::SendCommand(SocketName, Request);
			Heaps = Response.toList();
		}
	}
	else
	{
#endif
		qint32 status = 0;
		Heaps = GetProcessHeaps(m_ProcessId, &status);
		if (!NT_SUCCESS(status))
			qWarning("CWinProcess::GetHeapList: pid %llu: 0x%08X", m_ProcessId, (quint32)status);
#ifdef _WIN64
	}
#endif

	QMap<quint64, CHeapPtr> HeapList;

	foreach(const QVariant & vHeap, Heaps)
	{
		QVariantMap Heap = vHeap.toMap();

		CWinHeapPtr pHeapInfo = CWinHeapPtr(new CWinHeap());
		pHeapInfo->SetSystem(GetSystem());
		HeapList.insert(Heap["BaseAddress"].toULongLong(), pHeapInfo);

		pHeapInfo->m_Flags = Heap["Flags"].toUInt();
		pHeapInfo->m_Signature = Heap["Signature"].toUInt();
		pHeapInfo->m_HeapFrontEndType = Heap["HeapFrontEndType"].toUInt();
		pHeapInfo->m_NumberOfEntries = Heap["NumberOfEntries"].toUInt();
		pHeapInfo->m_BaseAddress = Heap["BaseAddress"].toULongLong();
		pHeapInfo->m_BytesAllocated = Heap["BytesAllocated"].toULongLong();
		pHeapInfo->m_BytesCommitted = Heap["BytesCommitted"].toULongLong();
	}

	return HeapList;
}

QVariantList GetProcessHeaps(quint64 ProcessId, qint32* pStatus)
{
	QVariantList List;
	if (pStatus)
		*pStatus = STATUS_SUCCESS;

	QMap<quint64, CHeapPtr> HeapList;

	NTSTATUS status = STATUS_UNSUCCESSFUL;
	HANDLE processHandle = NULL;
	HANDLE powerRequestHandle = NULL;
	PROCESS_REFLECTION_INFORMATION reflectionInfo = { 0 };
	HANDLE clientProcessId = (HANDLE)ProcessId;

	PVOID DebugBuffer = NULL;
	PVOID ProcessHeap = NULL;

	if (WindowsVersion >= WINDOWS_8 && WindowsVersion <= WINDOWS_8_1)
	{
		// Windows 8 requires ALL_ACCESS for PLM execution requests. (dmex)
		status = PhOpenProcess(
			&processHandle,
			PROCESS_ALL_ACCESS,
			clientProcessId
		);
	}
	else
	{
		// Windows 10 and above require SET_LIMITED for PLM execution requests. (dmex)
		status = PhOpenProcess(
			&processHandle,
			PROCESS_QUERY_LIMITED_INFORMATION | PROCESS_SET_LIMITED_INFORMATION | // PLM
			PROCESS_CREATE_THREAD | PROCESS_VM_OPERATION | PROCESS_DUP_HANDLE, // Reflection
			clientProcessId
		);
	}

	if (processHandle)
	{
		PhCreateExecutionRequiredRequest(processHandle, &powerRequestHandle);

		// todo add setting
		/*if (PhGetIntegerSetting(L"EnableHeapReflection")) 
		{
			// NOTE: RtlQueryProcessDebugInformation injects a thread into the process causing deadlocks and other issues in rare cases.
			// We mitigate these problems by reflecting the process and querying heap information from the clone. (dmex)

			status = PhCreateProcessReflection(
				&reflectionInfo,
				processHandle
			);

			if (NT_SUCCESS(status))
			{
				clientProcessId = reflectionInfo.ReflectionClientId.UniqueProcess;
			}
		}*/
	}

	PPH_PROCESS_DEBUG_HEAP_INFORMATION heapDebugInfo;

	status = PhQueryProcessHeapInformation(
		clientProcessId,
		&heapDebugInfo
	);

	if (!NT_SUCCESS(status))
	{
		//
		// Reported rather than swallowed. An empty heap list that cannot say why
		// it is empty looks exactly like a process with no heaps, and the two
		// want very different reactions from whoever is looking.
		//
		if (pStatus)
			*pStatus = (qint32)status;
		goto CleanupExit;
	}

	DebugBuffer = heapDebugInfo;
	ProcessHeap = heapDebugInfo->DefaultHeap;

	for (ULONG i = 0; i < heapDebugInfo->NumberOfHeaps; i++)
	{
		PPH_PROCESS_DEBUG_HEAP_ENTRY entry = &heapDebugInfo->Heaps[i];

		QVariantMap Heap;
		Heap["Flags"] = (quint32)entry->Flags;
		Heap["Signature"] = (quint32)entry->Signature;
		Heap["HeapFrontEndType"] = (quint8)entry->HeapFrontEndType;
		Heap["NumberOfEntries"] = (quint32)entry->NumberOfEntries;
		Heap["BaseAddress"] = (quint64)entry->BaseAddress;
		Heap["BytesAllocated"] = (quint64)entry->BytesAllocated;
		Heap["BytesCommitted"] = (quint64)entry->BytesCommitted;
		List.append(Heap);
	}

CleanupExit:
	PhFreeProcessReflection(&reflectionInfo);

	if (processHandle)
		NtClose(processHandle);

	if (powerRequestHandle)
		PhDestroyExecutionRequiredRequest(powerRequestHandle);

	if (DebugBuffer)
		PhFree(DebugBuffer);

	return List;
}

STATUS CWinProcess::FlushHeaps()
{
	LARGE_INTEGER timeout;
	NTSTATUS status;
	HANDLE processHandle;

	status = PhOpenProcess(
		&processHandle,
		PROCESS_CREATE_THREAD | PROCESS_QUERY_LIMITED_INFORMATION |
		PROCESS_SET_LIMITED_INFORMATION | PROCESS_VM_READ,
		m->UniqueProcessId
	);

	if (NT_SUCCESS(status))
	{
		status = PhFlushProcessHeapsRemote(processHandle, PhTimeoutFromMilliseconds(&timeout, 4000));
		NtClose(processHandle);
	}

	if (!NT_SUCCESS(status))
	{
		return ERR(TE_FlushHeaps, status);
	}

	return OK;
}

QList<CWndPtr> CWinProcess::GetWindows() const
{
	bool IsImmersive = IsImmersiveProcess();

	QList<quint64> Windows;
	QList<quint64> ImmersiveWindows;

	QMultiMap<quint64, quint64> WindowList = ((CWindowsAPI*)GetSystem().data())->GetWindowByPID(GetProcessId());
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

	//
	// And the ones only an agent can see.
	//
	// The tests above are Win32 calls on a handle from this session; for a
	// window in another one they answer nothing rather than no. So the agent's
	// own report is used instead - it made the same three judgements standing
	// where the window is, which is the only place they can be made.
	//
	foreach(const CWndAgents::SWindow& Window, CWndAgents::Instance()->GetWindows(GetProcessId()))
	{
		if (!Window.Visible || Window.Title.isEmpty() || Windows.contains(Window.hWnd))
			continue;

		//
		// Top level only, judged the same way: a window whose parent is also in
		// the list is a child of something already offered.
		//
		if (Window.Parent && CWndAgents::Instance()->GetWindows(GetProcessId()).size() > 1)
		{
			bool bHasVisibleParent = false;
			foreach(const CWndAgents::SWindow& Other, CWndAgents::Instance()->GetWindows(GetProcessId()))
			{
				if (Other.hWnd == Window.Parent && Other.Visible) { bHasVisibleParent = true; break; }
			}
			if (bHasVisibleParent)
				continue;
		}

		Windows.append(Window.hWnd);
	}

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
	if(QThread::currentThread() != GetSystem()->thread())
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
								Task.Name = action ? QString::fromWCharArray(action) : MakePlaceholder(TE_NAME_UNKNOWN_ACTION);
								Task.Path = path ? QString::fromWCharArray(path) : MakePlaceholder(TE_NAME_UNKNOWN_PATH);
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

    PVOID environment;
    ULONG environmentLength;
    ULONG enumerationKey;
    PH_ENVIRONMENT_VARIABLE variable;

    if (!m->IsHandleVmRead)
        return Drivers;

    if (NT_SUCCESS(PhGetProcessEnvironment(m->QueryHandle, IsWoW64(), &environment, &environmentLength)))
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

                        if (deviceDesc = PhQueryRegistryStringZ(driverKeyHandle, L"DeviceDesc"))
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

                        hardwareId = PhQueryRegistryStringZ(driverKeyHandle, L"HardwareID");

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

    if (FAILED(status = IWbemServices_ExecQuery(wbemServices, (PWSTR)L"WQL", queryString->Buffer, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &wbemEnumerator)))
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
                    PhMoveReference(&fileName, expandedString);
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
	if (QThread::currentThread() != GetSystem()->thread())
		result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    HRESULT status;
    PPH_STRING queryString = NULL;
    IWbemLocator* wbemLocator = NULL;
    IWbemServices* wbemServices = NULL;
    IEnumWbemClassObject* wbemEnumerator = NULL;
    IWbemClassObject *wbemClassObject;

    if (FAILED(status = CoCreateInstance(*(_GUID*)&CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER, *(_GUID*)&IID_IWbemLocator, (PVOID*)&wbemLocator)))
        goto CleanupExit;

    if (FAILED(status = IWbemLocator_ConnectServer(wbemLocator, (PWSTR)L"root\\CIMV2", NULL, NULL, NULL, 0, 0, NULL, &wbemServices)))
        goto CleanupExit;

    queryString = PhConcatStrings2(L"SELECT Namespace,Provider,User FROM Msft_Providers WHERE HostProcessIdentifier = ", (wchar_t*)QString::number(GetProcessId()).toStdWString().c_str());

    if (FAILED(status = IWbemServices_ExecQuery(wbemServices, (PWSTR)L"WQL", queryString->Buffer, WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &wbemEnumerator)))
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

quint64 CWinProcess::GetLXSSProcessId() const
{
	QReadLocker Locker(&m_Mutex);
	return m->LxssProcessId;
}
CAssemblyEnumerator* CWinProcess::GetAssemblyEnumerator(QObject* parent) const
{
	return new CAssemblyEnum(GetProcessId(), parent);
}
