/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2010-2016 wj32
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
#include "SymbolProvider.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"
#include "../../Common/Functions.h"

#include <evntcons.h>

struct SSymbolProvider
{
	SSymbolProvider(quint64 PID)
	{
		ProcessId = (HANDLE)PID;
		SymbolProvider = PhCreateSymbolProvider(ProcessId);
	}

	~SSymbolProvider() {
		if (SymbolProvider != NULL)
		{
			PhDereferenceObject(SymbolProvider);
			SymbolProvider = NULL;
		}
	}

	HANDLE ProcessId;
	PPH_SYMBOL_PROVIDER SymbolProvider;

	time_t LastTimeUsed;
};


CSymbolProvider::CSymbolProvider(QObject *parent) : QThread(parent)
{
	m_bRunning = false;
}

bool CSymbolProvider::Init()
{
    /*if (m->SymbolProvider)
    {
        if (m->SymbolProvider->IsRealHandle)
            m->ProcessHandle = m->SymbolProvider->ProcessHandle;
    }*/

	// start thread
	m_bRunning = true;
	start();

	return true;
}

CSymbolProvider::~CSymbolProvider()
{
	UnInit();
}

void CSymbolProvider::UnInit()
{
	m_bRunning = true;

	//quit();

	if (!wait(10 * 1000))
		terminate();

	while (!m_JobQueue.isEmpty()) {
		m_JobQueue.takeFirst()->deleteLater();
	}
}

void CSymbolProvider::GetSymbolFromAddress(quint64 ProcessId, quint64 Address, QObject *receiver, const char *member)
{
    if (!QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSymbolProvider::GetSymbolFromAddress() called with no event dispatcher");
        return;
    }

	/*const char* bracketPosition = strchr(member, '(');
	if (!bracketPosition || !(member[0] >= '0' && member[0] <= '2')) {
		qWarning("CSymbolProvider::GetSymbolFromAddress: Invalid slot specification");
		return;
	}
	QByteArray methodName(member+1, bracketPosition - 1 - member); // extract method name
	QMetaObject::invokeMethod(const_cast<QObject *>(receiver), methodName.constData(), Qt::AutoConnection, Q_ARG(const QString&, "test"));*/

	CSymbolProviderJob* pJob = new CSymbolProviderJob(ProcessId, Address, this); 
	QObject::connect(pJob, SIGNAL(SymbolFromAddress(quint64, quint64, int, const QString&, const QString&, const QString&)), receiver, member, Qt::QueuedConnection);

	QMutexLocker Locker(&m_JobMutex);
	m_JobQueue.append(pJob);
}

void CSymbolProvider::GetStackTrace(quint64 ProcessId, quint64 ThreadId, QObject *receiver, const char *member)
{
    if (!QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSymbolProvider::GetSymbolFromAddress() called with no event dispatcher");
        return;
    }

	CStackProviderJob* pJob = new CStackProviderJob(ProcessId, ThreadId, this); 
	QObject::connect(pJob, SIGNAL(StackTraced(const CStackTracePtr&)), receiver, member, Qt::QueuedConnection);

	QMutexLocker Locker(&m_JobMutex);
	m_JobQueue.append(pJob);
}

// thrdprov.c
typedef struct _PH_THREAD_SYMBOL_LOAD_CONTEXT
{
    HANDLE ProcessId;
	SSymbolProvider* m;
    PPH_SYMBOL_PROVIDER SymbolProvider;
} PH_THREAD_SYMBOL_LOAD_CONTEXT, *PPH_THREAD_SYMBOL_LOAD_CONTEXT;

// thrdprov.c
BOOLEAN LoadSymbolsEnumGenericModulesCallback(_In_ PPH_MODULE_INFO Module, _In_opt_ PVOID Context)
{
    PPH_THREAD_SYMBOL_LOAD_CONTEXT context = (PPH_THREAD_SYMBOL_LOAD_CONTEXT)Context;
    PPH_SYMBOL_PROVIDER symbolProvider = context->SymbolProvider;

    //if (context->m->Terminating)
    //    return FALSE;

    // If we're loading kernel module symbols for a process other than System, ignore modules which
    // are in user space. This may happen in Windows 7.
    if (context->ProcessId == SYSTEM_PROCESS_ID &&
        context->m->ProcessId != SYSTEM_PROCESS_ID &&
        (ULONG_PTR)Module->BaseAddress <= PhSystemBasicInformation.MaximumUserModeAddress)
    {
        return TRUE;
    }

    PhLoadModuleSymbolProvider(symbolProvider, Module->FileName->Buffer, (ULONG64)Module->BaseAddress, Module->Size);

    return TRUE;
}

// thrdprov.c
BOOLEAN LoadBasicSymbolsEnumGenericModulesCallback(_In_ PPH_MODULE_INFO Module, _In_opt_ PVOID Context)
{
    PPH_THREAD_SYMBOL_LOAD_CONTEXT context = (PPH_THREAD_SYMBOL_LOAD_CONTEXT)Context;
    PPH_SYMBOL_PROVIDER symbolProvider = context->SymbolProvider;

    //if (context->m->Terminating)
    //    return FALSE;

    if (PhEqualString2(Module->Name, L"ntdll.dll", TRUE) ||
        PhEqualString2(Module->Name, L"kernel32.dll", TRUE))
    {
        PhLoadModuleSymbolProvider(symbolProvider, Module->FileName->Buffer, (ULONG64)Module->BaseAddress, Module->Size);
    }

    return TRUE;
}

// thrdprov.c
VOID PhLoadSymbolsThreadProvider(SSymbolProvider* m)
{
	PH_THREAD_SYMBOL_LOAD_CONTEXT loadContext;

	loadContext.m = m;
    loadContext.SymbolProvider = m->SymbolProvider;

	if (m->ProcessId != SYSTEM_IDLE_PROCESS_ID)
    {
        if (m->SymbolProvider->IsRealHandle || m->ProcessId == SYSTEM_PROCESS_ID)
        {
            loadContext.ProcessId = m->ProcessId;
            PhEnumGenericModules(m->ProcessId, m->SymbolProvider->ProcessHandle, 0, LoadSymbolsEnumGenericModulesCallback, &loadContext);
        }
        else
        {
            // We can't enumerate the process modules. Load symbols for ntdll.dll and kernel32.dll.
            loadContext.ProcessId = NtCurrentProcessId();
            PhEnumGenericModules(NtCurrentProcessId(), NtCurrentProcess(), 0, LoadBasicSymbolsEnumGenericModulesCallback, &loadContext );
        }

        // Load kernel module symbols as well.
        if (m->ProcessId != SYSTEM_PROCESS_ID)
        {
            loadContext.ProcessId = SYSTEM_PROCESS_ID;
            PhEnumGenericModules(SYSTEM_PROCESS_ID, NULL, 0, LoadSymbolsEnumGenericModulesCallback, &loadContext);
        }
    }
    else
    {
        // System Idle Process has one thread for each CPU, each having a start address at
        // KiIdleLoop. We need to load symbols for the kernel.

        PRTL_PROCESS_MODULES kernelModules;

        if (NT_SUCCESS(PhEnumKernelModules(&kernelModules)))
        {
            if (kernelModules->NumberOfModules > 0)
            {
                PPH_STRING fileName;
                PPH_STRING newFileName;

                fileName = PhConvertMultiByteToUtf16((PSTR)kernelModules->Modules[0].FullPathName);
                newFileName = PhGetFileName(fileName);
                PhDereferenceObject(fileName);

                PhLoadModuleSymbolProvider(m->SymbolProvider, newFileName->Buffer, (ULONG64)kernelModules->Modules[0].ImageBase, kernelModules->Modules[0].ImageSize);
                PhDereferenceObject(newFileName);
            }

            PhFree(kernelModules);
        }
    }
}

void CSymbolProvider::run()
{
	//exec();

	time_t LastCleanUp = 0;

	while (m_bRunning)
	{
		time_t OldTime = GetTime() - 60; // cleanup everythign older than 60 sec
		if (LastCleanUp < OldTime)
		{
			foreach(SSymbolProvider* m, mm.values())
			{
				if (m->LastTimeUsed < OldTime)
				{
					mm.remove((quint64)m->ProcessId);
					delete m;
				}
			}

			LastCleanUp = GetTime();
		}


		QMutexLocker Locker(&m_JobMutex);
		if (m_JobQueue.isEmpty()) {
			Locker.unlock();
			QThread::msleep(250);
			continue;
		}
		CAbstractSymbolProviderJob* pJob = m_JobQueue.takeFirst();
		Locker.unlock();

		SSymbolProvider* &m = mm[pJob->m_ProcessId];
		if (m == NULL)
		{
			m = new SSymbolProvider(pJob->m_ProcessId);

			PhLoadSymbolsThreadProvider(m);
		}
		m->LastTimeUsed = GetTime();

		pJob->Run(m);

		pJob->deleteLater();
	}

	foreach(SSymbolProvider* m, mm)
		delete m;
	mm.clear();
}

void CSymbolProviderJob::Run(struct SSymbolProvider* m)
{
	this->m = m;

	PH_SYMBOL_RESOLVE_LEVEL StartAddressResolveLevel = PhsrlAddress;
	PPH_STRING fileName = NULL;
	PPH_STRING symbolName = NULL;
	PPH_STRING AddressString = PhGetSymbolFromAddress(m->SymbolProvider, m_Address, &StartAddressResolveLevel, &fileName, &symbolName, NULL);

	if (StartAddressResolveLevel == PhsrlAddress /*&& data->ThreadProvider->SymbolsLoadedRunId < data->RunId*/)
	{
		// The process may have loaded new modules, so load symbols for those and try again.
		PhLoadSymbolsThreadProvider(m);
		AddressString = PhGetSymbolFromAddress(m->SymbolProvider, m_Address, &StartAddressResolveLevel, &fileName, &symbolName, NULL);
	}

	emit SymbolFromAddress(m_ProcessId, m_Address, (int)StartAddressResolveLevel, CastPhString(AddressString), CastPhString(fileName), CastPhString(symbolName));
}

// thrdstk.c
BOOLEAN NTAPI PhpWalkThreadStackCallback(_In_ PPH_THREAD_STACK_FRAME StackFrame, _In_opt_ PVOID Context)
{
	((CStackProviderJob*)Context)->OnCallBack(StackFrame);
    return TRUE;
}

void CStackProviderJob::Run(struct SSymbolProvider* m)
{
	this->m = m;

	m_StackTrace = CStackTracePtr(new CStackTrace(m_ProcessId, m_ThreadId));

	PhLoadSymbolsThreadProvider(m);

	NTSTATUS status;
    HANDLE threadHandle;
	CLIENT_ID clientId;

    clientId.UniqueProcess = (HANDLE)m_ProcessId;
    clientId.UniqueThread = (HANDLE)m_ThreadId;

	if (!NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, (HANDLE)m_ThreadId)))
    {
        if (KphIsConnected())
        {
            status = PhOpenThread(&threadHandle, THREAD_QUERY_LIMITED_INFORMATION, (HANDLE)m_ThreadId);
        }
    }

	if (NT_SUCCESS(status))
	{
		status = PhWalkThreadStack(threadHandle, m->SymbolProvider->ProcessHandle, &clientId, m->SymbolProvider,
			PH_WALK_I386_STACK | PH_WALK_AMD64_STACK | PH_WALK_KERNEL_STACK, PhpWalkThreadStackCallback, this);
	}

	emit StackTraced(m_StackTrace);
}

void CStackProviderJob::OnCallBack(struct _PH_THREAD_STACK_FRAME* StackFrame)
{
    //if (m->Terminating)
    //    return FALSE;

    // PhFormatString(L"Processing stack frame %u...", threadStackContext->NewList->Count)

    PPH_STRING symbol = PhGetSymbolFromAddress(m->SymbolProvider, (ULONG64)StackFrame->PcAddress, NULL, NULL, NULL, NULL);

	QString Symbol = CastPhString(symbol);

    if (symbol && (StackFrame->Flags & PH_THREAD_STACK_FRAME_I386) && !(StackFrame->Flags & PH_THREAD_STACK_FRAME_FPO_DATA_PRESENT))
    {
        Symbol += tr(" (No unwind info)");
    }

	quint64 Params[4] = { (quint64)StackFrame->Params[0], (quint64)StackFrame->Params[1], (quint64)StackFrame->Params[2], (quint64)StackFrame->Params[3] };
	
	m_StackTrace->AddFrame(Symbol, (quint64)StackFrame->PcAddress, (quint64)StackFrame->ReturnAddress, (quint64)StackFrame->FrameAddress, 
									(quint64)StackFrame->StackAddress, (quint64)StackFrame->BStoreAddress, Params, (quint64)StackFrame->Flags);
}