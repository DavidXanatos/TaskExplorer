/*
 * System Informer Plugins -
 *   qt port of Debug View Plugin
 *
 * Copyright (C) 2015-2017 dmex
 * Copyright (C) 2020-2022 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 * 
 */

#include "stdafx.h"
#include "WinDbgMonitor.h"
#include "../ProcessHacker.h"
#include "../ProcessHacker/GpuCounters.h"
#include "../WinProcess.h"
#include "../WindowsAPI.h"
#include "../../../../MiscHelpers/Common/Settings.h"
#include <Sddl.h>

#define DBWIN_BUFFER_READY L"DBWIN_BUFFER_READY"
#define DBWIN_DATA_READY L"DBWIN_DATA_READY"
#define DBWIN_BUFFER L"DBWIN_BUFFER"

#define DBWIN_BUFFER_READY_OBJECT_NAME L"\\BaseNamedObjects\\DBWIN_BUFFER_READY"
#define DBWIN_DATA_READY_OBJECT_NAME L"\\BaseNamedObjects\\DBWIN_DATA_READY"
#define DBWIN_BUFFER_SECTION_NAME L"\\BaseNamedObjects\\DBWIN_BUFFER"

// The Win32 OutputDebugString buffer.
typedef struct _DBWIN_PAGE_BUFFER
{
    ULONG ProcessId; /** The ID of the process. */
    CHAR Buffer[PAGE_SIZE - sizeof(ULONG)]; /** The buffer containing the debug message. */
} DBWIN_PAGE_BUFFER, *PDBWIN_PAGE_BUFFER;


struct SWinDbgMonitor
{
	SWinDbgMonitor()
	{
		LocalCaptureEnabled = FALSE;
		LocalBufferReadyEvent = NULL;
		LocalDataReadyEvent = NULL;
		LocalDataBufferHandle = NULL;
		LocalDebugBuffer = NULL;

		GlobalCaptureEnabled = FALSE;
		GlobalBufferReadyEvent = NULL;
		GlobalDataReadyEvent = NULL;
		GlobalDataBufferHandle = NULL;
		GlobalDebugBuffer = NULL;

		KernelCaptureEnabled = FALSE;

		DbgCreateSecurityAttributes();
	}

	~SWinDbgMonitor()
	{
		DbgCleanupSecurityAttributes();
	}

	SECURITY_ATTRIBUTES SecurityAttributes;

	BOOLEAN LocalCaptureEnabled;
    HANDLE LocalBufferReadyEvent;
    HANDLE LocalDataReadyEvent;
    HANDLE LocalDataBufferHandle;
    PDBWIN_PAGE_BUFFER LocalDebugBuffer;

	BOOLEAN GlobalCaptureEnabled;
    HANDLE GlobalBufferReadyEvent;
    HANDLE GlobalDataReadyEvent;
    HANDLE GlobalDataBufferHandle;
    PDBWIN_PAGE_BUFFER GlobalDebugBuffer;

	BOOLEAN KernelCaptureEnabled;

	BOOL DbgCreateSecurityAttributes()
	{
		SecurityAttributes.nLength = sizeof(SECURITY_ATTRIBUTES);
		SecurityAttributes.bInheritHandle = FALSE;
		return ConvertStringSecurityDescriptorToSecurityDescriptor(
			L"D:(A;;GRGWGX;;;WD)(A;;GA;;;SY)(A;;GA;;;BA)(A;;GRGWGX;;;AN)(A;;GRGWGX;;;RC)(A;;GRGWGX;;;S-1-15-2-1)S:(ML;;NW;;;LW)",
			SDDL_REVISION, &SecurityAttributes.lpSecurityDescriptor, NULL);
	}

	void DbgCleanupSecurityAttributes()
	{
		if (SecurityAttributes.lpSecurityDescriptor)
		{
			LocalFree(SecurityAttributes.lpSecurityDescriptor);
			SecurityAttributes.lpSecurityDescriptor = NULL;
		}
	}

	VOID DbgFormatObjectName(
		_In_ BOOLEAN LocalName,
		_In_ PWSTR OriginalName,
		_Out_ PUNICODE_STRING ObjectName
		)
	{
		SIZE_T length;
		SIZE_T originalNameLength;

		// Sessions other than session 0 require SeCreateGlobalPrivilege.
		if (LocalName && NtCurrentPeb()->SessionId != 0)
		{
			WCHAR buffer[256] = L"";

			memcpy(buffer, L"\\Sessions\\", 10 * sizeof(WCHAR));
			_ultow(NtCurrentPeb()->SessionId, buffer + 10, 10);
			length = PhCountStringZ(buffer);
			originalNameLength = PhCountStringZ(OriginalName);
			memcpy(buffer + length, OriginalName, (originalNameLength + 1) * sizeof(WCHAR));
			length += originalNameLength;

			ObjectName->Buffer = buffer;
			ObjectName->MaximumLength = (ObjectName->Length = (USHORT)(length * sizeof(WCHAR))) + sizeof(WCHAR);
		}
		else
		{
			RtlInitUnicodeString(ObjectName, OriginalName);
		}
	}

	STATUS Init(bool bGlobal)
	{
		BOOLEAN& CaptureEnabled = bGlobal ? GlobalCaptureEnabled : LocalCaptureEnabled;
		HANDLE& BufferReadyEvent = bGlobal ? GlobalBufferReadyEvent : LocalBufferReadyEvent;
		HANDLE& DataReadyEvent = bGlobal ? GlobalDataReadyEvent : LocalDataReadyEvent;
		HANDLE& DataBufferHandle = bGlobal ? GlobalDataBufferHandle : LocalDataBufferHandle;
		PDBWIN_PAGE_BUFFER& DebugBuffer = bGlobal ? GlobalDebugBuffer : LocalDebugBuffer;

        SIZE_T viewSize;
        LARGE_INTEGER maximumSize;
        OBJECT_ATTRIBUTES objectAttributes;
        UNICODE_STRING objectName;

        maximumSize.QuadPart = PAGE_SIZE;
        viewSize = sizeof(DBWIN_PAGE_BUFFER);

        if (!(BufferReadyEvent = CreateEvent(&SecurityAttributes, FALSE, FALSE, (std::wstring(bGlobal ? L"Global\\" : L"Local\\") + DBWIN_BUFFER_READY).c_str())))
			return ERR("DBWIN_BUFFER_READY", GetLastError());

        if (!(DataReadyEvent = CreateEvent(&SecurityAttributes, FALSE, FALSE, (std::wstring(bGlobal ? L"Global\\" : L"Local\\") + DBWIN_DATA_READY).c_str())))
			return ERR("DBWIN_DATA_READY", GetLastError());

        DbgFormatObjectName(bGlobal ? FALSE : TRUE, DBWIN_BUFFER_SECTION_NAME, &objectName);
        InitializeObjectAttributes(
            &objectAttributes,
            &objectName,
            OBJ_CASE_INSENSITIVE,
            NULL,
            SecurityAttributes.lpSecurityDescriptor
            );

        if (!NT_SUCCESS(NtCreateSection(
            &DataBufferHandle,
            STANDARD_RIGHTS_REQUIRED | SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE,
            &objectAttributes,
            &maximumSize,
            PAGE_READWRITE,
            SEC_COMMIT,
            NULL
            )))
        {
			return ERR("NtCreateSection", GetLastError());
        }

        if (!NT_SUCCESS(NtMapViewOfSection(
            DataBufferHandle,
            NtCurrentProcess(),
            (PVOID*)&DebugBuffer,
            0,
            0,
            NULL,
            &viewSize,
            ViewShare,
            0,
            PAGE_READONLY
            )))
        {
			return ERR("NtMapViewOfSection", GetLastError());
        }

        CaptureEnabled = TRUE;

		return OK;
	}

	void UnInit(bool bGlobal)
	{
		BOOLEAN& CaptureEnabled = bGlobal ? GlobalCaptureEnabled : LocalCaptureEnabled;
		HANDLE& BufferReadyEvent = bGlobal ? GlobalBufferReadyEvent : LocalBufferReadyEvent;
		HANDLE& DataReadyEvent = bGlobal ? GlobalDataReadyEvent : LocalDataReadyEvent;
		HANDLE& DataBufferHandle = bGlobal ? GlobalDataBufferHandle : LocalDataBufferHandle;
		PDBWIN_PAGE_BUFFER& DebugBuffer = bGlobal ? GlobalDebugBuffer : LocalDebugBuffer;

        CaptureEnabled = FALSE;

        if (DebugBuffer)
        {
            NtUnmapViewOfSection(NtCurrentProcess(), DebugBuffer);
            DebugBuffer = NULL;
        }

        if (DataBufferHandle)
        {
            NtClose(DataBufferHandle);
            DataBufferHandle = NULL;
        }

        if (BufferReadyEvent)
        {
            NtClose(BufferReadyEvent);
            BufferReadyEvent = NULL;
        }

        if (DataReadyEvent)
        {
            NtClose(DataReadyEvent);
            DataReadyEvent = NULL;
        }
	}
};

CWinDbgMonitor::CWinDbgMonitor(QObject *parent)
	: QObject(parent)
{
	m = new SWinDbgMonitor;
}

CWinDbgMonitor::~CWinDbgMonitor()
{
	if (m->LocalCaptureEnabled)
		m->UnInit(false);
	if (m->GlobalCaptureEnabled)
		m->UnInit(true);
	//if (m->KernelCaptureEnabled) // todo: xxxx si
	//	KphSetDebugLog(FALSE);

	delete m;
}

static NTSTATUS DbgEventsThread(bool bGlobal, CWinDbgMonitor* This)
{
	HANDLE& BufferReadyEvent = bGlobal ? This->m->GlobalBufferReadyEvent : This->m->LocalBufferReadyEvent;
	HANDLE& DataReadyEvent = bGlobal ? This->m->GlobalDataReadyEvent : This->m->LocalDataReadyEvent;
	PDBWIN_PAGE_BUFFER debugMessageBuffer = bGlobal ? This->m->GlobalDebugBuffer : This->m->LocalDebugBuffer;

	while (TRUE)
	{
		NtSetEvent(BufferReadyEvent, NULL);
		
		LARGE_INTEGER timeout;
		NTSTATUS status = NtWaitForSingleObject(
			DataReadyEvent,
			FALSE,
			PhTimeoutFromMilliseconds(&timeout, 100)
			);

		if (status == STATUS_TIMEOUT)
			continue;
		if (status != STATUS_SUCCESS)
			break;

		// The process calling OutputDebugString is blocked here...
		
		emit This->DebugMessage(debugMessageBuffer->ProcessId, QString::fromLatin1(debugMessageBuffer->Buffer));
	}

	return STATUS_SUCCESS;
}

static NTSTATUS DbgEventsLocalThread(PVOID Parameter)
{
	return DbgEventsThread(false, (CWinDbgMonitor*)Parameter);
}

static NTSTATUS DbgEventsGlobalThread(PVOID Parameter)
{
	return DbgEventsThread(true, (CWinDbgMonitor*)Parameter);
}

static NTSTATUS DbgEventsKernelThread(PVOID Parameter)
{
	CWinDbgMonitor* This = (CWinDbgMonitor*)Parameter;
	
	/*ULONG SequenceNumber = 0; // todo: xxxx si

	while (TRUE)
	{
		SIZE_T Length = 0;
		char Buffer[8 * 1024] = { 0 };
		NTSTATUS status = KphReadDebugLog(&SequenceNumber, Buffer, ARRSIZE(Buffer), &Length);
		if (status == STATUS_NO_MORE_ENTRIES) {
			QThread::msleep(10);
			continue;
		}
		if (!NT_SUCCESS(status) && status != STATUS_BUFFER_TOO_SMALL) // Note: if the buffer was to small it still wil hold a truncated result
			break;

		emit This->DebugMessage((quint64)SYSTEM_PROCESS_ID, QString::fromLatin1(Buffer, Length));
	}*/

	return STATUS_SUCCESS;
}

STATUS CWinDbgMonitor::SetMonitor(EModes Mode)
{
	if ((Mode & eLocal) != 0)
	{
		if (!m->LocalCaptureEnabled)
		{
			STATUS Status = m->Init(false);
			if (Status.IsError())
				return Status;
			if (HANDLE threadHandle = PhCreateThread(0, (PUSER_THREAD_START_ROUTINE)DbgEventsLocalThread, this))
				NtClose(threadHandle);
		}
	}
	else if (m->LocalCaptureEnabled)
		m->UnInit(false);

	if ((Mode & eGlobal) != 0)
	{
		if (!m->GlobalCaptureEnabled)
		{
			STATUS Status = m->Init(true);
			if (Status.IsError())
				return Status;
			if (HANDLE threadHandle = PhCreateThread(0, (PUSER_THREAD_START_ROUTINE)DbgEventsGlobalThread, this))
				NtClose(threadHandle);
		}
	}
	else if(m->GlobalCaptureEnabled)
		m->UnInit(true);

	if ((Mode & eKernel) != 0)
	{
		/*if (!m->KernelCaptureEnabled) // todo: xxxx si
		{
			NTSTATUS status = KphSetDebugLog(TRUE);
			if (!NT_SUCCESS(status))
				return ERR("KphSetDebugLog", status);
			m->KernelCaptureEnabled = TRUE;
			if (HANDLE threadHandle = PhCreateThread(0, (PUSER_THREAD_START_ROUTINE)DbgEventsKernelThread, this))
				NtClose(threadHandle);
		}*/
	}
	else if(m->KernelCaptureEnabled)
	{
		//KphSetDebugLog(FALSE); // todo: xxxx si
		m->KernelCaptureEnabled = FALSE;
	}

	return OK;
}

CWinDbgMonitor::EModes CWinDbgMonitor::GetMode() const
{
	int Mode = eNone;
	if (m->LocalCaptureEnabled)
		Mode |= eLocal;
	if (m->GlobalCaptureEnabled)
		Mode |= eGlobal;
	if (m->KernelCaptureEnabled)
		Mode |= eKernel;
	return (CWinDbgMonitor::EModes)Mode;
}
