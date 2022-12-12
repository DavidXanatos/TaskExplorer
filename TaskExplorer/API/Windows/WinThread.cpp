/*
 * Task Explorer -
 *   qt wrapper and support functions based on thrdprv.c
 *
 * Copyright (C) 2010-2016 wj32
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "WinThread.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"
#include "../../SVC/TaskService.h"

struct SWinThread
{
	SWinThread()
	{
		UniqueThreadId = NULL;
		ThreadHandle = NULL;
		//LastExtendedUpdate = GetCurTick();
		StartAddressResolveLevel = PhsrlAddress;
		StartAddressResolvePending = false;
		memset(&idealProcessorNumber, 0, sizeof(PROCESSOR_NUMBER));

		CreateTime.QuadPart = 0;
	}

	HANDLE UniqueThreadId;
	HANDLE ThreadHandle;
	//quint64 LastExtendedUpdate;
	PH_SYMBOL_RESOLVE_LEVEL StartAddressResolveLevel;
	bool StartAddressResolvePending;

	PROCESSOR_NUMBER idealProcessorNumber;

	LARGE_INTEGER CreateTime;
};

CWinThread::CWinThread(QObject *parent) 
	: CThreadInfo(parent) 
{
	m_StartAddress = 0;

	m_BasePriorityIncrement = 0;

	m_IsGuiThread = false;
	m_IsCritical = false;
	m_HasToken = false;
	m_HasToken2 = false;
	m_IsSandboxed = false;


	m = new SWinThread();
}

CWinThread::~CWinThread()
{
	UnInit(); // just in case 

	delete m;
}

bool CWinThread::InitStaticData(void* ProcessHandle, struct _SYSTEM_THREAD_INFORMATION* thread)
{
	QWriteLocker Locker(&m_Mutex);

	m_ThreadId = (quint64)thread->ClientId.UniqueThread;
	m_ProcessId = (quint64)thread->ClientId.UniqueProcess;

	m->CreateTime = thread->CreateTime;
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);

    // Try to open a handle to the thread.
	m->UniqueThreadId = thread->ClientId.UniqueThread;
    if (!NT_SUCCESS(PhOpenThread(&m->ThreadHandle, THREAD_QUERY_INFORMATION, thread->ClientId.UniqueThread)))
    {
        PhOpenThread(&m->ThreadHandle, THREAD_QUERY_LIMITED_INFORMATION, thread->ClientId.UniqueThread);
    }

	PVOID startAddress = NULL;
    if (m->ThreadHandle)
    {
        PhGetThreadStartAddress(m->ThreadHandle, &startAddress);

		PVOID serviceTag;
		if (NT_SUCCESS(PhGetThreadServiceTag(m->ThreadHandle, ProcessHandle, &serviceTag)))
		{
			PPH_STRING serviceName = PhGetServiceNameFromTag(thread->ClientId.UniqueProcess, serviceTag);

			m_ServiceName = CastPhString(serviceName);
		}
    }

    if (!startAddress)
        startAddress = thread->StartAddress;

    m_StartAddress = (ULONG64)startAddress;
	m_StartAddressString = FormatAddress(m_StartAddress); // this will be replaced with the resolved symbol

	return true;
}

bool CWinThread::UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread, quint64 sysTotalTime, quint64 sysTotalCycleTime)
{
	QWriteLocker Locker(&m_Mutex);

	BOOLEAN modified = FALSE;

	m_KernelTime = thread->KernelTime.QuadPart;
	m_UserTime = thread->UserTime.QuadPart;

	m_Priority = thread->Priority;
	m_BasePriority = thread->BasePriority;
	
	if (m_State != (KTHREAD_STATE)thread->ThreadState)
	{
		m_State = (KTHREAD_STATE)thread->ThreadState;
		modified = TRUE;
	}

	if (m_WaitReason != thread->WaitReason)
	{
		m_WaitReason = thread->WaitReason;
		modified = TRUE;
	}

	bool HandleReOpened = false;
	if (m->ThreadHandle == NULL)
	{
		// Try to re open a handle to the thread.
		if (!NT_SUCCESS(PhOpenThread(&m->ThreadHandle, THREAD_QUERY_INFORMATION, m->UniqueThreadId)))
		{
			PhOpenThread(&m->ThreadHandle, THREAD_QUERY_LIMITED_INFORMATION, m->UniqueThreadId);
		}

		HandleReOpened = true;
	}


	QWriteLocker StatsLocker(&m_StatsMutex);

    // Update the context switch count.
    {
        ULONG oldDelta = m_CpuStats.ContextSwitchesDelta.Delta;
        m_CpuStats.ContextSwitchesDelta.Update(thread->ContextSwitches);
        if (m_CpuStats.ContextSwitchesDelta.Delta != oldDelta)
            modified = TRUE;
    }

	// Update the cycle count.
	if (m->ThreadHandle)
	{
		ULONG64 oldDelta = m_CpuStats.CycleDelta.Delta;

		ULONG64 cycles = 0;
		if (m_ProcessId != (quint64)SYSTEM_IDLE_PROCESS_ID)
			PhGetThreadCycleTime(m->ThreadHandle, &cycles);
		else
			cycles = qobject_cast<CWindowsAPI*>(theAPI)->GetCpuIdleCycleTime(m_ThreadId);

		m_CpuStats.CycleDelta.Update(cycles);

		// without this we will get a cpu usage spike when starting to display this thread
		if (HandleReOpened)
			m_CpuStats.CycleDelta.Delta = 0;

		if (m_CpuStats.CycleDelta.Delta != oldDelta)
			modified = TRUE;
	}

    // Update the CPU time deltas.
    m_CpuStats.CpuKernelDelta.Update(m_KernelTime);
    m_CpuStats.CpuUserDelta.Update(m_UserTime);

	// If the cycle time isn't available, we'll fall back to using the CPU time.
	m_CpuStats.UpdateStats(sysTotalTime, (m_ProcessId == (quint64)SYSTEM_IDLE_PROCESS_ID || m->ThreadHandle) ? sysTotalCycleTime : 0);

	StatsLocker.unlock();

	// Update the GUI thread status.
    {
        GUITHREADINFO info = { sizeof(GUITHREADINFO) };
        bool oldIsGuiThread = m_IsGuiThread;

        m_IsGuiThread = !!GetGUIThreadInfo(m_ThreadId, &info);

        if (m_IsGuiThread != oldIsGuiThread)
            modified = TRUE;
    }

	if (m->StartAddressResolveLevel != PhsrlFunction && !m->StartAddressResolvePending)
	{
		m->StartAddressResolvePending = true;
		qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetSymbolFromAddress(m_ProcessId, m_StartAddress, this, SLOT(OnSymbolFromAddress(quint64, quint64, int, const QString&, const QString&, const QString&)));
	}

	// for all other values we need a handle
	if (!m->ThreadHandle)
		return modified;

	// Thread debug name
	if (WindowsVersion >= WINDOWS_10_RS1)
    {
        PPH_STRING threadName;
        if (NT_SUCCESS(PhGetThreadName(m->ThreadHandle, &threadName)))
        {
            m_ThreadName = CastPhString(threadName);
        }
    }

    // Update the base priority increment.
    {
		LONG basePriorityIncrement = THREAD_PRIORITY_ERROR_RETURN;
		quint64 threadAffinityMask = 0;
		THREAD_BASIC_INFORMATION basicInfo;
        if (m->ThreadHandle && NT_SUCCESS(PhGetThreadBasicInformation(m->ThreadHandle,&basicInfo)))
        {
            basePriorityIncrement = basicInfo.BasePriority;
			threadAffinityMask = basicInfo.AffinityMask;
        }

        if (m_BasePriorityIncrement != basePriorityIncrement || m_AffinityMask != threadAffinityMask)
        {
			m_BasePriorityIncrement = basePriorityIncrement;
			m_AffinityMask = threadAffinityMask;

            modified = TRUE;
        }
    }
	
    // Update the page priority increment.
    {
		ULONG pagePriorityInteger = MEMORY_PRIORITY_NORMAL + 1;
		PhGetThreadPagePriority(m->ThreadHandle, &pagePriorityInteger);

        if (m_PagePriority != pagePriorityInteger)
        {
			m_PagePriority = pagePriorityInteger;

            modified = TRUE;
        }
    }

    // Update the I/O priority increment.
    {
		IO_PRIORITY_HINT ioPriorityInteger = MaxIoPriorityTypes;
		PhGetThreadIoPriority(m->ThreadHandle, &ioPriorityInteger);

        if (m_IOPriority != ioPriorityInteger)
        {
			m_IOPriority = ioPriorityInteger;

            modified = TRUE;
        }
    }

	// update critical flag
	{
		BOOLEAN breakOnTermination;
		if (NT_SUCCESS(PhGetThreadBreakOnTermination(m->ThreadHandle, &breakOnTermination)))
		{
			if ((bool)breakOnTermination != m_IsCritical)
			{
				m_IsCritical = breakOnTermination;
				modified = TRUE;
			}
		}
	}

	// update HasToken
	{
		BOOLEAN HasToken = FALSE;
		HANDLE tokenHandle;
        if (NT_SUCCESS(NtOpenThreadToken(m->ThreadHandle, TOKEN_QUERY, TRUE, &tokenHandle)))
        {
			HasToken = TRUE;
            NtClose(tokenHandle);
        }

		if ((bool)HasToken != m_HasToken)
		{
			m_HasToken = HasToken;
			modified = TRUE;
		}
	}


	if(m_IsSandboxed){
		BOOLEAN HasToken2 = FALSE;

		CSandboxieAPI* pSandboxieAPI = ((CWindowsAPI*)theAPI)->GetSandboxieAPI();
		HasToken2 = pSandboxieAPI && pSandboxieAPI->TestOriginalToken(m_ProcessId, m_ThreadId);
		if ((bool)HasToken2 != m_HasToken2)
		{
			m_HasToken2 = HasToken2;
			modified = TRUE;
		}
	}

	// ideal processor
	{
		PROCESSOR_NUMBER idealProcessorNumber;
		if (NT_SUCCESS(PhGetThreadIdealProcessor(m->ThreadHandle, &idealProcessorNumber)))
		{
			if (memcmp(&idealProcessorNumber, &m->idealProcessorNumber, sizeof(PROCESSOR_NUMBER)) != 0)
			{
				memcpy(&m->idealProcessorNumber, &idealProcessorNumber, sizeof(PROCESSOR_NUMBER));
				modified = TRUE;
			}
		}
	}

	return modified;
}

void CWinThread::OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName)
{
	m->StartAddressResolvePending = false;
	m->StartAddressResolveLevel = (PH_SYMBOL_RESOLVE_LEVEL)ResolveLevel;
	m_StartAddressString = StartAddressString;
	m_StartAddressFileName = FileName;
}

void CWinThread::CloseHandle()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->ThreadHandle != NULL) {
		NtClose(m->ThreadHandle);
		m->ThreadHandle = NULL;
	}
}

void CWinThread::UnInit()
{
	CloseHandle();

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_CpuStats.ContextSwitchesDelta.Delta = 0;
	
	m_CpuStats.CpuUsage = 0;
	m_CpuStats.CpuKernelUsage = 0;
	m_CpuStats.CpuUserUsage = 0;
}

quint64 CWinThread::TraceStack()
{
	return qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetStackTrace(m_ProcessId, m_ThreadId, this, SIGNAL(StackTraced(const CStackTracePtr&)));
}

QString CWinThread::GetName() const
{
	CProcessPtr pProcess = GetProcess().staticCast<CProcessInfo>();
	if (pProcess)
		return pProcess->GetName();
	return tr("Unknown process");
}

QString CWinThread::GetStateString() const
{
	QReadLocker Locker(&m_Mutex);

	QString State;

    if (m_State != Waiting)
    {
		if (m_State < MaximumThreadState)
			State = QString::fromWCharArray(PhKThreadStateNames[m_State]);
        else
            State = tr("Unknown");
    }
    else
    {
		if (m_WaitReason < MaximumWaitReason)
			State = tr("Wait:") + QString::fromWCharArray(PhKWaitReasonNames[m_WaitReason]);
        else
            State = tr("Waiting");
    }

    if (m->ThreadHandle)
    {
		ULONG suspendCount;
        if (m_WaitReason == Suspended && NT_SUCCESS(PhGetThreadSuspendCount(m->ThreadHandle, &suspendCount)))
			State.append(tr(" (%1)").arg(suspendCount));
    }

	return State;
}

quint64 CWinThread::GetRawCreateTime() const
{
	QReadLocker Locker(&m_Mutex); 
	return m->CreateTime.QuadPart; 
}

QString CWinThread::GetStartAddressString() const
{
	QReadLocker Locker(&m_Mutex);
	return m_StartAddressString;
}

QString CWinThread::GetPriorityString() const
{
	QReadLocker Locker(&m_Mutex);

    switch (m_BasePriorityIncrement)
    {
    case THREAD_BASE_PRIORITY_LOWRT + 1:
    case THREAD_BASE_PRIORITY_LOWRT:
        return tr("Time critical");
    case THREAD_PRIORITY_HIGHEST:
        return tr("Highest");
    case THREAD_PRIORITY_ABOVE_NORMAL:
        return tr("Above normal");
    case THREAD_PRIORITY_NORMAL:
        return tr("Normal");
    case THREAD_PRIORITY_BELOW_NORMAL:
        return tr("Below normal");
    case THREAD_PRIORITY_LOWEST:
        return tr("Lowest");
    case THREAD_BASE_PRIORITY_IDLE:
    case THREAD_BASE_PRIORITY_IDLE - 1:
        return tr("Idle");
    case THREAD_PRIORITY_ERROR_RETURN:
        return tr("");
    default:
		return QString::number(m_BasePriorityIncrement);
    }
}

QString CWinThread::GetBasePriorityString() const	
{ 
	// The process priority class and thread priority level are combined to form the base priority of each thread.
	// https://docs.microsoft.com/en-us/windows/win32/procthread/scheduling-priorities
	return QString::number(GetBasePriority());
}
QString CWinThread::GetPagePriorityString() const	
{ 
	return CWinProcess::GetPagePriorityString(GetPagePriority()); 
}
QString CWinThread::GetIOPriorityString() const		
{ 
	return CWinProcess::GetIOPriorityString(GetIOPriority()); 
}

STATUS CWinThread::SetPriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

    // Special saturation values
    if (Value == THREAD_PRIORITY_TIME_CRITICAL)
        Value = THREAD_BASE_PRIORITY_LOWRT + 1;
    else if (Value == THREAD_PRIORITY_IDLE)
        Value = THREAD_BASE_PRIORITY_IDLE - 1;

	NTSTATUS status;
    HANDLE threadHandle;
	if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, (HANDLE)m_ThreadId)))
    {
        status = PhSetThreadBasePriority(threadHandle, Value);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "SetPriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set Thread priority"), status);
    }
	return OK;
}

STATUS CWinThread::SetPagePriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE threadHandle;
	if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, (HANDLE)m_ThreadId)))
    {
        status = PhSetThreadPagePriority(threadHandle, Value);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "SetPagePriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set Page priority"), status);
    }
	return OK;
}

STATUS CWinThread::SetIOPriority(long Value)
{
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
    HANDLE threadHandle;
	if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, (HANDLE)m_ThreadId)))
    {
        status = PhSetThreadIoPriority(threadHandle, (IO_PRIORITY_HINT)Value);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "SetIOPriority", Value))
				return OK;
		}

		return ERR(tr("Failed to set I/O priority"), status);
    }
	return OK;
}

STATUS CWinThread::SetAffinityMask(quint64 Value)
{	
	QWriteLocker Locker(&m_Mutex); 

	NTSTATUS status;
	HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_LIMITED_INFORMATION, (HANDLE)m_ThreadId)))
    {
        status = PhSetThreadAffinityMask(threadHandle, Value);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "SetAffinityMask", Value))
				return OK;
		}

		return ERR(tr("Failed to set CPU affinity"), status);
    }
	return OK;
}

STATUS CWinThread::Terminate(bool bForce)
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_QUERY_INFORMATION | THREAD_TERMINATE, (HANDLE)m_ThreadId)))
    {
#ifndef SAFE_MODE // in safe mode always check and fail
		if (!bForce)
#endif
		{
			BOOLEAN breakOnTermination;
			PhGetThreadBreakOnTermination(threadHandle, &breakOnTermination);

			if (breakOnTermination /*m_IsCritical*/)
			{
				NtClose(threadHandle);
				return ERR(tr("You are about to terminate one or more critical threads. This will shut down the operating system immediately."), ERROR_CONFIRM);
			}
		}

        status = NtTerminateThread(threadHandle, STATUS_SUCCESS);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "Terminate"))
				return OK;
		}

		return ERR(tr("Failed to terminate thread"), status);
    }
	return OK;
}

bool CWinThread::IsSuspended() const
{
	QReadLocker Locker(&m_Mutex);
	return m_State == Waiting && m_WaitReason == Suspended;
}

STATUS CWinThread::Suspend()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, (HANDLE)m_ThreadId)))
    {
        status = NtSuspendThread(threadHandle, NULL);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "Suspend"))
				return OK;
		}

		return ERR(tr("Failed to suspend thread"), status);
    }
	return OK;
}

STATUS CWinThread::Resume()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, (HANDLE)m_ThreadId)))
    {
        status = NtResumeThread(threadHandle, NULL);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		if(CTaskService::CheckStatus(status))
		{
			if (CTaskService::TaskAction(m_ProcessId, m_ThreadId, "Resume"))
				return OK;
		}

		return ERR(tr("Failed to resume thread"), status);
    }
	return OK;
}

QString CWinThread::GetIdealProcessor() const
{
	return tr("%1:%2").arg(m->idealProcessorNumber.Group).arg(m->idealProcessorNumber.Number);
}

QString CWinThread::GetTypeString() const
{
	QReadLocker Locker(&m_Mutex);
	if (m_IsMainThread)
		return tr("Main");
	return m_IsGuiThread ? tr("GUI") : tr("Normal");
}

STATUS CWinThread::SetCriticalThread(bool bSet, bool bForce)
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;
    HANDLE threadHandle;
    BOOLEAN breakOnTermination;

    status = PhOpenThread(&threadHandle, THREAD_QUERY_INFORMATION | THREAD_SET_INFORMATION, (HANDLE)m_ThreadId);
    if (NT_SUCCESS(status))
    {
        status = PhGetThreadBreakOnTermination(threadHandle, &breakOnTermination);

        if (NT_SUCCESS(status))
        {
			if (bSet == false && breakOnTermination)
            {
				status = PhSetThreadBreakOnTermination(threadHandle, FALSE);
            }
			else if (bSet == true && !breakOnTermination)
			{
#ifndef SAFE_MODE // in safe mode always check and fail
				if (!bForce)
#endif
				{
					NtClose(threadHandle);

					return ERR(tr("If the process ends, the operating system will shut down immediately."), ERROR_CONFIRM);
				}

				status = PhSetThreadBreakOnTermination(threadHandle, TRUE);
			} 
        }

        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
        return ERR(tr("Unable to change the thread critical status."), status);
    }

	m_IsCritical = bSet;
	return OK;
}

STATUS CWinThread::CancelIO()
{
	QWriteLocker Locker(&m_Mutex);

	NTSTATUS status;

	HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_TERMINATE, (HANDLE)m_ThreadId)))
    {
		IO_STATUS_BLOCK isb;
        status = NtCancelSynchronousIoFile(threadHandle, NULL, &isb);
    }

    if (status == STATUS_NOT_FOUND)
    {
        return ERR(tr("There is no synchronous I/O to cancel."), status);
    }
    if (!NT_SUCCESS(status))
    {
		return ERR(tr("Unable to cancel synchronous I/O"), status);
    }
	return OK;
}

extern "C" {
#include "ProcessHacker/clrsup.h"
}

QString GetClrThreadAppDomainImpl(quint64 ProcessId, quint64 ThreadId)
{
	QString AppDomain;
	PCLR_PROCESS_SUPPORT support = CreateClrProcessSupport((HANDLE)ProcessId);
	if (support)
	{
		IXCLRDataTask *task;
		if (SUCCEEDED(IXCLRDataProcess_GetTaskByOSThreadID(support->DataProcess, ThreadId, &task)))
		{
			IXCLRDataAppDomain *appDomain;
			if (SUCCEEDED(IXCLRDataTask_GetCurrentAppDomain(task, &appDomain)))
			{
				AppDomain = CastPhString(GetNameXClrDataAppDomain(appDomain));

				IXCLRDataAppDomain_Release(appDomain);
			}

			IXCLRDataTask_Release(task);
		}

		FreeClrProcessSupport(support);
	}
	return AppDomain;
}

QString SvcGetClrThreadAppDomain(const QVariantMap& Parameters)
{
	return GetClrThreadAppDomainImpl(Parameters["ProcessId"].toULongLong(), Parameters["ThreadId"].toULongLong());
}

QString GetClrThreadAppDomain(quint64 ProcessId, quint64 ThreadId)
{
	/*BOOLEAN isDotNet;
	if (!NT_SUCCESS(PhGetProcessIsDotNet((HANDLE)ProcessId, &isDotNet)) || !isDotNet)
		return "";*/

#ifdef WIN64
	BOOLEAN IsWow64 = FALSE;

    HANDLE processHandle;
    if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)ProcessId)))
    {
        PhGetProcessIsWow64(processHandle, &IsWow64);
        NtClose(processHandle);
    }

    if (IsWow64)
    {
		QString SocketName = CTaskService::RunWorker(false, true);

		if (!SocketName.isEmpty())
		{
			QVariantMap Parameters;
			Parameters["ProcessId"] = ProcessId;
			Parameters["ThreadId"] = ThreadId;

			QVariantMap Request;
			Request["Command"] = "GetClrThreadAppDomain";
			Request["Parameters"] = Parameters;

			QVariant Response = CTaskService::SendCommand(SocketName, Request);

			if (Response.type() == QVariant::String)
				return Response.toString();
		}
		return "";
    }
#endif
	return GetClrThreadAppDomainImpl(ProcessId, ThreadId);
}

QString CWinThread::GetAppDomain() const
{
	QReadLocker Locker(&m_Mutex);
	if (!m_AppDomain.isNull())
		return m_AppDomain;
	Locker.unlock();

	((CWinThread*)this)->SetAppDomain("");
	QTimer::singleShot(0, this, SLOT(UpdateAppDomain()));
	return "";
}

void CWinThread::UpdateAppDomain()
{
	BOOLEAN isDotNet;
	if (!NT_SUCCESS(PhGetProcessIsDotNet((HANDLE)m_ProcessId, &isDotNet)) || !isDotNet)
		return; // nothign to do here

	QFutureWatcher<QString>* pWatcher = new QFutureWatcher<QString>(this);
	QObject::connect(pWatcher, SIGNAL(resultReadyAt(int)), this, SLOT(OnAppDomain(int)));
	QObject::connect(pWatcher, SIGNAL(finished()), pWatcher, SLOT(deleteLater()));
	quint64 ThreadId = m_ThreadId;
	quint64 ProcessId= m_ProcessId;
	pWatcher->setFuture(QtConcurrent::run([ProcessId, ThreadId]() {
		return GetClrThreadAppDomain(ProcessId, ThreadId);
	}));
}

void CWinThread::OnAppDomain(int Index)
{
	QFutureWatcher<QString>* pWatcher = (QFutureWatcher<QString>*)sender();
	if (pWatcher)
		SetAppDomain(pWatcher->resultAt(Index));
}

static NTSTATUS NTAPI CWinThread_OpenThreadPermissions(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenThread(Handle, DesiredAccess, (HANDLE)Context);
}

void CWinThread::OpenPermissions()
{
    PhEditSecurity(NULL, (wchar_t*)GetStartAddressString().toStdWString().c_str(), L"Thread", CWinThread_OpenThreadPermissions, NULL, (HANDLE)GetThreadId());
}