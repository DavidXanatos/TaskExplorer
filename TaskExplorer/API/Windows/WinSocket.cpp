/*
 * Process Hacker -
 *   qt wrapper and support functions
 *
 * Copyright (C) 2011 wj32
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
#include "WinProcess.h"
#include "ProcessHacker.h"
#include <lsasup.h>

#include "WinSocket.h"

#include <QtWin>

//exttools.h
typedef enum _ET_FIREWALL_STATUS
{
    FirewallUnknownStatus,
    FirewallAllowedNotRestricted,
    FirewallAllowedRestricted,
    FirewallNotAllowedNotRestricted,
    FirewallNotAllowedRestricted,
    FirewallMaximumStatus
} ET_FIREWALL_STATUS;

//treeext.c
ET_FIREWALL_STATUS EtQueryFirewallStatus(wstring FileName, WCHAR* LocalAddress, uint16_t LocalPort, bool bIPv6 = false, bool bUDP = false, bool bTCP = false);


struct SWinSocket
{
	SWinSocket() : FwStatus (FirewallUnknownStatus) {}

	ULONGLONG OwnerInfo[PH_NETWORK_OWNER_INFO_SIZE];
    ulong LocalScopeId; // Ipv6
    ulong RemoteScopeId; // Ipv6

	ET_FIREWALL_STATUS FwStatus;
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
	QHostAddress address;
	if (addr->Type == PH_IPV4_NETWORK_TYPE /*&& addr->Ipv4 != 0*/)
	{
		address = QHostAddress(ntohl(addr->InAddr.S_un.S_addr));
	}
	else if (addr->Type == PH_IPV6_NETWORK_TYPE /*&& !PhIsNullIpAddress(addr)*/)
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

	if (m_ProcessId == 0)
	{
		m_ProcessName = tr("Waiting connections");
	}
	else
	{
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
	}

	bool bIPv6 = false;
	WCHAR RemoteAddressString[INET6_ADDRSTRLEN] = { 0 };
	if (connection->LocalEndpoint.Address.Type == PH_IPV4_NETWORK_TYPE /*&& connection->LocalEndpoint.Address.Ipv4 != 0*/)
	{
		RtlIpv4AddressToString(&connection->LocalEndpoint.Address.InAddr, RemoteAddressString);
	}
	else if (connection->LocalEndpoint.Address.Type == PH_IPV6_NETWORK_TYPE /*&& !PhIsNullIpAddress(&connection->LocalEndpoint.Address)*/)
	{
		bIPv6 = true;
		RtlIpv6AddressToString(&connection->LocalEndpoint.Address.In6Addr, RemoteAddressString);
	}

	if (RemoteAddressString[0] != 0)
	{
		m->FwStatus = EtQueryFirewallStatus(m_ProcessName.toStdWString(), RemoteAddressString, connection->LocalEndpoint.Port, 
			bIPv6, (connection->ProtocolType & PH_UDP_PROTOCOL_TYPE) != 0, (connection->ProtocolType & PH_TCP_PROTOCOL_TYPE) != 0);
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

QString CWinSocket::GetFirewallStatus()
{ 
	QReadLocker Locker(&m_Mutex); 

	switch (m->FwStatus)
	{
		case FirewallAllowedNotRestricted:		return tr("Allowed, not restricted");
		case FirewallAllowedRestricted:			return tr("Allowed, restricted");
		case FirewallNotAllowedNotRestricted:	return tr("Not allowed, not restricted");
		case FirewallNotAllowedRestricted:		return tr("Not allowed, restricted");
		case FirewallUnknownStatus:				
		default:								return tr("");
	}
}


//treeext.c
#include <netfw.h>
static GUID IID_INetFwMgr_I = { 0xf7898af5, 0xcac4, 0x4632, { 0xa2, 0xec, 0xda, 0x06, 0xe5, 0x11, 0x1a, 0xf2 } };
static GUID CLSID_NetFwMgr_I = { 0x304ce942, 0x6e39, 0x40d8, { 0x94, 0x3a, 0xb9, 0x13, 0xc4, 0x0c, 0x9c, 0xd4 } };

ET_FIREWALL_STATUS EtQueryFirewallStatus(wstring FileName, WCHAR* LocalAddress, uint16_t LocalPort, bool bIPv6, bool bUDP, bool bTCP)
{
    static INetFwMgr* manager = NULL;
    ET_FIREWALL_STATUS result;
    BSTR imageFileNameBStr;
    BSTR localAddressBStr;
    VARIANT allowed;
    VARIANT restricted;

    if (!manager)
    {
        if (!SUCCEEDED(CoCreateInstance(CLSID_NetFwMgr_I, NULL, CLSCTX_INPROC_SERVER, IID_INetFwMgr_I, (LPVOID*)&manager)))
            return FirewallUnknownStatus;

        if (!manager)
            return FirewallUnknownStatus;
    }

    result = FirewallUnknownStatus;

    if (imageFileNameBStr = SysAllocStringLen(FileName.c_str(), FileName.size()))
    {
        localAddressBStr = SysAllocString(LocalAddress);

        memset(&allowed, 0, sizeof(VARIANT)); // VariantInit
        memset(&restricted, 0, sizeof(VARIANT)); // VariantInit

        if (SUCCEEDED(INetFwMgr_IsPortAllowed(
            manager,
            imageFileNameBStr,
            bIPv6 ? NET_FW_IP_VERSION_V6 : NET_FW_IP_VERSION_V4,
            LocalPort,
            localAddressBStr,
            bUDP ? NET_FW_IP_PROTOCOL_UDP : bTCP ? NET_FW_IP_PROTOCOL_TCP : NET_FW_IP_PROTOCOL_ANY,
            &allowed,
            &restricted
            )))
        {
            if (allowed.boolVal)
            {
                if (restricted.boolVal)
                    result = FirewallAllowedRestricted;
                else
                    result = FirewallAllowedNotRestricted;
            }
            else
            {
                if (restricted.boolVal)
                    result = FirewallNotAllowedRestricted;
                else
                    result = FirewallNotAllowedNotRestricted;
            }
        }

        if (localAddressBStr)
            SysFreeString(localAddressBStr);

        SysFreeString(imageFileNameBStr);
    }

    return result;
}