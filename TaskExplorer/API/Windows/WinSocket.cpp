/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2009-2016 wj32
 * Copyright (C) 2017-2019 dmex
 * Copyright (C) 2019-2020 David Xanatos
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
#include "WinProcess.h"
#include "ProcessHacker.h"
#include <lsasup.h>

#include "WinSOcket.h"

#include <QtWin>

struct SWinSocket
{
	ULONGLONG OwnerInfo[PH_NETWORK_OWNER_INFO_SIZE];
    ulong LocalScopeId; // Ipv6
    ulong RemoteScopeId; // Ipv6
};

CWinSocket::CWinSocket(QObject *parent) 
	: CSocketInfo(parent) 
{
	m_SubsystemProcess = false;

	m = new SWinSocket;
}

CWinSocket::~CWinSocket()
{
	delete m;
}

QHostAddress CWinSocket::PH2QAddress(struct _PH_IP_ADDRESS* addr)
{
	/*WCHAR RemoteAddressString[INET6_ADDRSTRLEN];
	if (addr->Type == PH_IPV4_NETWORK_TYPE && addr->Ipv4 != 0)
	{
		RtlIpv4AddressToString(&addr->InAddr, RemoteAddressString);
	}
	else if (addr->Type == PH_IPV6_NETWORK_TYPE && !PhIsNullIpAddress(addr))
	{
		RtlIpv6AddressToString(&addr->In6Addr, RemoteAddressString);
	}*/
	
	QHostAddress address;
	if (addr->Type == PH_IPV4_NETWORK_TYPE && addr->Ipv4 != 0)
	{
		address = QHostAddress(ntohl(addr->InAddr.S_un.S_addr));
	}
	else if (addr->Type == PH_IPV6_NETWORK_TYPE && !PhIsNullIpAddress(addr))
	{
		address = QHostAddress((quint8*)addr->Ipv6);
	}

	return address;
}

bool CWinSocket::InitStaticData(struct _PH_NETWORK_CONNECTION* connection)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProtocolType = connection->ProtocolType;
	m_LocalAddress = PH2QAddress(&connection->LocalEndpoint.Address);
	m_LocalPort = ntohs(connection->LocalEndpoint.Port);
	m_RemoteAddress = PH2QAddress(&connection->RemoteEndpoint.Address); 
	m_RemotePort = ntohs(connection->RemoteEndpoint.Port);
	m_State = connection->State;
	m_ProcessId = (quint64)connection->ProcessId;
	m_CreateTime = QDateTime::fromTime_t((int64_t)connection->CreateTime.QuadPart / 10000000ULL - 11644473600ULL);

	// generate a somewhat unique id to optimize map search
	m_HashID = ((quint64)connection->LocalEndpoint.Port << 0) | ((quint64)connection->RemoteEndpoint.Port << 16) | (quint64)(((quint32*)&m_ProcessId)[0] ^ ((quint32*)&m_ProcessId)[1]) << 32;

	memcpy(m->OwnerInfo, connection->OwnerInfo, sizeof(ULONGLONG) * PH_NETWORK_OWNER_INFO_SIZE);
    PVOID serviceTag = UlongToPtr(*(PULONG)m->OwnerInfo);
    PPH_STRING serviceName = PhGetServiceNameFromTag(connection->ProcessId, serviceTag);
	m_OwnerService = CastPhString(serviceName);

	m->LocalScopeId = connection->LocalScopeId;
	m->RemoteScopeId = connection->RemoteScopeId;

	//
	// ToDo: Resolve host names
	//

	m_pProcess = theAPI->GetProcessByID(m_ProcessId);
	if (CWinProcess* pWinProc = qobject_cast<CWinProcess*>(m_pProcess.data()))
	{
		m_ProcessName = pWinProc->GetName();
		m_SubsystemProcess = pWinProc->IsSubsystemProcess();
	}
	else
	{
        HANDLE processHandle;
        PPH_STRING fileName;
        PROCESS_EXTENDED_BASIC_INFORMATION basicInfo;

        // HACK HACK HACK
        // WSL subsystem processes (e.g. apache/nginx) create sockets, clone/fork themselves, duplicate the socket into the child process and then terminate.
        // The socket handle remains valid and in-use by the child process BUT the socket continues returning the PID of the exited process???
        // Fixing this causes a major performance problem; If we have 100,000 sockets then on previous versions of Windows we would only need 2 system calls maximum
        // (for the process list) to identify the owner of every socket but now we need to make 4 system calls for every_last_socket totaling 400,000 system calls... great. (dmex)
        if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, connection->ProcessId)))
        {
            if (NT_SUCCESS(PhGetProcessExtendedBasicInformation(processHandle, &basicInfo)))
            {
                m_SubsystemProcess = !!basicInfo.IsSubsystemProcess;
            }

            if (NT_SUCCESS(PhGetProcessImageFileName(processHandle, &fileName)))
            {
				m_ProcessName = CastPhString(fileName);
            }

            NtClose(processHandle);
        }
	}

	return true;
}

bool CWinSocket::UpdateDynamicData(struct _PH_NETWORK_CONNECTION* connection)
{
	QWriteLocker Locker(&m_Mutex);

	return true;
}

bool CWinSocket::Match(struct _PH_NETWORK_CONNECTION* connection)
{
	return (m_ProtocolType == connection->ProtocolType &&
			m_LocalAddress == PH2QAddress(&connection->LocalEndpoint.Address) &&
			m_LocalPort == ntohs(connection->LocalEndpoint.Port) &&
			m_RemoteAddress == PH2QAddress(&connection->RemoteEndpoint.Address) &&
			m_RemotePort == ntohs(connection->RemoteEndpoint.Port) &&
			m_ProcessId == (quint64)connection->ProcessId);
}