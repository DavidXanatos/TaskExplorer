#include "stdafx.h"
#include "WinDumper.h"
#include "ProcessHacker.h"
#include "../../SVC/TaskService.h"

#include <dbghelp.h>

struct SWinDumper
{
	SWinDumper()
	{
		ProcessId = 0;
		DumpType = MiniDumpNormal;
		IsWow64 = 0;

		ProcessHandle = 0;
		FileHandle = 0;

		Stop = FALSE;
		//Succeeded = FALSE;
	}

    HANDLE ProcessId;
    wstring FileName;
    MINIDUMP_TYPE DumpType;
    BOOLEAN IsWow64;

    HANDLE ProcessHandle;
    HANDLE FileHandle;

    volatile BOOLEAN Stop;
    //BOOLEAN Succeeded;
};

CWinDumper::CWinDumper(QObject* parent)
	: CMemDumper(parent)
{
	m = new SWinDumper();
}

CWinDumper::~CWinDumper()
{
	if(!wait(10*1000))
		terminate();

	delete m;
}

STATUS CWinDumper::PrepareDump(const CProcessPtr& pProcess, const QString& DumpPath)
{
	m->ProcessId = (HANDLE)pProcess->GetProcessId();
	m->FileName = QString(DumpPath).replace("/", "\\").toStdWString();
    m->DumpType = (MINIDUMP_TYPE)(MiniDumpWithFullMemory | MiniDumpWithHandleData | MiniDumpWithUnloadedModules | MiniDumpWithFullMemoryInfo | MiniDumpWithThreadInfo);

	NTSTATUS status = PhOpenProcess(&m->ProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, m->ProcessId);
    if (!NT_SUCCESS(status))
    {
		return ERR(tr("Unable to open the process"), status);
    }

#ifdef _WIN64
    PhGetProcessIsWow64(m->ProcessHandle, &m->IsWow64);
#endif

    status = PhCreateFileWin32(&m->FileHandle, (wchar_t*)m->FileName.c_str(), FILE_GENERIC_WRITE | DELETE, 0, 0, FILE_OVERWRITE_IF, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT);
    if (!NT_SUCCESS(status))
    {
		NtClose(m->ProcessHandle);
		return ERR(tr("Unable to access the dump file"), status);
    }

	return OK;
}

// Note: IsCanceling and GetProcessId are called from different threads so a mutex would be nice, but given its only read access we don't care

quint64	CWinDumper::GetProcessId() const
{
	return (quint64)m->ProcessId;
}

bool CWinDumper::IsCanceling() const
{
	return m->Stop;
}

void CWinDumper::Cancel()
{
	m->Stop = TRUE;
}

static BOOL CALLBACK PhpProcessMiniDumpCallback(
    _In_ PVOID CallbackParam,
    _In_ const PMINIDUMP_CALLBACK_INPUT CallbackInput,
    _Inout_ PMINIDUMP_CALLBACK_OUTPUT CallbackOutput
    )
{
    CWinDumper* This = (CWinDumper*)CallbackParam;

    // Don't try to send status updates if we're creating a dump of the current process.
    if (This->GetProcessId() == (quint64)NtCurrentProcessId())
        return TRUE;

    // MiniDumpWriteDump seems to get bored of calling the callback
    // after it begins dumping the process handles. The code is
    // still here in case they fix this problem in the future.

    switch (CallbackInput->CallbackType)
    {
    case CancelCallback:
        if (This->IsCanceling())
            CallbackOutput->Cancel = TRUE;
        break;
    case ModuleCallback:
		emit This->ProgressMessage(CWinDumper::tr("Processing module %1...").arg(QString::fromWCharArray(CallbackInput->Module.FullPath)));
        break;
    case ThreadCallback:
		emit This->ProgressMessage(CWinDumper::tr("Processing thread 0x%1...").arg(CallbackInput->Thread.ThreadId, 0, 16));
        break;
    }
    return TRUE;
}

void CWinDumper::run()
{
	MINIDUMP_CALLBACK_INFORMATION callbackInfo;

    callbackInfo.CallbackRoutine = PhpProcessMiniDumpCallback;
    callbackInfo.CallbackParam = this;

#ifdef _WIN64
    if (m->IsWow64)
    {
		QString ServiceName = CTaskService::RunService32();
		if (ServiceName.isEmpty())
		{
			emit StatusMessage(tr("Failed to start a 32-bit version of TaskExplorer. A 64-bit dump will be created instead."));
		}
		else
		{
			emit StatusMessage(tr("Started a 32-bit version of TaskExplorer, to create a 32-bit dump file."));

			NTSTATUS status;

			HANDLE ServiceProcessId = (HANDLE)CTaskService::SendCommand(ServiceName, "SvcGetProcessId").toULongLong();
			if (ServiceProcessId)
			{
				HANDLE serverHandle = NULL;
				status = PhOpenProcess(&serverHandle, PROCESS_DUP_HANDLE, ServiceProcessId);
				if (NT_SUCCESS(status))
				{
					HANDLE remoteProcessHandle = NULL;
					status = NtDuplicateObject(NtCurrentProcess(), m->ProcessHandle, serverHandle, &remoteProcessHandle, PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, 0, 0);
					if (NT_SUCCESS(status))
					{
						HANDLE remoteFileHandle = NULL;
						status = NtDuplicateObject(NtCurrentProcess(), m->FileHandle, serverHandle, &remoteFileHandle, FILE_GENERIC_WRITE, 0, 0);
						if (NT_SUCCESS(status))
						{
							QVariantMap Parameters;
							Parameters["LocalProcessHandle"] = (quint64)remoteProcessHandle;
							Parameters["ProcessId"] = (quint64)m->ProcessId;
							Parameters["LocalFileHandle"] = (quint64)remoteFileHandle;
							Parameters["DumpType"] = (int)m->DumpType;

							QVariantMap Request;
							Request["Command"] = "SvcWriteMiniDumpProcess";
							Request["Parameters"] = Parameters;

							QVariant Response = CTaskService::SendCommand(ServiceName, Request, -1); // no timeout, this may take a while
							if (NT_SUCCESS(Response.toUInt()))
							{
								emit StatusMessage(tr("32-bit memory dump Completed."), 0);
								goto Completed;
							}
						}
					}
				}
			}

			emit StatusMessage(tr("The 32-bit version of TaskExplorer failed to create the memory dump, Error: %1\r\nA 64-bit dump will be created instead.").arg(status), status);
		}		
    }
#endif

    if (PhWriteMiniDumpProcess(m->ProcessHandle, m->ProcessId, m->FileHandle, m->DumpType, NULL, NULL, &callbackInfo))
    {
        emit StatusMessage(tr("Memory dump Completed."), 0);
    }
    else
    {
		emit StatusMessage(tr("Failed to create Dump."), GetLastError());
    }

#ifdef _WIN64
Completed:
#endif

	NtClose(m->FileHandle);
	NtClose(m->ProcessHandle);
}

quint32 PhSvcApiWriteMiniDumpProcess(const QVariantMap& Parameters)
{
	HANDLE LocalProcessHandle = (HANDLE)Parameters["LocalProcessHandle"].toULongLong();
	HANDLE ProcessId = (HANDLE)Parameters["ProcessId"].toULongLong();
	HANDLE LocalFileHandle = (HANDLE)Parameters["LocalFileHandle"].toULongLong();
	MINIDUMP_TYPE DumpType = (MINIDUMP_TYPE)Parameters["DumpType"].toInt();

    if (!PhWriteMiniDumpProcess(LocalProcessHandle, ProcessId, LocalFileHandle, DumpType, NULL, NULL, NULL))
    {
        ULONG error = GetLastError();
        if (error == HRESULT_FROM_WIN32(ERROR_INVALID_PARAMETER))
            return STATUS_INVALID_PARAMETER;
        else
            return STATUS_UNSUCCESSFUL;
    }
	return STATUS_SUCCESS;
}