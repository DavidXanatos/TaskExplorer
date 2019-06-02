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
		StartAddressResolveLevel(PhsrlAddress),
		IsGuiThread(FALSE)
	{}

	HANDLE ThreadHandle;
	PH_SYMBOL_RESOLVE_LEVEL StartAddressResolveLevel;
	BOOLEAN IsGuiThread;
};

CWinThread::CWinThread(QObject *parent) 
	: CThreadInfo(parent) 
{
	m_StartAddress = 0;

	m_BasePriorityIncrement = 0;

	m = new SWinThread();
}

CWinThread::~CWinThread()
{
	UnInit(); // just in case 

	delete m;
}

bool CWinThread::InitStaticData(struct _SYSTEM_THREAD_INFORMATION* thread)
{
	QWriteLocker Locker(&m_Mutex);

	m_ThreadId = (quint64)thread->ClientId.UniqueThread;
	m_ProcessId = (quint64)thread->ClientId.UniqueProcess;

	m_CreateTime = QDateTime::fromTime_t((int64_t)thread->CreateTime.QuadPart / 10000000ULL - 11644473600ULL);;

    // Try to open a handle to the thread.
    if (!NT_SUCCESS(PhOpenThread(&m->ThreadHandle, THREAD_QUERY_INFORMATION, thread->ClientId.UniqueThread)))
    {
        PhOpenThread(&m->ThreadHandle, THREAD_QUERY_LIMITED_INFORMATION, thread->ClientId.UniqueThread);
    }

	PVOID startAddress = NULL;
    if (m->ThreadHandle)
    {
        PhGetThreadStartAddress(m->ThreadHandle, &startAddress);
    }

    if (!startAddress)
        startAddress = thread->StartAddress;

    m_StartAddress = (ULONG64)startAddress;

	return true;
}

bool CWinThread::UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread)
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

	m_State = (KTHREAD_STATE)thread->ThreadState;

	if (m_WaitReason != thread->WaitReason)
	{
		m_WaitReason = thread->WaitReason;
		modified = TRUE;
	}


    // Update the context switch count.
    /*{
        ULONG oldDelta;

        oldDelta = threadItem->ContextSwitchesDelta.Delta;
        PhUpdateDelta(&threadItem->ContextSwitchesDelta, thread->ContextSwitches);

        if (threadItem->ContextSwitchesDelta.Delta != oldDelta)
        {
            modified = TRUE;
        }
    }*/

    // Update the cycle count.
    /*{
        ULONG64 cycles;
        ULONG64 oldDelta;

        oldDelta = threadItem->CyclesDelta.Delta;

        if (NT_SUCCESS(PhpGetThreadCycleTime(
            threadProvider,
            threadItem,
            &cycles
            )))
        {
            PhUpdateDelta(&threadItem->CyclesDelta, cycles);

            if (threadItem->CyclesDelta.Delta != oldDelta)
            {
                modified = TRUE;
            }
        }
    }*/

	/*
    // Update the CPU time deltas.
    PhUpdateDelta(&threadItem->CpuKernelDelta, threadItem->KernelTime.QuadPart);
    PhUpdateDelta(&threadItem->CpuUserDelta, threadItem->UserTime.QuadPart);

    // Update the CPU usage.
    // If the cycle time isn't available, we'll fall back to using the CPU time.
    if (PhEnableCycleCpuUsage && (threadProvider->ProcessId == SYSTEM_IDLE_PROCESS_ID || threadItem->ThreadHandle))
    {
        threadItem->CpuUsage = (FLOAT)threadItem->CyclesDelta.Delta / PhCpuTotalCycleDelta;
    }
    else
    {
        threadItem->CpuUsage = (FLOAT)(threadItem->CpuKernelDelta.Delta + threadItem->CpuUserDelta.Delta) /
            (PhCpuKernelDelta.Delta + PhCpuUserDelta.Delta + PhCpuIdleDelta.Delta);
    }
	*/

    // Update the base priority increment.
    {
        LONG oldBasePriorityIncrement = m_BasePriorityIncrement;

		THREAD_BASIC_INFORMATION basicInfo;
        if (m->ThreadHandle && NT_SUCCESS(PhGetThreadBasicInformation(m->ThreadHandle,&basicInfo)))
        {
            m_BasePriorityIncrement = basicInfo.BasePriority;
        }
        else
        {
            m_BasePriorityIncrement = THREAD_PRIORITY_ERROR_RETURN;
        }

        if (m_BasePriorityIncrement != oldBasePriorityIncrement)
        {
            modified = TRUE;
        }
    }

    // Update the GUI thread status.
    {
        GUITHREADINFO info = { sizeof(GUITHREADINFO) };
        BOOLEAN oldIsGuiThread = m->IsGuiThread;

        m->IsGuiThread = !!GetGUIThreadInfo(m_ThreadId, &info);

        if (m->IsGuiThread != oldIsGuiThread)
            modified = TRUE;
    }

	return modified;
}

bool CWinThread::UpdateExtendedData()
{
	if (m->StartAddressResolveLevel != PhsrlFunction)
	{
		//pSymbolProvide->GetSymbolFromAddress(m_StartAddress, this, SLOT(OnSymbolFromAddress(quint64, int, const QString&, const QString&, const QString&)));
		qobject_cast<CWindowsAPI*>(theAPI)->GetSymbolProvider()->GetSymbolFromAddress(m_ProcessId, m_StartAddress, this, SLOT(OnSymbolFromAddress(quint64, quint64, int, const QString&)));
	}
	return true;
}

void CWinThread::OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString/*, const QString& FileName, const QString& SymbolName*/)
{
	m_StartAddressString = StartAddressString;
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