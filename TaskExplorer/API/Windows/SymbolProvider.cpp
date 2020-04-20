/*
 * Task Explorer -
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
#include "../../Common/Common.h"
#include "./ProcessHacker/clrsup.h"
#include "../../SVC/TaskService.h"
#include "../../Common/Settings.h"

#include <dbghelp.h>

struct SSymbolProvider
{
	SSymbolProvider(quint64 PID)
	{
		ProcessId = (HANDLE)PID;
		ThreadId = NULL;
		SymbolProvider = PhCreateSymbolProvider(ProcessId);
		ThreadHandle = NULL;

		//.NET
		IsWow64 = FALSE;
		Support = NULL;
		PredictedEip = NULL;
		PredictedEbp = NULL;
		PredictedEsp = NULL;

		LastTimeUsed = 0;
	}

	~SSymbolProvider() {
		if (SymbolProvider != NULL)
		{
			PhDereferenceObject(SymbolProvider);
			SymbolProvider = NULL;
		}
	}

	HANDLE ProcessId;
	HANDLE ThreadId;
	PPH_SYMBOL_PROVIDER SymbolProvider;
	HANDLE ThreadHandle;

	//.NET
	BOOLEAN IsWow64;
	PCLR_PROCESS_SUPPORT Support;
	QString SocketName;
    PVOID PredictedEip;
    PVOID PredictedEbp;
    PVOID PredictedEsp;

	time_t LastTimeUsed;
};


CSymbolProvider::CSymbolProvider(QObject *parent) : QThread(parent)
{
	m_bRunning = false;

	m_SymbolProviderEventRegistration = new PH_CALLBACK_REGISTRATION;
	memset(m_SymbolProviderEventRegistration, 0, sizeof(PH_CALLBACK_REGISTRATION));
}

VOID __stdcall SymbolProviderEventCallbackHandler(
    _In_opt_ PVOID Parameter,
    _In_opt_ PVOID Context
    )
{
    PPH_SYMBOL_EVENT_DATA event = (PPH_SYMBOL_EVENT_DATA)Parameter;
    CSymbolProvider* This = (CSymbolProvider*)Context;
    PPH_STRING statusMessage = NULL;

    switch (event->ActionCode)
    {
    case CBA_DEFERRED_SYMBOL_LOAD_START:
    case CBA_DEFERRED_SYMBOL_LOAD_COMPLETE:
    case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
    case CBA_SYMBOLS_UNLOADED:
        {
            PIMAGEHLP_DEFERRED_SYMBOL_LOADW64 callbackData = (PIMAGEHLP_DEFERRED_SYMBOL_LOADW64)event->EventData;
            PPH_STRING fileName = NULL;

            if (callbackData->FileName[0] != UNICODE_NULL)
            {
                fileName = PhCreateString(callbackData->FileName);
                PhMoveReference((PVOID*)&fileName, PhGetBaseName(fileName));
            }

            switch (event->ActionCode)
            {
            case CBA_DEFERRED_SYMBOL_LOAD_START:
                statusMessage = PhFormatString(L"Loading symbols from %s...", PhGetStringOrEmpty(fileName));
                break;
            case CBA_DEFERRED_SYMBOL_LOAD_COMPLETE:
                statusMessage = PhFormatString(L"Loaded symbols from %s...", PhGetStringOrEmpty(fileName));
                break;
            case CBA_DEFERRED_SYMBOL_LOAD_FAILURE:
                statusMessage = PhFormatString(L"Failed to load symbols from %s...", PhGetStringOrEmpty(fileName));
                break;
            case CBA_SYMBOLS_UNLOADED:
                statusMessage = PhFormatString(L"Unloading symbols from %s...", PhGetStringOrEmpty(fileName));
                break;
            }

            if (fileName)
                PhDereferenceObject(fileName);
        }
        break;
    case CBA_READ_MEMORY:
        {
            PIMAGEHLP_CBA_READ_MEMORY callbackEvent = (PIMAGEHLP_CBA_READ_MEMORY)event->EventData;
            //statusMessage = PhFormatString(L"Reading %lu bytes of memory from %I64u...", callbackEvent->bytes, callbackEvent->addr);
        }
        break;
    case CBA_EVENT:
        {
            PIMAGEHLP_CBA_EVENTW callbackEvent = (PIMAGEHLP_CBA_EVENTW)event->EventData;
            statusMessage = PhFormatString(L"%s", callbackEvent->desc);
        }
        break;
    case CBA_DEBUG_INFO:
        {
            statusMessage = PhFormatString(L"%s", event->EventData);
        }
        break;
    case CBA_ENGINE_PRESENT:
    case CBA_DEFERRED_SYMBOL_LOAD_PARTIAL:
    case CBA_DEFERRED_SYMBOL_LOAD_CANCEL:
    default:
        {
            //statusMessage = PhFormatString(L"Unknown: %lu", event->ActionCode);
        }
        break;
    }

    if (statusMessage)
    {
        //dprintf("%S\r\n", statusMessage->Buffer);
		emit This->StatusMessage(CastPhString(statusMessage));
    }
}

bool CSymbolProvider::Init()
{
    /*if (m->SymbolProvider)
    {
        if (m->SymbolProvider->IsRealHandle)
            m->ProcessHandle = m->SymbolProvider->ProcessHandle;
    }*/

	PhRegisterCallback(&PhSymbolEventCallback, (PPH_CALLBACK_FUNCTION)SymbolProviderEventCallbackHandler, this, m_SymbolProviderEventRegistration);

	// start thread
	m_bRunning = true;
	start();

	return true;
}

CSymbolProvider::~CSymbolProvider()
{
	UnInit();

	delete m_SymbolProviderEventRegistration;
}

void CSymbolProvider::UnInit()
{
	m_bRunning = false;

	//quit();

	if (!wait(10 * 1000))
		terminate();

	PhUnregisterCallback(&PhSymbolEventCallback, m_SymbolProviderEventRegistration);

	// cleanup unfinished tasks
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

	CSymbolProviderJob* pJob = new CSymbolProviderJob(ProcessId, Address); 
	pJob->moveToThread(this);
	QObject::connect(pJob, SIGNAL(SymbolFromAddress(quint64, quint64, int, const QString&, const QString&, const QString&)), receiver, member, Qt::QueuedConnection);

	QMutexLocker Locker(&m_JobMutex);
	m_JobQueue.append(pJob);
}

void CSymbolProvider::GetAddressFromSymbol(quint64 ProcessId, const QString& Symbol, QObject *receiver, const char *member)
{
    if (!QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSymbolProvider::GetAddressFromSymbol() called with no event dispatcher");
        return;
    }

	CAddressProviderJob* pJob = new CAddressProviderJob(ProcessId, Symbol); 
	pJob->moveToThread(this);
	QObject::connect(pJob, SIGNAL(AddressFromSymbol(quint64, const QString&, quint64)), receiver, member, Qt::QueuedConnection);

	QMutexLocker Locker(&m_JobMutex);
	m_JobQueue.append(pJob);
}


quint64 CSymbolProvider::GetStackTrace(quint64 ProcessId, quint64 ThreadId, QObject *receiver, const char *member)
{
    if (!QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSymbolProvider::GetSymbolFromAddress() called with no event dispatcher");
        return NULL;
    }

	CStackProviderJob* pJob = new CStackProviderJob(ProcessId, ThreadId); 
	pJob->moveToThread(this);
	QObject::connect(pJob, SIGNAL(StackTraced(const CStackTracePtr&)), receiver, member, Qt::QueuedConnection);

	QMutexLocker Locker(&m_JobMutex);
	int i = 0; // insert Stack Traces before other symbol requests
	for (; i < m_JobQueue.size(); i++)
	{
		if (qobject_cast<CStackProviderJob*>(m_JobQueue.at(i)) == NULL)
			break;
	}
	m_JobQueue.insert(i, pJob);

	return (quint64)((CAbstractSymbolProviderJob*)pJob);
}

void CSymbolProvider::CancelJob(quint64 JobID)
{
	QMutexLocker Locker(&m_JobMutex);
	for (int i = 0; i < m_JobQueue.size(); i++)
	{
		CAbstractSymbolProviderJob* pJob = m_JobQueue.at(i);
		if (((quint64)pJob) == JobID)
		{
			pJob->deleteLater();
			m_JobQueue.removeAt(i);
			break;
		}
	}
}

// thrdprov.c
typedef struct _PH_THREAD_SYMBOL_LOAD_CONTEXT
{
    HANDLE ProcessId;
	SSymbolProvider* m;
    PPH_SYMBOL_PROVIDER SymbolProvider;
} PH_THREAD_SYMBOL_LOAD_CONTEXT, *PPH_THREAD_SYMBOL_LOAD_CONTEXT;

// thrdprov.c
BOOLEAN __stdcall LoadSymbolsEnumGenericModulesCallback(_In_ PPH_MODULE_INFO Module, _In_opt_ PVOID Context)
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
            PhEnumGenericModules(m->ProcessId, m->SymbolProvider->ProcessHandle, 0, (PPH_ENUM_GENERIC_MODULES_CALLBACK)LoadSymbolsEnumGenericModulesCallback, &loadContext);
        }
        else
        {
            // We can't enumerate the process modules. Load symbols for ntdll.dll and kernel32.dll.
            loadContext.ProcessId = NtCurrentProcessId();
            PhEnumGenericModules(NtCurrentProcessId(), NtCurrentProcess(), 0, (PPH_ENUM_GENERIC_MODULES_CALLBACK)LoadBasicSymbolsEnumGenericModulesCallback, &loadContext );
        }

        // Load kernel module symbols as well.
        if (m->ProcessId != SYSTEM_PROCESS_ID)
        {
            loadContext.ProcessId = SYSTEM_PROCESS_ID;
            PhEnumGenericModules(SYSTEM_PROCESS_ID, NULL, 0, (PPH_ENUM_GENERIC_MODULES_CALLBACK)LoadSymbolsEnumGenericModulesCallback, &loadContext);
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

VOID PhLoadSymbolProviderOptions(_Inout_ PPH_SYMBOL_PROVIDER SymbolProvider)
{
	bool DbgHelpUndecorate = theConf->GetBool("Options/DbgHelpUndecorate", true);
	PhSetOptionsSymbolProvider(SYMOPT_UNDNAME, DbgHelpUndecorate ? SYMOPT_UNDNAME : 0);

	bool DbgHelpSearch = theConf->GetInt("Options/DbgHelpSearch", 2) == 1;
	QString DbgHelpSearchPath = theConf->GetString("Options/DbgHelpSearchPath", "SRV*C:\\Symbols*https://msdl.microsoft.com/download/symbols");
	if (DbgHelpSearch && DbgHelpSearchPath.length() > 0)
		PhSetSearchPathSymbolProvider(SymbolProvider, (wchar_t*)DbgHelpSearchPath.toStdWString().c_str());
}

void CSymbolProvider::run()
{
	//if(WindowsVersion >= WINDOWS_10_RS1)
	//	SetThreadDescription(GetCurrentThread(), L"Symbol Provider");

	//exec();

	time_t LastCleanUp = 0;

	while (m_bRunning)
	{
		time_t OldTime = GetTime() - 3; // cleanup everythign older than 3 sec
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

			PhLoadSymbolProviderOptions(m->SymbolProvider);

			PhLoadSymbolsThreadProvider(m);
		}
		m->LastTimeUsed = GetTime();

		pJob->Run(m);

		pJob->deleteLater();
	}

	// cleanup provider cache
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

void CAddressProviderJob::Run(struct SSymbolProvider* m)
{
	this->m = m;

	quint64 Address = 0;

	PH_SYMBOL_INFORMATION symbolInfo;
	if (PhGetSymbolFromName(m->SymbolProvider, (wchar_t*)m_Symbol.toStdWString().c_str(), &symbolInfo))
		Address = (quint64)symbolInfo.Address;

	emit AddressFromSymbol(m_ProcessId, m_Symbol, Address);
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
	m->ThreadId = (HANDLE)m_ThreadId;

	m_StackTrace = CStackTracePtr(new CStackTrace(m_ProcessId, m_ThreadId));

	NTSTATUS status;
	CLIENT_ID clientId;

    clientId.UniqueProcess = (HANDLE)m_ProcessId;
    clientId.UniqueThread = (HANDLE)m_ThreadId;

	// case PluginThreadStackInitializing:
#ifdef WIN64
    HANDLE processHandle;

    if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, clientId.UniqueProcess)))
    {
        PhGetProcessIsWow64(processHandle, &m->IsWow64);
        NtClose(processHandle);
    }
#endif
	//

	PhLoadSymbolsThreadProvider(m);

	if (!NT_SUCCESS(status = PhOpenThread(&m->ThreadHandle, THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, (HANDLE)m_ThreadId)))
    {
        if (KphIsConnected())
        {
            status = PhOpenThread(&m->ThreadHandle, THREAD_QUERY_LIMITED_INFORMATION, (HANDLE)m_ThreadId);
        }
    }

	//case PluginThreadStackBeginDefaultWalkStack:
	if (theConf->GetBool("Options/DbgTraceDotNet", true))
	{
		BOOLEAN isDotNet;
		if (NT_SUCCESS(PhGetProcessIsDotNet(clientId.UniqueProcess, &isDotNet)) && isDotNet)
		{
			m->Support = (PCLR_PROCESS_SUPPORT)CreateClrProcessSupport(clientId.UniqueProcess);

#ifdef WIN64
			if (m->IsWow64)
				m->SocketName = CTaskService::RunWorker(false, true);
#endif
		}
	}
	//

	if (NT_SUCCESS(status))
	{
		status = PhWalkThreadStack(m->ThreadHandle, m->SymbolProvider->ProcessHandle, &clientId, m->SymbolProvider,
			PH_WALK_I386_STACK | PH_WALK_AMD64_STACK | PH_WALK_KERNEL_STACK, PhpWalkThreadStackCallback, this);
	}

	// case PluginThreadStackEndDefaultWalkStack:
    if (m->Support)
    {
        FreeClrProcessSupport(m->Support);
        m->Support = NULL;
    }

#ifdef WIN64
    //if (!m->SocketName.isEmpty())
    //	CTaskService::TerminateService(m->SocketName);
#endif
	//

	emit StackTraced(m_StackTrace);
}

void PredictAddressesFromClrData(PCLR_PROCESS_SUPPORT Support, HANDLE ThreadId, PVOID PcAddress, PVOID FrameAddress, PVOID StackAddress, PVOID *PredictedEip, PVOID *PredictedEbp, PVOID *PredictedEsp)
{
#ifdef _WIN64
    *PredictedEip = NULL;
    *PredictedEbp = NULL;
    *PredictedEsp = NULL;
#else
    IXCLRDataTask *task;

    *PredictedEip = NULL;
    *PredictedEbp = NULL;
    *PredictedEsp = NULL;

    if (SUCCEEDED(IXCLRDataProcess_GetTaskByOSThreadID(Support->DataProcess, HandleToUlong(ThreadId), &task)))
    {
        IXCLRDataStackWalk *stackWalk;

        if (SUCCEEDED(IXCLRDataTask_CreateStackWalk(task, 0xf, &stackWalk)))
        {
            HRESULT result;
            BOOLEAN firstTime = TRUE;
            CONTEXT context;
            ULONG32 contextSize;

            memset(&context, 0, sizeof(CONTEXT));
            context.ContextFlags = CONTEXT_CONTROL;
            context.Eip = PtrToUlong(PcAddress);
            context.Ebp = PtrToUlong(FrameAddress);
            context.Esp = PtrToUlong(StackAddress);

            result = IXCLRDataStackWalk_SetContext2(stackWalk, CLRDATA_STACK_SET_CURRENT_CONTEXT, sizeof(CONTEXT), (BYTE *)&context);

            if (SUCCEEDED(result = IXCLRDataStackWalk_Next(stackWalk)) && result != S_FALSE)
            {
                if (SUCCEEDED(IXCLRDataStackWalk_GetContext(stackWalk, CONTEXT_CONTROL, sizeof(CONTEXT), &contextSize, (BYTE *)&context)))
                {
                    *PredictedEip = UlongToPtr(context.Eip);
                    *PredictedEbp = UlongToPtr(context.Ebp);
                    *PredictedEsp = UlongToPtr(context.Esp);
                }
            }

            IXCLRDataStackWalk_Release(stackWalk);
        }

        IXCLRDataTask_Release(task);
    }
#endif
}

void CallPredictAddressesFromClrData(const QString& SocketName, HANDLE ProcessId, HANDLE ThreadId, PVOID PcAddress, PVOID FrameAddress, PVOID StackAddress,PVOID *PredictedEip, PVOID *PredictedEbp, PVOID *PredictedEsp)
{
    *PredictedEip = NULL;
    *PredictedEbp = NULL;
    *PredictedEsp = NULL;

	QVariantMap Parameters;
	Parameters["ProcessId"] = (quint64)HandleToUlong(ProcessId);
	Parameters["ThreadId"] = (quint64)HandleToUlong(ThreadId);
	Parameters["PcAddress"] = (quint64)PtrToUlong(PcAddress);
	Parameters["FrameAddress"] = (quint64)PtrToUlong(FrameAddress);
	Parameters["StackAddress"] = (quint64)PtrToUlong(StackAddress);

	QVariantMap Request;
	Request["Command"] = "PredictAddressesFromClrData";
	Request["Parameters"] = Parameters;

	QVariant Response = CTaskService::SendCommand(SocketName, Request);

	if(Response.type() == QVariant::Map)
	{
		QVariantMap Result = Response.toMap();

		*PredictedEip = UlongToPtr(Result["PredictedEip"].toULongLong());
        *PredictedEbp = UlongToPtr(Result["PredictedEbp"].toULongLong());
        *PredictedEsp = UlongToPtr(Result["PredictedEsp"].toULongLong());
	}
}

QVariant SvcApiPredictAddressesFromClrData(const QVariantMap& Parameters)
{
    PCLR_PROCESS_SUPPORT support = CreateClrProcessSupport(UlongToHandle(Parameters["ProcessId"].toULongLong()));
    if (!support)
        return false;

	PVOID predictedEip;
    PVOID predictedEbp;
    PVOID predictedEsp;
	PredictAddressesFromClrData(support, 
		UlongToHandle(Parameters["ThreadId"].toULongLong()), 
		UlongToPtr(Parameters["PcAddress"].toULongLong()), 
		UlongToPtr(Parameters["FrameAddress"].toULongLong()), 
		UlongToPtr(Parameters["StackAddress"].toULongLong()), 
		&predictedEip, &predictedEbp, &predictedEsp);

	FreeClrProcessSupport(support);

	QVariantMap Result;
	Result["PredictedEip"] = (quint64)PtrToUlong(predictedEip);
	Result["PredictedEbp"] = (quint64)PtrToUlong(predictedEbp);
    Result["PredictedEsp"] =  (quint64)PtrToUlong(predictedEsp);
	return Result;
}

QString CallGetRuntimeNameByAddress(const QString& SocketName, HANDLE ProcessId, ULONG64 Address, PULONG64 Displacement)
{
	QVariantMap Parameters;
	Parameters["ProcessId"] = (quint64)HandleToUlong(ProcessId);
	Parameters["Address"] = (quint64)Address;
	
	QVariantMap Request;
	Request["Command"] = "GetRuntimeNameByAddressClrProcess";
	Request["Parameters"] = Parameters;

	QVariant Response = CTaskService::SendCommand(SocketName, Request);

	if (Response.type() == QVariant::Map)
	{
		QVariantMap Result = Response.toMap();

		if (Displacement)
			*Displacement = Result["Displacement"].toULongLong();

		return Result["Name"].toString();
	}
	return QString();
}

QVariant SvcApiGetRuntimeNameByAddressClrProcess(const QVariantMap& Parameters)
{
	PCLR_PROCESS_SUPPORT support = CreateClrProcessSupport(UlongToHandle(Parameters["ProcessId"].toULongLong()));
    if (!support)
        return false;

	ULONG64 Displacement;
	PPH_STRING name = GetRuntimeNameByAddressClrProcess(support, Parameters["Address"].toULongLong(), &Displacement);

	FreeClrProcessSupport(support);

	if (!name)
		return false;

	QVariantMap Result;
	Result["Name"] = CastPhString(name);
	Result["Displacement"] = (quint64)Displacement;
	return Result;
}

// PhpWalkThreadStackCallback
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

	// case PluginThreadStackResolveSymbol:
    QString ManagedSymbol;
    ULONG64 displacement;

    if (m->Support)
    {
#ifndef WIN64
        PVOID predictedEip = m->PredictedEip;
        PVOID predictedEbp = m->PredictedEbp;
        PVOID predictedEsp = m->PredictedEsp;

        PredictAddressesFromClrData(m->Support, m->ThreadId, StackFrame->PcAddress, StackFrame->FrameAddress, StackFrame->StackAddress, 
			&m->PredictedEip, &m->PredictedEbp, &m->PredictedEsp);

        // Fix up dbghelp EBP with real EBP given by the CLR data routines.
        if (StackFrame->PcAddress == predictedEip)
        {
            StackFrame->FrameAddress = predictedEbp;
            StackFrame->StackAddress = predictedEsp;
        }
#endif

        ManagedSymbol = CastPhString(GetRuntimeNameByAddressClrProcess(m->Support, (ULONG64)StackFrame->PcAddress, &displacement));
    }
#ifdef _WIN64
    else if (m->IsWow64 && !m->SocketName.isEmpty())
    {
        PVOID predictedEip = m->PredictedEip;
        PVOID predictedEbp = m->PredictedEbp;
        PVOID predictedEsp = m->PredictedEsp;

        CallPredictAddressesFromClrData(m->SocketName, m->ProcessId, m->ThreadId, StackFrame->PcAddress, StackFrame->FrameAddress, StackFrame->StackAddress, 
			&m->PredictedEip, &m->PredictedEbp, &m->PredictedEsp);

        // Fix up dbghelp EBP with real EBP given by the CLR data routines.
        if (StackFrame->PcAddress == predictedEip)
        {
            StackFrame->FrameAddress = predictedEbp;
            StackFrame->StackAddress = predictedEsp;
        }

        ManagedSymbol = CallGetRuntimeNameByAddress(m->SocketName, m->ProcessId, (ULONG64)StackFrame->PcAddress, &displacement);
    }
#endif

    if (!ManagedSymbol.isEmpty())
    {
		if (displacement != 0)
			ManagedSymbol.append(tr(" + 0x%1").arg(displacement, 0, 16));
		ManagedSymbol.append(tr(" <-- %1").arg(Symbol));

		Symbol = ManagedSymbol;
    }
	//

	quint64 Params[4] = { (quint64)StackFrame->Params[0], (quint64)StackFrame->Params[1], (quint64)StackFrame->Params[2], (quint64)StackFrame->Params[3] };
	
	QString FileInfo;

    PPH_STRING fileName;
    PH_SYMBOL_LINE_INFORMATION lineInfo;
	if(PhGetLineFromAddress(m->SymbolProvider, (ULONG64)StackFrame->PcAddress, &fileName, NULL, &lineInfo))
	{
		FileInfo = tr("File: %1: line %2").arg(CastPhString(fileName)).arg(lineInfo.LineNumber);
	}


	m_StackTrace->AddFrame(Symbol, (quint64)StackFrame->PcAddress, (quint64)StackFrame->ReturnAddress, (quint64)StackFrame->FrameAddress, 
									(quint64)StackFrame->StackAddress, (quint64)StackFrame->BStoreAddress, Params, (quint64)StackFrame->Flags, FileInfo);
}