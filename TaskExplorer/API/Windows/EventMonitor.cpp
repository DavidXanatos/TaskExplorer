/*
 * Process Hacker -
 *   qt port of etwmon.c
 *
 * Copyright (C) 2010-2015 wj32
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
#include "EventMonitor.h"
#include "WindowsAPI.h"
#include "ProcessHacker.h"


#include "../TaskExplorer/GUI/TaskExplorer.h"

#include <evntcons.h>

typedef struct
{
    ULONG DiskNumber;
    ULONG IrpFlags;
    ULONG TransferSize;
    ULONG ResponseTime;
    ULONG64 ByteOffset;
    ULONG_PTR FileObject;
    ULONG_PTR Irp;
    ULONG64 HighResResponseTime;
    ULONG IssuingThreadId; // since WIN8 (ETW_DISKIO_READWRITE_V3)
} DiskIo_TypeGroup1;

typedef struct
{
    ULONG_PTR FileObject;
    WCHAR FileName[1];
} FileIo_Name;

typedef struct
{
    ULONGLONG FileObject;
    WCHAR FileName[1];
} FileIo_Name_Wow64;

typedef struct
{
    ULONG PID;
    ULONG size;
    ULONG daddr;
    ULONG saddr;
    USHORT dport;
    USHORT sport;
} TcpIpOrUdpIp_IPV4_Header;

typedef struct
{
    ULONG PID;
    ULONG size;
    IN6_ADDR daddr;
    IN6_ADDR saddr;
    USHORT dport;
    USHORT sport;
} TcpIpOrUdpIp_IPV6_Header;

// etwstat

static GUID ProcessHackerGuid = { 0x1288c53b, 0xaf35, 0x481b, { 0xb6, 0xb5, 0xa0, 0x5c, 0x39, 0x87, 0x2e, 0xd } };
static GUID SystemTraceControlGuid_I = { 0x9e814aad, 0x3204, 0x11d2, { 0x9a, 0x82, 0x00, 0x60, 0x08, 0xa8, 0x69, 0x39 } };
static GUID KernelRundownGuid_I = { 0x3b9c9951, 0x3480, 0x4220, { 0x93, 0x77, 0x9c, 0x8e, 0x51, 0x84, 0xf5, 0xcd } };

static GUID DiskIoGuid_I = { 0x3d6fa8d4, 0xfe05, 0x11d0, { 0x9d, 0xda, 0x00, 0xc0, 0x4f, 0xd7, 0xba, 0x7c } };
static GUID FileIoGuid_I = { 0x90cbdc39, 0x4a3e, 0x11d1, { 0x84, 0xf4, 0x00, 0x00, 0xf8, 0x04, 0x64, 0xe3 } };
static GUID TcpIpGuid_I = { 0x9a280ac0, 0xc8e0, 0x11d1, { 0x84, 0xe2, 0x00, 0xc0, 0x4f, 0xb9, 0x98, 0xa2 } };
static GUID UdpIpGuid_I = { 0xbf3a50c5, 0xa9c9, 0x4988, { 0xa0, 0x05, 0x2d, 0xf0, 0xb7, 0xc8, 0x0f, 0x80 } };

// ETW tracing layer

static UNICODE_STRING EtpSharedKernelLoggerName = RTL_CONSTANT_STRING(KERNEL_LOGGER_NAME);
static UNICODE_STRING EtpPrivateKernelLoggerName = RTL_CONSTANT_STRING(L"PhEtKernelLogger");
// rundown
static UNICODE_STRING EtpRundownLoggerName = RTL_CONSTANT_STRING(L"PhEtRundownLogger");


struct SEventMonitor
{
	SEventMonitor()
	{
		SessionHandle = NULL;
		ActualKernelLoggerName = NULL;
		ActualSessionGuid = NULL;
		TraceProperties = NULL;
		IsStartedSession = FALSE;
	}

	TRACEHANDLE SessionHandle;
	PUNICODE_STRING ActualKernelLoggerName;
	PGUID ActualSessionGuid;
	PEVENT_TRACE_PROPERTIES TraceProperties; // ToDo: free this?
	BOOLEAN IsStartedSession;
};


CEventMonitor::CEventMonitor(QObject *parent) : QThread(parent)
{
	m_bRunning = false;

	m_bRundownMode = false;
	m_pMonitorRundown = NULL;
	
	m = new SEventMonitor();
}

bool CEventMonitor::Init()
{
	if (!StartSession())
		return false;

	// start thread
	m_bRunning = true;
	start();

	// Note: this gives us a rundown of know file names
	if (!m_bRundownMode)
	{
		m_pMonitorRundown = new CEventMonitor(this);
		m_pMonitorRundown->m_bRundownMode = true;

		connect(m_pMonitorRundown, SIGNAL(FileEvent(int, quint64, quint64, quint64, const QString&)), this, SIGNAL(FileEvent(int, quint64, quint64, quint64, const QString&)));

		m_pMonitorRundown->Init();
	}

	return true;
}

CEventMonitor::~CEventMonitor()
{
	//quit();

	m_bRunning = false;

	StopSession();

    //if (m->IsRundownActive)
    //    StopRundownSession();

	if (!wait(5 * 1000))
		terminate();

	delete m;
}

bool CEventMonitor::StartSession()
{
	ULONG result;
	ULONG bufferSize;

	if (m_bRundownMode)
	{
		m->ActualKernelLoggerName = &EtpRundownLoggerName;
		m->ActualSessionGuid = &KernelRundownGuid_I;
	}
	else if (WindowsVersion >= WINDOWS_8)
	{
		m->ActualKernelLoggerName = &EtpPrivateKernelLoggerName;
		m->ActualSessionGuid = &ProcessHackerGuid;
	}
	else
	{
		m->ActualKernelLoggerName = &EtpSharedKernelLoggerName;
		m->ActualSessionGuid = &SystemTraceControlGuid_I;
	}

	bufferSize = sizeof(EVENT_TRACE_PROPERTIES) + m->ActualKernelLoggerName->Length + sizeof(WCHAR);

	if (!m->TraceProperties)
		m->TraceProperties = (PEVENT_TRACE_PROPERTIES)PhAllocate(bufferSize);

	memset(m->TraceProperties, 0, sizeof(EVENT_TRACE_PROPERTIES));

	m->TraceProperties->Wnode.BufferSize = bufferSize;
	//m->TraceProperties->Wnode.Guid = *m->ActualSessionGuid;
	m->TraceProperties->Wnode.ClientContext = 1;
	m->TraceProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
	m->TraceProperties->MinimumBuffers = 1;
	m->TraceProperties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;
	m->TraceProperties->FlushTimer = 1;
	//m->TraceProperties->EnableFlags = EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_DISK_FILE_IO | EVENT_TRACE_FLAG_NETWORK_TCPIP;
	m->TraceProperties->LogFileNameOffset = 0;
	m->TraceProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

	if (!m_bRundownMode)
	{
		m->TraceProperties->Wnode.Guid = *m->ActualSessionGuid;
		m->TraceProperties->EnableFlags = EVENT_TRACE_FLAG_DISK_IO | EVENT_TRACE_FLAG_DISK_FILE_IO | EVENT_TRACE_FLAG_NETWORK_TCPIP;

		if (WindowsVersion >= WINDOWS_8)
			m->TraceProperties->LogFileMode |= EVENT_TRACE_SYSTEM_LOGGER_MODE;
	}

	m->IsStartedSession = FALSE;
	result = StartTrace(&m->SessionHandle, m->ActualKernelLoggerName->Buffer, m->TraceProperties);
	if (result == ERROR_SUCCESS)
		m->IsStartedSession = TRUE;

	if (result == ERROR_ALREADY_EXISTS)
	{
		if (m_bRundownMode) // Note: in RunDown mode we force a reset
		{
			StopSession();
			// ControlTrace (called from EtpStopEtwSession) screws up the structure.
			m->TraceProperties->Wnode.BufferSize = bufferSize;
			m->TraceProperties->LogFileNameOffset = 0;
			m->TraceProperties->LoggerNameOffset = sizeof(EVENT_TRACE_PROPERTIES);

			result = StartTrace(&m->SessionHandle, m->ActualKernelLoggerName->Buffer, m->TraceProperties);
			if (result == ERROR_SUCCESS)
				m->IsStartedSession = TRUE;
		}
		else
		{
			// The session already exists.
			//result = ControlTrace(0, m->ActualKernelLoggerName->Buffer, m->TraceProperties, EVENT_TRACE_CONTROL_UPDATE);

			result = ERROR_SUCCESS;
		}
	}

	if (result == ERROR_SUCCESS)
	{
		if(m_bRundownMode)
			result = EnableTraceEx(m->ActualSessionGuid, NULL, m->SessionHandle, 1, 0, 0x10, 0, 0, NULL);
	}
	
	if (result != ERROR_SUCCESS)
	{
		StopSession();
		return false;
	}

	return true;
}

ulong CEventMonitor::ControlSession(ulong ControlCode)
{
    // If we have a session handle, we use that instead of the logger name.

    m->TraceProperties->LogFileNameOffset = 0; // make sure it is 0, otherwise ControlTrace crashes

    return ControlTrace(m->IsStartedSession ? m->SessionHandle : 0, m->IsStartedSession ? NULL : m->ActualKernelLoggerName->Buffer, m->TraceProperties, ControlCode);
}

void CEventMonitor::StopSession()
{
    ControlSession(EVENT_TRACE_CONTROL_STOP);
}

void CEventMonitor::FlushSession()
{
    ControlSession(EVENT_TRACE_CONTROL_FLUSH);
}

ULONG NTAPI EtpEtwBufferCallback(_In_ PEVENT_TRACE_LOGFILE Buffer)
{
	return ((CEventMonitor*)Buffer->Context)->IsRunning();
}

VOID NTAPI EtpEtwEventCallback(_In_ PEVENT_RECORD EventRecord)
{
	CEventMonitor* This = ((CEventMonitor*)EventRecord->UserContext);

    if (IsEqualGUID(EventRecord->EventHeader.ProviderId, DiskIoGuid_I))
    {
        // DiskIo

        ET_ETW_EVENT_TYPE Type = EtEtwUnknow;

        switch (EventRecord->EventHeader.EventDescriptor.Opcode)
        {
        case EVENT_TRACE_TYPE_IO_READ:
            Type = EtEtwDiskReadType;
            break;
        case EVENT_TRACE_TYPE_IO_WRITE:
            Type = EtEtwDiskWriteType;
            break;
        default:
            break;
        }

        if (Type != EtEtwUnknow)
        {
            DiskIo_TypeGroup1 *data = (DiskIo_TypeGroup1*)EventRecord->UserData;

			quint64 ProcessId = -1;
			quint64 ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for disk events. 
            if (WindowsVersion >= WINDOWS_8)
            {
                ThreadId = data->IssuingThreadId;
                //ProcessId = EtThreadIdToProcessId(diskEvent.ClientId.UniqueThread);
				ProcessId = -1; // indicate that it must be looked up by thread id
            }
            else
            {
                if (EventRecord->EventHeader.ProcessId != ULONG_MAX)
                {
                    ProcessId = EventRecord->EventHeader.ProcessId;
                    ThreadId = EventRecord->EventHeader.ThreadId;
                }
            }

            //EtDiskProcessDiskEvent(&diskEvent);
			emit This->DiskEvent(Type, data->FileObject, ProcessId, ThreadId, data->IrpFlags, data->TransferSize, data->HighResResponseTime);
        }
    }
    else if (IsEqualGUID(EventRecord->EventHeader.ProviderId, FileIoGuid_I))
    {
        // FileIo

        ET_ETW_EVENT_TYPE Type = EtEtwUnknow;

        switch (EventRecord->EventHeader.EventDescriptor.Opcode)
        {
        case 0: // Name
            Type = EtEtwFileNameType;
            break;
        case 32: // FileCreate
            Type = EtEtwFileCreateType;
            break;
        case 35: // FileDelete
            Type = EtEtwFileDeleteType;
            break;
        default:
            break;
        }

        if (Type != EtEtwUnknow)
        {
			quint64 FileId;
			QString FileName;

            if (PhIsExecutingInWow64())
            {
                FileIo_Name_Wow64 *dataWow64 = (FileIo_Name_Wow64*)EventRecord->UserData;

                FileId = dataWow64->FileObject;
				FileName = QString::fromWCharArray(dataWow64->FileName);
            }
            else
            {
                FileIo_Name *data = (FileIo_Name*)EventRecord->UserData;

                FileId = data->FileObject;
				FileName = QString::fromWCharArray(data->FileName);
            }

			quint64 ProcessId = -1;
			quint64 ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for file events. 
            if (WindowsVersion >= WINDOWS_8)
            {
                ThreadId = -1;
				ProcessId = -1;
            }
            else
            {
                if (EventRecord->EventHeader.ProcessId != ULONG_MAX)
                {
                    ProcessId = EventRecord->EventHeader.ProcessId;
                    ThreadId = EventRecord->EventHeader.ThreadId;
                }
            }

            //EtDiskProcessFileEvent(&fileEvent);
			emit This->FileEvent(Type, FileId, ThreadId, ThreadId, FileName);
        }
    }
    else if (
        IsEqualGUID(EventRecord->EventHeader.ProviderId, TcpIpGuid_I) ||
        IsEqualGUID(EventRecord->EventHeader.ProviderId, UdpIpGuid_I)
        )
    {
        // TcpIp/UdpIp

		ET_ETW_EVENT_TYPE Type = EtEtwUnknow;
		ulong ProtocolType = 0;

        switch (EventRecord->EventHeader.EventDescriptor.Opcode)
        {
        case EVENT_TRACE_TYPE_SEND: // send
            Type = EtEtwNetworkSendType;
            ProtocolType = PH_IPV4_NETWORK_TYPE;
            break;
        case EVENT_TRACE_TYPE_RECEIVE: // receive
            Type = EtEtwNetworkReceiveType;
            ProtocolType = PH_IPV4_NETWORK_TYPE;
            break;
        case EVENT_TRACE_TYPE_SEND + 16: // send ipv6
            Type = EtEtwNetworkSendType;
            ProtocolType = PH_IPV6_NETWORK_TYPE;
            break;
        case EVENT_TRACE_TYPE_RECEIVE + 16: // receive ipv6
            Type = EtEtwNetworkReceiveType;
            ProtocolType = PH_IPV6_NETWORK_TYPE;
            break;
        }

        if (IsEqualGUID(EventRecord->EventHeader.ProviderId, TcpIpGuid_I))
            ProtocolType |= PH_TCP_PROTOCOL_TYPE;
        else
            ProtocolType |= PH_UDP_PROTOCOL_TYPE;

        if (Type != EtEtwUnknow)
        {
			quint64 ProcessId = -1;
			ulong TransferSize = 0;

			QHostAddress LocalAddress;
			quint16 LocalPort = 0;
			QHostAddress RemoteAddress;
			quint16 RemotePort = 0;

            if (ProtocolType & PH_IPV4_NETWORK_TYPE)
            {
                TcpIpOrUdpIp_IPV4_Header *data = (TcpIpOrUdpIp_IPV4_Header*)EventRecord->UserData;

                ProcessId = data->PID;
                TransferSize = data->size;

				LocalAddress = QHostAddress(ntohl(data->saddr));
                LocalPort = ntohs(data->sport);

				RemoteAddress = QHostAddress(ntohl(data->daddr));
				RemotePort = ntohs(data->dport);
            }
            else if (ProtocolType & PH_IPV6_NETWORK_TYPE)
            {
                TcpIpOrUdpIp_IPV6_Header *data = (TcpIpOrUdpIp_IPV6_Header*)EventRecord->UserData;

                ProcessId = data->PID;
                TransferSize = data->size;

				LocalAddress = QHostAddress(data->saddr.u.Byte);
                LocalPort = ntohs(data->sport);

				RemoteAddress = QHostAddress(data->daddr.u.Byte);
				RemotePort = ntohs(data->dport);
            }

			// Note: Incomming UDP packets have the endpoints swaped :/
			if ((ProtocolType & PH_UDP_PROTOCOL_TYPE) != 0 && Type == EtEtwNetworkReceiveType)
			{
				QHostAddress TempAddresss = LocalAddress;
				quint16 TempPort = LocalPort;
				LocalAddress = RemoteAddress;
				LocalPort = RemotePort;
				RemoteAddress = TempAddresss;
				RemotePort = TempPort;
			}

			//if(ProcessId == )
			//	qDebug() << ProcessId  << Type << LocalAddress.toString() << LocalPort << RemoteAddress.toString() << RemotePort;

			//EtProcessNetworkEvent(&networkEvent);
			emit This->NetworkEvent(Type, ProcessId, -1, ProtocolType, TransferSize, LocalAddress, LocalPort, RemoteAddress, RemotePort);
        }
    }
}

VOID NTAPI EtpRundownEtwEventCallback(_In_ PEVENT_RECORD EventRecord)
{
	CEventMonitor* This = ((CEventMonitor*)EventRecord->UserContext);

	// Note: this could easely be integrated in EtpEtwEventCallback (DX)

    // TODO: Find a way to call CloseTrace when the enumeration finishes so we can
    // stop the trace cleanly.

    if (IsEqualGUID(EventRecord->EventHeader.ProviderId, FileIoGuid_I))
    {
        // FileIo

        ET_ETW_EVENT_TYPE Type = EtEtwUnknow;

        switch (EventRecord->EventHeader.EventDescriptor.Opcode)
        {
        case 36: // FileRundown
            Type = EtEtwFileRundownType;
            break;
        default:
            break;
        }

        if (Type != EtEtwUnknow)
        {
			quint64 FileId;
			QString FileName;

            if (PhIsExecutingInWow64())
            {
                FileIo_Name_Wow64 *dataWow64 = (FileIo_Name_Wow64*)EventRecord->UserData;

                FileId = dataWow64->FileObject;
				FileName = QString::fromWCharArray(dataWow64->FileName);
            }
            else
            {
                FileIo_Name *data = (FileIo_Name*)EventRecord->UserData;

                FileId = data->FileObject;
				FileName = QString::fromWCharArray(data->FileName);
            }

			quint64 ProcessId = -1;
			quint64 ThreadId = -1;

			// Since Windows 8, we no longer get the correct process/thread IDs in the
			// event headers for file events. 
            if (WindowsVersion >= WINDOWS_8)
            {
				ThreadId = -1;
				ProcessId = -1;
            }
            else
            {
                if (EventRecord->EventHeader.ProcessId != ULONG_MAX)
                {
                    ProcessId = EventRecord->EventHeader.ProcessId;
                    ThreadId = EventRecord->EventHeader.ThreadId;
                }
            }

            //EtDiskProcessFileEvent(&fileEvent);
			emit This->FileEvent(Type, FileId, ProcessId, ThreadId, FileName);
        }
    }
}

void CEventMonitor::run()
{
	//exec();

	ULONG result;
    EVENT_TRACE_LOGFILE logFile;
    TRACEHANDLE traceHandle;

	/*if (!m_bRundownMode)
	{
		// Since Windows 8, we no longer get the correct process/thread IDs in the
		// event headers for disk events. We need to update our process information since
		// etwmon uses our EtThreadIdToProcessId function.
		if (WindowsVersion >= WINDOWS_8)
			EtUpdateProcessInformation();
	}*/

    memset(&logFile, 0, sizeof(EVENT_TRACE_LOGFILE));
    logFile.LoggerName = m->ActualKernelLoggerName->Buffer;
    logFile.ProcessTraceMode = PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD;
    logFile.BufferCallback = EtpEtwBufferCallback;
	logFile.EventRecordCallback = m_bRundownMode ? EtpRundownEtwEventCallback : EtpEtwEventCallback;
	logFile.Context = this;

    while (TRUE)
    {
        result = ERROR_SUCCESS;
        traceHandle = OpenTrace(&logFile);

        if (traceHandle != INVALID_PROCESSTRACE_HANDLE)
        {
            while (m_bRunning && (result = ProcessTrace(&traceHandle, 1, NULL, NULL)) == ERROR_SUCCESS)
                NOTHING;

			if (traceHandle != INVALID_PROCESSTRACE_HANDLE)
				CloseTrace(traceHandle);
        }

        if (!m_bRunning || m_bRundownMode)
            break;

        if (result == ERROR_WMI_INSTANCE_NOT_FOUND)
        {
            // The session was stopped by another program. Try to start it again.
            if(!StartSession())
				// Some error occurred, so sleep for a while before trying again.
				// Don't sleep if we just successfully started a session, though.
				QThread::msleep(250);
        }
    }
}