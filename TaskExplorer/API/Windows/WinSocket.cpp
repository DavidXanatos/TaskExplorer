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
#include "WindowsAPI.h"
#include "EventMonitor.h"

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
	if (addr->Type == PH_IPV4_NETWORK_TYPE)
	{
		if (addr->Ipv4 != 0)
			address = QHostAddress(ntohl(addr->InAddr.S_un.S_addr));
		else
			address = QHostAddress::Any;
	}
	else if (addr->Type == PH_IPV6_NETWORK_TYPE)
	{
		if(!PhIsNullIpAddress(addr))
			address = QHostAddress((quint8*)addr->Ipv6);
		else
			address = QHostAddress::Any;
	}

	return address;
}

bool CWinSocket::InitStaticData(quint64 ProcessId, ulong ProtocolType,
	const QHostAddress& LocalAddress, quint16 LocalPort, const QHostAddress& RemoteAddress, quint16 RemotePort)
{
	QWriteLocker Locker(&m_Mutex);

	m_ProtocolType = ProtocolType;
	m_LocalAddress = LocalAddress;
	m_LocalPort = LocalPort;
	if ((m_ProtocolType & PH_TCP_PROTOCOL_TYPE) != 0)
	{
		m_RemoteAddress = RemoteAddress;
		m_RemotePort = RemotePort;
	}
	m_ProcessId = ProcessId;

	// generate a somewhat unique id to optimize map search
	m_HashID = CSocketInfo::MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	if (m_ProcessId == 0)
	{
		m_ProcessName = tr("Waiting connections");
	}
	else
	{
		CProcessPtr pProcess = theAPI->GetProcessByID(m_ProcessId);
		m_pProcess = pProcess; // relember m_pProcess is a week pointer

		if (CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data()))
		{
			m_ProcessName = pWinProc->GetName();
			m_SubsystemProcess = pWinProc->IsSubsystemProcess();

			// PhpUpdateNetworkItemOwner(networkItem, processItem);
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
			if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)ProcessId)))
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

	//
	// ToDo: Resolve host names
	//

	// EtpUpdateFirewallStatus
	WCHAR RemoteAddressString[INET6_ADDRSTRLEN + 1] = { 0 };
	QString LocalAddressStr = LocalAddress.toString();
	if (LocalAddressStr.length() < INET6_ADDRSTRLEN);
		LocalAddressStr.toWCharArray(RemoteAddressString);
	if (RemoteAddressString[0] != 0)
	{
		m->FwStatus = EtQueryFirewallStatus(m_ProcessName.toStdWString(), RemoteAddressString, htons(LocalPort), 
			LocalAddress.protocol() == QAbstractSocket::IPv6Protocol, (ProtocolType & PH_UDP_PROTOCOL_TYPE) != 0, (ProtocolType & PH_TCP_PROTOCOL_TYPE) != 0);
	}
	//

	return true;
}

bool CWinSocket::InitStaticDataEx(struct _PH_NETWORK_CONNECTION* connection)
{
	QWriteLocker Locker(&m_Mutex);

	m_CreateTimeStamp = FILETIME2ms(connection->CreateTime.QuadPart);

	ULONGLONG OwnerInfo[PH_NETWORK_OWNER_INFO_SIZE];
	memcpy(OwnerInfo, connection->OwnerInfo, sizeof(ULONGLONG) * PH_NETWORK_OWNER_INFO_SIZE);

	// PhpUpdateNetworkItemOwner
    PVOID serviceTag = UlongToPtr(*(PULONG)OwnerInfo);
    PPH_STRING serviceName = PhGetServiceNameFromTag(connection->ProcessId, serviceTag);
	m_OwnerService = CastPhString(serviceName);
	//

	m->LocalScopeId = connection->LocalScopeId;
	m->RemoteScopeId = connection->RemoteScopeId;

	return true;
}

bool CWinSocket::UpdateDynamicData(struct _PH_NETWORK_CONNECTION* connection)
{
	QWriteLocker Locker(&m_Mutex);

    BOOLEAN modified = FALSE;

    //if (InterlockedExchange(&networkItem->JustResolved, 0) != 0)
    //    modified = TRUE;

    if (m_State != connection->State)
    {
        m_State = connection->State;
        modified = TRUE;
    }

	if (m_pProcess.isNull())
	{
		CProcessPtr pProcess = theAPI->GetProcessByID(m_ProcessId);
		m_pProcess = pProcess; // relember m_pProcess is a week pointer

		if (!pProcess.isNull())
		{
			if (m_ProcessName.isEmpty())
			{
				m_ProcessName = pProcess->GetName();
				//m_SubsystemProcess = pWinProc->IsSubsystemProcess();
				//PhpUpdateNetworkItemOwner(networkItem, networkItem->ProcessItem);
				modified = TRUE;
			}

			/*if (!networkItem->ProcessIconValid && PhTestEvent(&networkItem->ProcessItem->Stage1Event))
			{
				networkItem->ProcessIcon = networkItem->ProcessItem->SmallIcon;
				networkItem->ProcessIconValid = TRUE;
				modified = TRUE;
			}*/
		}
	}

	UpdateStats();

	return modified;
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
    BSTR ModuleFileNameBStr;
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

    if (ModuleFileNameBStr = SysAllocStringLen(FileName.c_str(), FileName.size()))
    {
        localAddressBStr = SysAllocString(LocalAddress);

        memset(&allowed, 0, sizeof(VARIANT)); // VariantInit
        memset(&restricted, 0, sizeof(VARIANT)); // VariantInit

        if (SUCCEEDED(INetFwMgr_IsPortAllowed(
            manager,
            ModuleFileNameBStr,
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

        SysFreeString(ModuleFileNameBStr);
    }

    return result;
}

void CWinSocket::AddNetworkIO(int Type, ulong TransferSize)
{
	QWriteLocker Locker(&m_StatsMutex);

	switch (Type)
	{
	case EtEtwNetworkReceiveType:	m_Stats.Net.AddReceive(TransferSize); break;
	case EtEtwNetworkSendType:		m_Stats.Net.AddSend(TransferSize); break;
	}
}

STATUS CWinSocket::Close()
{
	if (m_ProtocolType != PH_TCP4_NETWORK_PROTOCOL || m_State != MIB_TCP_STATE_ESTAB)
		return ERR(tr("Not supported type or state"));

	MIB_TCPROW tcpRow;
	tcpRow.dwState = MIB_TCP_STATE_DELETE_TCB;
	tcpRow.dwLocalAddr = htonl(m_LocalAddress.toIPv4Address());
	tcpRow.dwLocalPort = htons(m_LocalPort);
	tcpRow.dwRemoteAddr = htonl(m_RemoteAddress.toIPv4Address());
	tcpRow.dwRemotePort = htons(m_RemotePort);

	// ToDo: make it work with IPv6
	// Note: there is (as of 2019) no SetTcp6Entry that would accept MIB_TCP6ROW :/
	// may be do int the way close handle in Process Explorer works ???
	/*
	MIB_TCP6ROW tcp6Row;
	tcp6Row.State = MIB_TCP_STATE_DELETE_TCB;
	Q_IPV6ADDR LocalAddr = m_LocalAddress.toIPv6Address();
	memcpy(&tcp6Row.LocalAddr, &LocalAddr, 16);
	tcp6Row.dwLocalPort = htons(m_LocalPort);
	Q_IPV6ADDR RemoteAddr = m_RemoteAddress.toIPv6Address();
	memcpy(&tcp6Row.RemoteAddr, &RemoteAddr, 16);
	tcp6Row.dwRemotePort = htons(m_RemotePort);
	tcp6Row.dwLocalScopeId = m->LocalScopeId;
	tcp6Row.dwRemoteScopeId = m->RemoteScopeId;
	*/

	ULONG result = SetTcpEntry(&tcpRow);
	if (result == NO_ERROR)
		return OK;

	
	// SetTcpEntry returns ERROR_MR_MID_NOT_FOUND for access denied errors for some reason.
    if (result == ERROR_MR_MID_NOT_FOUND)
		result = ERROR_ACCESS_DENIED;
	
	// todo run itself as service and retry

	return ERR(result);
}