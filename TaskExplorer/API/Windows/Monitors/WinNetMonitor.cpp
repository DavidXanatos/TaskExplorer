/*
 * Process Hacker Plugins -
 *   qt port of Hardware Devices Plugin
 *
 * Copyright (C) 2016 wj32
 * Copyright (C) 2015-2018 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * Process Hacker is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Process Hacker is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"
#include "WinNetMonitor.h"
#include "../ProcessHacker.h"
#include "../ProcessHacker/devices.h"
#include <cfgmgr32.h>
#include <ndisguid.h>
#include <ras.h>
#include <raserror.h>

struct SWinNetMonitor
{
	SWinNetMonitor()
	{
		UpdateAddressesPending = false;
		eventHandle = NULL;
	}

	volatile bool UpdateAddressesPending;
	HANDLE eventHandle;
};

CWinNetMonitor::CWinNetMonitor(QObject *parent)
	: CNetMonitor(parent)
{
	m = new SWinNetMonitor;
}

CWinNetMonitor::~CWinNetMonitor()
{
	StopWatch();
	delete m;
}

bool CWinNetMonitor::Init()
{
	if (!UpdateAdapters())
		return false;

	StartWatch();

	return true;
}

BOOLEAN QueryNetworkDeviceInterfaceDescription(
    _In_ PWSTR DeviceInterface,
    _Out_ DEVINST *DeviceInstanceHandle,
    _Out_ PPH_STRING *DeviceDescription
    )
{
    CONFIGRET result;
    ULONG bufferSize;
    PPH_STRING deviceDescription;
    DEVPROPTYPE devicePropertyType;
    DEVINST deviceInstanceHandle;
    ULONG deviceInstanceIdLength = MAX_DEVICE_ID_LEN;
    WCHAR deviceInstanceId[MAX_DEVICE_ID_LEN + 1] = L"";

    if (CM_Get_Device_Interface_Property(
        DeviceInterface,
        &DEVPKEY_Device_InstanceId,
        &devicePropertyType,
        (PBYTE)deviceInstanceId,
        &deviceInstanceIdLength,
        0
        ) != CR_SUCCESS)
    {
        return FALSE;
    }

    if (CM_Locate_DevNode(
        &deviceInstanceHandle,
        deviceInstanceId,
        CM_LOCATE_DEVNODE_PHANTOM
        ) != CR_SUCCESS)
    {
        return FALSE;
    }

    bufferSize = 0x40;
    deviceDescription = PhCreateStringEx(NULL, bufferSize);

    // DEVPKEY_Device_DeviceDesc doesn't give us the full adapter name.
    // DEVPKEY_Device_FriendlyName does give us the full adapter name but is only
    //  supported on Windows 8 and above.

    // We use our NetworkAdapterQueryName function to query the full adapter name
    // from the NDIS driver directly, if that fails then we use one of the above properties.

    if ((result = CM_Get_DevNode_Property(
        deviceInstanceHandle,
        WindowsVersion >= WINDOWS_8 ? &DEVPKEY_Device_FriendlyName : &DEVPKEY_Device_DeviceDesc,
        &devicePropertyType,
        (PBYTE)deviceDescription->Buffer,
        &bufferSize,
        0
        )) != CR_SUCCESS)
    {
        PhDereferenceObject(deviceDescription);
        deviceDescription = PhCreateStringEx(NULL, bufferSize);

        result = CM_Get_DevNode_Property(
            deviceInstanceHandle,
            WindowsVersion >= WINDOWS_8 ? &DEVPKEY_Device_FriendlyName : &DEVPKEY_Device_DeviceDesc,
            &devicePropertyType,
            (PBYTE)deviceDescription->Buffer,
            &bufferSize,
            0
            );
    }

    if (result != CR_SUCCESS)
    {
        PhDereferenceObject(deviceDescription);
        return FALSE;
    }

    PhTrimToNullTerminatorString(deviceDescription);

    *DeviceInstanceHandle = deviceInstanceHandle;
    *DeviceDescription = deviceDescription;

    return TRUE;
}

bool CWinNetMonitor::UpdateAdapters()
{
	QSet<QString> OldNics = m_NicList.keys().toSet();

	/*if (1)
    {
		// Note: this method lists also all virtual filter adapters, hence is not well suited to be used to summ up all traffic as than it would count it multiple times.

        ULONG flags = GAA_FLAG_SKIP_UNICAST | GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER | GAA_FLAG_INCLUDE_ALL_INTERFACES;
        ULONG bufferLength = 0;
        PIP_ADAPTER_ADDRESSES buffer;

        if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, NULL, &bufferLength) != ERROR_BUFFER_OVERFLOW)
            return false;

        buffer = (PIP_ADAPTER_ADDRESSES) PhAllocate(bufferLength);
        memset(buffer, 0, bufferLength);

        if (GetAdaptersAddresses(AF_UNSPEC, flags, NULL, buffer, &bufferLength) == ERROR_SUCCESS)
        {
            for (PIP_ADAPTER_ADDRESSES i = buffer; i; i = i->Next)
            {
				PPH_STRING deviceGuid = PhConvertMultiByteToUtf16(i->AdapterName);

				QString DeviceGuid = CastPhString(deviceGuid, false);
				QString DeviceInterface = "\\\\.\\" + DeviceGuid;

				SNicInfo* adapterEntry;
				QMap<QString, SNicInfo>::iterator I = m_NicList.find(DeviceInterface);
				if (I == m_NicList.end())
				{
					I = m_NicList.insert(DeviceInterface, SNicInfo());
					adapterEntry = &I.value();
					adapterEntry->DeviceGuid = DeviceGuid;
					adapterEntry->DeviceInterface = DeviceInterface;
				}
				else
				{
					OldNics.remove(DeviceInterface);
					adapterEntry = &I.value();
				}
				*((PNET_LUID_LH)&adapterEntry->DeviceLuid) = i->Luid;
				adapterEntry->DeviceIndex = i->IfIndex;

				adapterEntry->DeviceName = QString::fromWCharArray(i->Description);
				adapterEntry->DevicePresent = true;
            }
        }

        PhFree(buffer);
    }
    else*/
	/*if (2)
	{
		MIB_IF_TABLE2* pIfTable = NULL;
		if (GetIfTable2(&pIfTable) == NO_ERROR)
		{
			quint64 uRecvTotal = 0;
			quint64 uRecvCount = 0;
			quint64 uSentTotal = 0;
			quint64 uSentCount = 0;
			for (int i = 0; i < pIfTable->NumEntries; i++)
			{
				MIB_IF_ROW2* pIfRow = (MIB_IF_ROW2*)& pIfTable->Table[i];

				if (pIfRow->InterfaceAndOperStatusFlags.FilterInterface)
					continue;

				PPH_STRING deviceGuid = PhFormatGuid(&pIfRow->InterfaceGuid);

				QString DeviceGuid = CastPhString(deviceGuid, false);
				QString DeviceInterface = "\\\\.\\" + DeviceGuid;

				SNicInfo* adapterEntry;
				QMap<QString, SNicInfo>::iterator I = m_NicList.find(DeviceInterface);
				if (I == m_NicList.end())
				{
					I = m_NicList.insert(DeviceInterface, SNicInfo());
					adapterEntry = &I.value();
					adapterEntry->DeviceGuid = DeviceGuid;
					adapterEntry->DeviceInterface = DeviceInterface;
				}
				else
				{
					OldNics.remove(DeviceInterface);
					adapterEntry = &I.value();
				}
				*((PNET_LUID_LH)&adapterEntry->DeviceLuid) = pIfRow->InterfaceLuid;
				adapterEntry->DeviceIndex = pIfRow->InterfaceIndex;

				adapterEntry->DeviceName = QString::fromWCharArray(pIfRow->Description);
				adapterEntry->DevicePresent = true;
			}
		}

		FreeMibTable(pIfTable);
	}
	else*/
    {
        PWSTR deviceInterfaceList;
        ULONG deviceInterfaceListLength = 0;
        PWSTR deviceInterface;

        if (CM_Get_Device_Interface_List_Size(
            &deviceInterfaceListLength,
            (PGUID)&GUID_DEVINTERFACE_NET,
            NULL,
            CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES
            ) != CR_SUCCESS)
        {
            return false;
        }

        deviceInterfaceList = (PWSTR)PhAllocate(deviceInterfaceListLength * sizeof(WCHAR));
        memset(deviceInterfaceList, 0, deviceInterfaceListLength * sizeof(WCHAR));

        if (CM_Get_Device_Interface_List(
            (PGUID)&GUID_DEVINTERFACE_NET,
            NULL,
            deviceInterfaceList,
            deviceInterfaceListLength,
            CM_GET_DEVICE_INTERFACE_LIST_ALL_DEVICES
            ) != CR_SUCCESS)
        {
            PhFree(deviceInterfaceList);
            return false;
        }

        for (deviceInterface = deviceInterfaceList; *deviceInterface; deviceInterface += PhCountStringZ(deviceInterface) + 1)
        {
            HKEY keyHandle;
            DEVINST deviceInstanceHandle;
            PPH_STRING deviceDescription = NULL;

            if (!QueryNetworkDeviceInterfaceDescription(deviceInterface, &deviceInstanceHandle, &deviceDescription))
                continue;

            if (CM_Open_DevInst_Key(
                deviceInstanceHandle,
                KEY_QUERY_VALUE,
                0,
                RegDisposition_OpenExisting,
                &keyHandle,
                CM_REGISTRY_SOFTWARE
                ) == CR_SUCCESS)
            {
				PPH_STRING deviceGuid = PhQueryRegistryString(keyHandle, L"NetCfgInstanceId");

				NDIS_MEDIUM MediumType =  (NDIS_MEDIUM)PhQueryRegistryUlong(keyHandle, L"*MediaType");
				NDIS_PHYSICAL_MEDIUM PhysicalMediaType = (NDIS_PHYSICAL_MEDIUM)PhQueryRegistryUlong(keyHandle, L"*PhysicalMediaType");

				QString deviceInstanceID = CastPhString(PhQueryRegistryString(keyHandle, L"DeviceInstanceID"));

				// Don't list virtual RAS adapters
				// Note: some USB adapters have PhysicalMediaType not set so also check for the deviceInstanceID value
				if (PhysicalMediaType != NdisPhysicalMediumUnspecified || deviceInstanceID.left(14) != "SWD\\MSRRAS\\MS_")
				{
					QString DeviceGuid = CastPhString(deviceGuid, false);
					QString DeviceInterface = "\\\\.\\" + DeviceGuid;

					SNicInfo* adapterEntry;
					QMap<QString, SNicInfo>::iterator I = m_NicList.find(DeviceInterface);
					if (I == m_NicList.end())
					{
						I = m_NicList.insert(DeviceInterface, SNicInfo());
						adapterEntry = &I.value();
						adapterEntry->DeviceGuid = DeviceGuid;
						adapterEntry->DeviceInterface = DeviceInterface;
					}
					else
					{
						OldNics.remove(DeviceInterface);
						adapterEntry = &I.value();
					}
					((PNET_LUID_LH)&adapterEntry->DeviceLuid)->Info.IfType = PhQueryRegistryUlong64(keyHandle, L"*IfType");
					((PNET_LUID_LH)&adapterEntry->DeviceLuid)->Info.NetLuidIndex = PhQueryRegistryUlong64(keyHandle, L"NetLuidIndex");

					HANDLE deviceHandle;
					if (NT_SUCCESS(PhCreateFileWin32(
						&deviceHandle,
						(wchar_t*)adapterEntry->DeviceInterface.toStdWString().c_str(),
						FILE_GENERIC_READ,
						FILE_ATTRIBUTE_NORMAL,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						FILE_OPEN,
						FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
					)))
					{
						PPH_STRING adapterName;

						// Try query the full adapter name
						adapterName = NetworkAdapterQueryName(deviceHandle, deviceGuid);

						if (adapterName)
							adapterEntry->DeviceName = CastPhString(adapterName);

						adapterEntry->DevicePresent = true;
						adapterEntry->DeviceSupported = 2;

						NtClose(deviceHandle);
					}

					if (adapterEntry->DeviceName.isEmpty())
						adapterEntry->DeviceName = CastPhString(deviceDescription, false);

					
					//if (PhysicalMediaType == NdisPhysicalMediumUnspecified)
					//	qDebug() << adapterEntry->DeviceName;

				}

				PhDereferenceObject(deviceGuid);

                NtClose(keyHandle);
            }

            PhDereferenceObject(deviceDescription);
        }

        // Cleanup.
        PhFree(deviceInterfaceList);
    }

	foreach(const QString& DeviceInterface, OldNics)
	{
		if (m_NicList[DeviceInterface].IsRAS)
			continue;
		m_NicList.remove(DeviceInterface);
	}

	return true;
}

void CWinNetMonitor::UpdateNetStats()
{
	bool EnableNDIS = true;

	QWriteLocker Locker(&m_StatsMutex);
	
	/////////////////////////////////////////////////////////////////////
	// update ras connections

	LPRASCONN lpRasConn = new RASCONN[1];
	DWORD cb = sizeof(RASCONN);
	lpRasConn->dwSize = sizeof(RASCONN);
	DWORD dwConnections = 0;
	DWORD dwRet = RasEnumConnections(lpRasConn, &cb, &dwConnections);
	if (dwRet == ERROR_BUFFER_TOO_SMALL)
	{
		delete[] lpRasConn;
		lpRasConn = new RASCONN[dwConnections];
		lpRasConn->dwSize = sizeof(RASCONN);
		dwRet = RasEnumConnections(lpRasConn, &cb, &dwConnections);
	}

	struct SRasCon
	{
		HRASCONN hrasconn;

		QString EntryName;
		QString DeviceName;
	};

	QMap<QString, SRasCon> RasCons;

	if (dwRet == ERROR_SUCCESS)
	{
		for (DWORD i = 0; i < dwConnections; i++)
		{
			LPRASCONN pRasConn = &lpRasConn[i];

			SRasCon RasCon;
			RasCon.hrasconn = pRasConn->hrasconn;
			RasCon.EntryName = QString::fromWCharArray(pRasConn->szEntryName);
			RasCon.DeviceName = QString::fromWCharArray(pRasConn->szDeviceName);

			RasCons.insert(RasCon.EntryName, RasCon);
		}
	}
	
	delete[] lpRasConn;

	if (!RasCons.isEmpty())
	{
		QSet<QString> OldNics = m_NicList.keys().toSet();

		MIB_IF_TABLE2* pIfTable = NULL;
		if (GetIfTable2(&pIfTable) == NO_ERROR)
		{
			quint64 uRecvTotal = 0;
			quint64 uRecvCount = 0;
			quint64 uSentTotal = 0;
			quint64 uSentCount = 0;
			for (int i = 0; i < pIfTable->NumEntries; i++)
			{
				MIB_IF_ROW2* pIfRow = (MIB_IF_ROW2*)& pIfTable->Table[i];

				if (pIfRow->InterfaceAndOperStatusFlags.FilterInterface)
					continue;

				QString EntryName = QString::fromWCharArray(pIfRow->Description);
				if (!RasCons.contains(EntryName))
					continue;

				SRasCon* pRasCon = &RasCons[EntryName];

				PPH_STRING deviceGuid = PhFormatGuid(&pIfRow->InterfaceGuid);

				QString DeviceGuid = CastPhString(deviceGuid, false);
				QString DeviceInterface = "\\\\.\\" + DeviceGuid;

				SNicInfo* adapterEntry;
				QMap<QString, SNicInfo>::iterator I = m_NicList.find(DeviceInterface);
				if (I == m_NicList.end())
				{
					I = m_NicList.insert(DeviceInterface, SNicInfo());
					adapterEntry = &I.value();
					adapterEntry->DeviceGuid = DeviceGuid;
					adapterEntry->DeviceInterface = DeviceInterface;
					adapterEntry->IsRAS = true;
				}
				else
				{
					OldNics.remove(DeviceInterface);
					adapterEntry = &I.value();
				}
				*((PNET_LUID_LH)&adapterEntry->DeviceLuid) = pIfRow->InterfaceLuid;
				adapterEntry->DeviceIndex = pIfRow->InterfaceIndex;

				adapterEntry->DeviceName = EntryName;
				adapterEntry->DevicePresent = true;


				if (adapterEntry->LinkState != CNetMonitor::SNicInfo::eConnected)
				{
					RASCONNSTATUS status = { sizeof(RASCONNSTATUS) };
					if (RasGetConnectStatus(pRasCon->hrasconn, &status) == ERROR_SUCCESS)
					{
						adapterEntry->Domains = QSet<QString> () << QString::fromWCharArray(status.szPhoneNumber);
						if (status.rasconnstate == RASCS_Connected)
							adapterEntry->LinkState = CNetMonitor::SNicInfo::eConnected;
						else if (status.rasconnstate == RASCS_Disconnected)
							adapterEntry->LinkState = CNetMonitor::SNicInfo::eDisconnected;
						else
							adapterEntry->LinkState = CNetMonitor::SNicInfo::eUnknown;
					}

					if (adapterEntry->LinkState == CNetMonitor::SNicInfo::eConnected)
					{
						RAS_PROJECTION_INFO projection;
						projection.version = RASAPIVERSION_CURRENT;
						DWORD dwSize = sizeof(projection);
						if (RasGetProjectionInfoEx(pRasCon->hrasconn, &projection, &dwSize) == ERROR_SUCCESS)
						{
							switch (projection.type)
							{
								case PROJECTION_INFO_TYPE_PPP:
								{
									adapterEntry->Addresses = QList<QHostAddress>() << QHostAddress(ntohl(projection.ppp.ipv4Address.S_un.S_addr));
									adapterEntry->Gateways = QList<QHostAddress>() << QHostAddress(ntohl(projection.ppp.ipv4ServerAddress.S_un.S_addr));
									// ToDo: IPv6
									break;
								}
								case PROJECTION_INFO_TYPE_IKEv2:
								{
									adapterEntry->Addresses = QList<QHostAddress>() << QHostAddress(ntohl(projection.ikev2.ipv4Address.S_un.S_addr));
									adapterEntry->Gateways = QList<QHostAddress>() << QHostAddress(ntohl(projection.ikev2.ipv4ServerAddress.S_un.S_addr));
									// ToDo: IPv6
									break;
								}
							}
						}

						/*if (pRasCon->ConnectionState == CNetMonitor::SNicInfo::eConnected)
						{
							RASPPPIP projection = { sizeof(RASPPPIP) };
							if (RasGetProjectionInfo(pRasConn->hrasconn, RASP_PppIp, &projection, &projection.dwSize) == ERROR_SUCCESS)
							{
								pRasCon->Client = QHostAddress(QString::fromWCharArray(projection.szIpAddress));
								pRasCon->Server = QHostAddress(QString::fromWCharArray(projection.szServerIpAddress));
							}
							else
							{
								RASPPPIPV6 projection6 = { sizeof(RASPPPIPV6) };
								if (RasGetProjectionInfo(pRasConn->hrasconn, RASP_PppIpv6, &projection6, &projection6.dwSize) == ERROR_SUCCESS)
								{
						
								}
							}
						}*/
					}
				}

				/*RAS_STATS Statistics;
				Statistics.dwSize = sizeof(RAS_STATS);
				if (RasGetConnectionStatistics(pRasConn->hrasconn, &Statistics) == ERROR_SUCCESS)
				{
					pRasCon->SendDelta.Update(Statistics.dwFramesXmited);
					pRasCon->SendRawDelta.Update(pRasCon->BytesSent.FixValue(Statistics.dwBytesXmited));

					pRasCon->ReceiveDelta.Update(Statistics.dwFramesRcved);
					pRasCon->ReceiveRawDelta.Update(pRasCon->BytesRecv.FixValue(Statistics.dwBytesRcved));
				}
				pRasCon->UpdateStats();*/
			}
		}

		FreeMibTable(pIfTable);

		foreach(const QString& DeviceInterface, OldNics)
		{
			if (!m_NicList[DeviceInterface].IsRAS)
				continue;
			m_NicList.remove(DeviceInterface);
		}
	}

	/////////////////////////////////////////////////////////////////////
	// Update stats

	quint64 curTick = GetCurTick();

	for (QMap<QString, SNicInfo>::iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		SNicInfo* entry = &I.value();

        HANDLE deviceHandle = NULL;
        ULONG64 networkInOctets = 0;
		ULONG64 networkInCount = 0;
        ULONG64 networkOutOctets = 0;
		ULONG64 networkOutCount = 0;
        NDIS_MEDIA_CONNECT_STATE mediaState = MediaConnectStateUnknown;
		ULONG64 interfaceLinkSpeed = 0;

		if (EnableNDIS && entry->DeviceSupported)
		{
			wstring DeviceInterface = entry->DeviceInterface.toStdWString();
			PWSTR deviceInterface = (PWSTR)DeviceInterface.c_str();
			if (NT_SUCCESS(PhCreateFileWin32(
				&deviceHandle,
				deviceInterface,
				FILE_GENERIC_READ,
				FILE_ATTRIBUTE_NORMAL,
				FILE_SHARE_READ | FILE_SHARE_WRITE,
				FILE_OPEN,
				FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT
			)))
			{
				if (entry->DeviceSupported == 2)
				{
					// Check the network adapter supports the OIDs we're going to be using.
					if (NetworkAdapterQuerySupported(deviceHandle))
					{
						entry->DeviceSupported = 1;
					}
					else
					{
						entry->DeviceSupported = 0;

						// Device is faulty. Close the handle so we can fallback to GetIfEntry.
						NtClose(deviceHandle);
						deviceHandle = NULL;
					}
				}
			}
		}

		if (deviceHandle)
        {
            NDIS_STATISTICS_INFO interfaceStats;
            NDIS_LINK_STATE interfaceState;

            memset(&interfaceStats, 0, sizeof(NDIS_STATISTICS_INFO));

            if (NT_SUCCESS(NetworkAdapterQueryLinkState(deviceHandle, &interfaceState)))
            {
                mediaState = interfaceState.MediaConnectState;
				interfaceLinkSpeed = interfaceState.XmitLinkSpeed;
            }
			else
			{
				NetworkAdapterQueryLinkSpeed(deviceHandle, &interfaceLinkSpeed);
			}

            if (NT_SUCCESS(NetworkAdapterQueryStatistics(deviceHandle, &interfaceStats)))
            {
                if (!(interfaceStats.SupportedStatistics & NDIS_STATISTICS_FLAGS_VALID_BYTES_RCV))
                    networkInOctets = NetworkAdapterQueryValue(deviceHandle, OID_GEN_BYTES_RCV);
				else
				{
					networkInOctets = interfaceStats.ifHCInOctets;
					networkInCount = interfaceStats.ifHCInUcastPkts + interfaceStats.ifHCInMulticastPkts + interfaceStats.ifHCInBroadcastPkts;
				}

                if (!(interfaceStats.SupportedStatistics & NDIS_STATISTICS_FLAGS_VALID_BYTES_XMIT))
                    networkOutOctets = NetworkAdapterQueryValue(deviceHandle, OID_GEN_BYTES_XMIT);
				else
				{
					networkOutOctets = interfaceStats.ifHCOutOctets;
					networkOutCount = interfaceStats.ifHCOutUcastPkts + interfaceStats.ifHCOutMulticastPkts + interfaceStats.ifHCOutBroadcastPkts;
				}
            }
            else
            {
                networkInOctets = NetworkAdapterQueryValue(deviceHandle, OID_GEN_BYTES_RCV);
                networkOutOctets = NetworkAdapterQueryValue(deviceHandle, OID_GEN_BYTES_XMIT);
            }

            // HACK: Pull the Adapter name from the current query.
            //if (!entry->AdapterName)
            //    entry->AdapterName = NetworkAdapterQueryName(deviceHandle, entry->AdapterId.InterfaceGuid);

            entry->DevicePresent = true;

            NtClose(deviceHandle);
        }
        else
        {
            MIB_IF_ROW2 interfaceRow;

			DV_NETADAPTER_ID AdapterId;
			AdapterId.InterfaceLuid = *(IF_LUID*)&entry->DeviceLuid;
			AdapterId.InterfaceIndex = entry->DeviceIndex;
			//AdapterId.InterfaceGuid // ignored by QueryInterfaceRow
			//AdapterId.InterfaceDevice // ignored by QueryInterfaceRow

            if (QueryInterfaceRow(&AdapterId, &interfaceRow))
            {
                networkInOctets = interfaceRow.InOctets;
				networkInCount = interfaceRow.InUcastPkts + interfaceRow.InNUcastPkts;

                networkOutOctets = interfaceRow.OutOctets;
				networkOutCount = interfaceRow.OutUcastPkts + interfaceRow.OutNUcastPkts;

                mediaState = interfaceRow.MediaConnectState;
                
                // HACK: Pull the Adapter name from the current query.
                //if (!entry->AdapterName && PhCountStringZ(interfaceRow.Description) > 0)
                //    entry->AdapterName = PhCreateString(interfaceRow.Description);

                entry->DevicePresent = mediaState != MediaConnectStateUnknown;
            }
            else
            {
                entry->DevicePresent = false;
            }
        }

		entry->LinkState = (SNicInfo::ELinkState)mediaState;
		entry->LinkSpeed = interfaceLinkSpeed;

		entry->SetReceive(networkInOctets, networkInCount);
		entry->SetSend(networkOutOctets, networkOutCount);

		quint64 time_ms = curTick - entry->LastStatUpdate;
		entry->LastStatUpdate = curTick;
		entry->UpdateStats(time_ms);
	}
}

PVOID NetAdapterGetAddresses(
    _In_ ULONG Family
    )
{
    ULONG flags = GAA_FLAG_INCLUDE_GATEWAYS | GAA_FLAG_SKIP_FRIENDLY_NAME | GAA_FLAG_INCLUDE_ALL_INTERFACES;
    ULONG bufferLength = 0;
    PIP_ADAPTER_ADDRESSES buffer;

    if (GetAdaptersAddresses(Family, flags, NULL, NULL, &bufferLength) != ERROR_BUFFER_OVERFLOW)
        return NULL;

    buffer = (PIP_ADAPTER_ADDRESSES)PhAllocate(bufferLength);
    memset(buffer, 0, bufferLength);

    if (GetAdaptersAddresses(Family, flags, NULL, buffer, &bufferLength) != ERROR_SUCCESS)
    {
        PhFree(buffer);
        return NULL;
    }

    return buffer;
}

VOID NetAdapterEnumerateAddresses(
    _In_ PVOID AddressesBuffer,
    _In_ ULONG64 InterfaceLuid,
    _In_ CNetMonitor::SNicInfo* entry
    )
{
    PIP_ADAPTER_ADDRESSES addressesBuffer;
    PIP_ADAPTER_UNICAST_ADDRESS unicastAddress;
    PIP_ADAPTER_GATEWAY_ADDRESS gatewayAddress;
    PIP_ADAPTER_DNS_SERVER_ADDRESS dnsAddress;

    for (addressesBuffer = (PIP_ADAPTER_ADDRESSES)AddressesBuffer; addressesBuffer; addressesBuffer = addressesBuffer->Next)
    {
        if (addressesBuffer->Luid.Value != InterfaceLuid)
            continue;

        if (addressesBuffer->DnsSuffix && PhCountStringZ(addressesBuffer->DnsSuffix) > 0)
        {
			entry->Domains.insert(QString::fromWCharArray(addressesBuffer->DnsSuffix));
        }

        for (unicastAddress = addressesBuffer->FirstUnicastAddress; unicastAddress; unicastAddress = unicastAddress->Next)
        {
            if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET)
            {
                PSOCKADDR_IN ipv4SockAddr;
                IN_ADDR subnetMask = { 0 };
                ipv4SockAddr = (PSOCKADDR_IN)unicastAddress->Address.lpSockaddr;

				entry->Addresses.append(QHostAddress(ntohl(ipv4SockAddr->sin_addr.S_un.S_addr)));

				ConvertLengthToIpv4Mask(unicastAddress->OnLinkPrefixLength, &subnetMask.s_addr);
				entry->NetMasks.append(QHostAddress(ntohl(subnetMask.s_addr)));
            }
            else if (unicastAddress->Address.lpSockaddr->sa_family == AF_INET6)
            {
                PSOCKADDR_IN6 ipv6SockAddr;
                ipv6SockAddr = (PSOCKADDR_IN6)unicastAddress->Address.lpSockaddr;

				entry->Addresses.append(QHostAddress(ipv6SockAddr->sin6_addr.u.Byte));
            }
        }

        for (gatewayAddress = addressesBuffer->FirstGatewayAddress; gatewayAddress; gatewayAddress = gatewayAddress->Next)
        {
            if (gatewayAddress->Address.lpSockaddr->sa_family == AF_INET)
            {
                PSOCKADDR_IN ipv4SockAddr;
                WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";

                ipv4SockAddr = (PSOCKADDR_IN)gatewayAddress->Address.lpSockaddr;

				entry->Gateways.append(QHostAddress(ntohl(ipv4SockAddr->sin_addr.S_un.S_addr)));
            }
            else if (gatewayAddress->Address.lpSockaddr->sa_family == AF_INET6)
            {
                PSOCKADDR_IN6 ipv6SockAddr;
                WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";

                ipv6SockAddr = (PSOCKADDR_IN6)gatewayAddress->Address.lpSockaddr;

				entry->Gateways.append(QHostAddress(ipv6SockAddr->sin6_addr.u.Byte));
            }
        }

        for (dnsAddress = addressesBuffer->FirstDnsServerAddress; dnsAddress; dnsAddress = dnsAddress->Next)
        {
            if (dnsAddress->Address.lpSockaddr->sa_family == AF_INET)
            {
                PSOCKADDR_IN ipv4SockAddr;
                WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";

                ipv4SockAddr = (PSOCKADDR_IN)dnsAddress->Address.lpSockaddr;

                entry->DNS.append(QHostAddress(ntohl(ipv4SockAddr->sin_addr.S_un.S_addr)));
            }
            else if (dnsAddress->Address.lpSockaddr->sa_family == AF_INET6)
            {
                PSOCKADDR_IN6 ipv6SockAddr;
                WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";

                ipv6SockAddr = (PSOCKADDR_IN6)dnsAddress->Address.lpSockaddr;

                entry->DNS.append(QHostAddress(ipv6SockAddr->sin6_addr.u.Byte));
            }
        }
    }
}


void CWinNetMonitor::UpdateAddresses()
{
	m->UpdateAddressesPending = false;

	QWriteLocker Locker(&m_StatsMutex);

	for (QMap<QString, SNicInfo>::iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		SNicInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		entry->Addresses.clear();
		entry->NetMasks.clear();
		entry->Gateways.clear();
		entry->DNS.clear();
		entry->Domains.clear();

		PVOID addressesBuffer = NULL;
		if (addressesBuffer = NetAdapterGetAddresses(AF_INET))
		{
			NetAdapterEnumerateAddresses(addressesBuffer, entry->DeviceLuid, entry);
			PhFree(addressesBuffer);
		}
		if (addressesBuffer = NetAdapterGetAddresses(AF_INET6))
		{
			NetAdapterEnumerateAddresses(addressesBuffer, entry->DeviceLuid, entry);
			PhFree(addressesBuffer);
		}
	}
}

static void WINAPI OnInterfaceChange(PVOID callerContext, PMIB_IPINTERFACE_ROW row, MIB_NOTIFICATION_TYPE notificationType) 
{
	CWinNetMonitor *This = reinterpret_cast<CWinNetMonitor*>(callerContext);
	This->QueueUpdateAddresses();
}

void CWinNetMonitor::QueueUpdateAddresses()
{
	if(!m->UpdateAddressesPending)
	{
		m->UpdateAddressesPending = true;
		QTimer::singleShot(100,this,SLOT(UpdateAddresses()));
	}
}

void CWinNetMonitor::StartWatch()
{
	NotifyIpInterfaceChange(AF_UNSPEC, OnInterfaceChange, this, true, &m->eventHandle);
}

void CWinNetMonitor::StopWatch()
{
	if (m->eventHandle) {
		CancelMibChangeNotify2(m->eventHandle);
	}
}