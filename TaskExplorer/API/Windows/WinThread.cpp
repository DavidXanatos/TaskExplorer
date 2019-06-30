/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2010-2016 wj32
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
#include "../../GUI/TaskExplorer.h"
#include "WinThread.h"
#include "ProcessHacker.h"
#include "WindowsAPI.h"
#include "SymbolProvider.h"

struct SWinThread
{
	SWinThread() :
		ThreadHandle(NULL),
		StartAddressResolveLevel(PhsrlAddress)
	{
		CreateTime.QuadPart = 0;
	}

	HANDLE ThreadHandle;
	PH_SYMBOL_RESOLVE_LEVEL StartAddressResolveLevel;

	LARGE_INTEGER CreateTime;
};

CWinThread::CWinThread(QObject *parent) 
	: CThreadInfo(parent) 
{
	m_StartAddress = 0;

	m_BasePriorityIncrement = 0;

	m_IsGuiThread = false;
	m_IsCritical = false;

	m = new SWinThread();
}

CWinThread::~CWinThread()
{
	UnInit(); // just in case 

	delete m;
}

bool CWinThread::InitStaticData(void* pProcessHandle, struct _SYSTEM_THREAD_INFORMATION* thread)
{
	QWriteLocker Locker(&m_Mutex);

	HANDLE ProcessHandle = *(HANDLE*)pProcessHandle;

	m_ThreadId = (quint64)thread->ClientId.UniqueThread;
	m_ProcessId = (quint64)thread->ClientId.UniqueProcess;

	m->CreateTime = thread->CreateTime;
	m_CreateTimeStamp = FILETIME2ms(m->CreateTime.QuadPart);

    // Try to open a handle to the thread.
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

	if (WindowsVersion >= WINDOWS_10_RS1)
    {
        PPH_STRING threadName;
        if (NT_SUCCESS(PhGetThreadName(m->ThreadHandle, &threadName)))
        {
            m_ThreadName = CastPhString(threadName);
        }
    }

	return true;
}

bool CWinThread::UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread, quint64 sysTotalTime, quint64 sysTotalCycleTime)
{
	QWriteLocker Locker(&m_Mutex);

	BOOLEAN modified = FALSE;

	//if (threadItem->JustResolved)
	//	modified = TRUE;
	//threadItem->JustResolved = FALSE;

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

    // Update the GUI thread status.
    {
        GUITHREADINFO info = { sizeof(GUITHREADINFO) };
        bool oldIsGuiThread = m_IsGuiThread;

        m_IsGuiThread = !!GetGUIThreadInfo(m_ThreadId, &info);

        if (m_IsGuiThread != oldIsGuiThread)
            modified = TRUE;
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

	// ideal processor
	{
		PROCESSOR_NUMBER idealProcessorNumber;
		if (NT_SUCCESS(PhGetThreadIdealProcessor(m->ThreadHandle, &idealProcessorNumber)))
		{
			QString IdealCPU = tr("%1:%2").arg(idealProcessorNumber.Group).arg(idealProcessorNumber.Number);

			if (IdealCPU != m_IdealProcessor)
			{
				m_IdealProcessor = IdealCPU;
				modified = TRUE;
			}
		}
	}


	QWriteLocker StatsLocker(&m_StatsMutex);

    // Update the context switch count.
    {
        ULONG oldDelta;

        oldDelta = m_CpuStats.ContextSwitchesDelta.Delta;
        m_CpuStats.ContextSwitchesDelta.Update(thread->ContextSwitches);

        if (m_CpuStats.ContextSwitchesDelta.Delta != oldDelta)
        {
            modified = TRUE;
        }
    }

    // Update the cycle count.
	{
        ULONG64 cycles = 0;
        ULONG64 oldDelta;

        oldDelta = m_CpuStats.CycleDelta.Delta;


		if (m_ProcessId != (quint64)SYSTEM_IDLE_PROCESS_ID)
		{
			PhGetThreadCycleTime(m->ThreadHandle, &cycles);
		}
		else
		{
			cycles = qobject_cast<CWindowsAPI*>(theAPI)->GetCpuIdleCycleTime(m_ThreadId);
		}

		m_CpuStats.CycleDelta.Update(cycles);

        if (m_CpuStats.CycleDelta.Delta != oldDelta)
            modified = TRUE;
    }

    // Update the CPU time deltas.
    m_CpuStats.CpuKernelDelta.Update(m_KernelTime);
    m_CpuStats.CpuUserDelta.Update(m_UserTime);

	// If the cycle time isn't available, we'll fall back to using the CPU time.
	m_CpuStats.UpdateStats(sysTotalTime, (m_ProcessId == (quint64)SYSTEM_IDLE_PROCESS_ID || m->ThreadHandle) ? sysTotalCycleTime : 0);

	return modified;
}

bool CWinThread::UpdateExtendedData()
{
	if (m->StartAddressResolveLevel != PhsrlFunction)
	{
		//pSymbolProvide->GetSymbolFromAddress(m_StartAddress, this, SLOT(OnSymbolFromAddress(quint64, int, const QString&, const QString&, const QString&)));
		qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetSymbolFromAddress(m_ProcessId, m_StartAddress, this, SLOT(OnSymbolFromAddress(quint64, quint64, int, const QString&, const QString&, const QString&)));
	}
	return true;
}

void CWinThread::OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName)
{
	m_StartAddressString = StartAddressString;
	m_StartAddressFileName = FileName;
}

void CWinThread::UnInit()
{
	QWriteLocker Locker(&m_Mutex);

	if (m->ThreadHandle != NULL) {
		NtClose(m->ThreadHandle);
		m->ThreadHandle = NULL;
	}
}

void CWinThread::TraceStack()
{
	qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetStackTrace(m_ProcessId, m_ThreadId, this, SIGNAL(StackTraced(const CStackTracePtr&)));
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

    /*if (threadItem->ThreadHandle)
    {
		ULONG suspendCount;
        if (threadItem->WaitReason == Suspended && NT_SUCCESS(PhGetThreadSuspendCount(threadItem->ThreadHandle, &suspendCount)))
        {
            PH_FORMAT format[4];

            PhInitFormatSR(&format[0], node->StateText->sr);
            PhInitFormatS(&format[1], L" (");
            PhInitFormatU(&format[2], suspendCount);
            PhInitFormatS(&format[3], L")");

            PhMoveReference(&node->StateText, PhFormat(format, 4, 30));
        }
    }

    getCellText->Text = PhGetStringRef(node->StateText);*/

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

	if (!m_StartAddressString.isEmpty())
		return m_StartAddressString;

	return "0x" + QString::number(m_StartAddress,16);
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
		// todo run itself as service and retry

		return ERR(tr("Failed to set I/O priority"), status);
    }
	return OK;
}

STATUS CWinThread::SetAffinityMask(quint64 Value)
{
	NTSTATUS status;
	HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_LIMITED_INFORMATION, (HANDLE)m_ThreadId)))
    {
        status = PhSetThreadAffinityMask(threadHandle, Value);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		// todo run itself as service and retry

		return ERR(tr("Failed to set CPU affinity"), status);
    }
	return OK;
}

STATUS CWinThread::Terminate()
{
	QWriteLocker Locker(&m_Mutex); 

    NTSTATUS status;
    HANDLE threadHandle;
    if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_TERMINATE, (HANDLE)m_ThreadId)))
    {
        status = NtTerminateThread(threadHandle, STATUS_SUCCESS);
        NtClose(threadHandle);
    }

    if (!NT_SUCCESS(status))
    {
		// todo run itself as service and retry

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
		// todo run itself as service and retry

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
		// todo run itself as service and retry

		return ERR(tr("Failed to resume thread"), status);
    }
	return OK;
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

static NTSTATUS NTAPI PhpThreadPermissionsOpenThread(_Out_ PHANDLE Handle, _In_ ACCESS_MASK DesiredAccess, _In_opt_ PVOID Context)
{
    return PhOpenThread(Handle, DesiredAccess, (HANDLE)Context);
}

void CWinThread::OpenPermissions()
{
	QWriteLocker Locker(&m_Mutex); 

    PhEditSecurity(NULL, (wchar_t*)GetStartAddressString().toStdWString().c_str(), L"Thread", PhpThreadPermissionsOpenThread, NULL, (HANDLE)m_ThreadId);
}