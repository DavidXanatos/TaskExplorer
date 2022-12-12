/*
 * Task Explorer -
 *   qt port of Firewall Monitor plugin
 *
 * Copyright (C) 2015-2017 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains System Informer code.
 *
 */

#include "stdafx.h"
#include "FirewallMonitor.h"
#include "../../../../MiscHelpers/Common/Common.h"
#include "../ProcessHacker.h"
#include <fwpmu.h>
#include <fwpsu.h>
#include "../ProcessHacker/wf.h"

#pragma comment(lib, "fwpuclnt.lib")

#define FWP_DIRECTION_IN 0x00003900L
#define FWP_DIRECTION_OUT 0x00003901L
#define FWP_DIRECTION_FORWARD 0x00003902L

typedef ULONG (WINAPI* _FwpmNetEventSubscribe1)(
   _In_ HANDLE engineHandle,
   _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
   _In_ FWPM_NET_EVENT_CALLBACK1 callback,
   _In_opt_ void* context,
   _Out_ HANDLE* eventsHandle
   );

typedef ULONG (WINAPI* _FwpmNetEventSubscribe2)(
   _In_ HANDLE engineHandle,
   _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
   _In_ FWPM_NET_EVENT_CALLBACK2 callback,
   _In_opt_ void* context,
   _Out_ HANDLE* eventsHandle
   );

typedef ULONG (WINAPI* _FwpmNetEventSubscribe3)(
    _In_ HANDLE engineHandle,
    _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
    _In_ FWPM_NET_EVENT_CALLBACK3 callback,
    _In_opt_ void* context,
    _Out_ HANDLE* eventsHandle
    );

typedef ULONG(WINAPI* _FwpmNetEventSubscribe4)(
    _In_ HANDLE engineHandle,
    _In_ const FWPM_NET_EVENT_SUBSCRIPTION0* subscription,
    _In_ FWPM_NET_EVENT_CALLBACK4 callback,
    _In_opt_ void* context,
    _Out_ HANDLE* eventsHandle
    );

struct SFirewallMonitor
{
	SFirewallMonitor()
	{
		FwEngineHandle = NULL;
		FwEventHandle = NULL;
		FwEnumHandle = NULL;

		FwpmNetEventSubscribe1_I = NULL;
		FwpmNetEventSubscribe2_I = NULL;
		FwpmNetEventSubscribe3_I = NULL;
		FwpmNetEventSubscribe4_I = NULL;
	}

	HANDLE FwEngineHandle;
	HANDLE FwEventHandle;
	HANDLE FwEnumHandle;

	_FwpmNetEventSubscribe1 FwpmNetEventSubscribe1_I;
	_FwpmNetEventSubscribe2 FwpmNetEventSubscribe2_I;
	_FwpmNetEventSubscribe3 FwpmNetEventSubscribe3_I;
	_FwpmNetEventSubscribe4 FwpmNetEventSubscribe4_I;
};


CFirewallMonitor::CFirewallMonitor(QObject *parent) : QObject(parent) //QThread(parent)
{
	m_bRunning = false;
	
	m = new SFirewallMonitor();
}

bool CFirewallMonitor::Init()
{
	// start thread
	//m_bRunning = true;
	//start();

	m_bRunning = StartFwMonitor();

	return true;
}

CFirewallMonitor::~CFirewallMonitor()
{
	//quit();

	/*m_bRunning = false;

	if (!wait(5 * 1000))
		terminate();*/

	if (m_bRunning)
		StopFwMonitor();

	delete m;
}


VOID CALLBACK FwMonEventCallback(_Inout_ PVOID FwContext, _In_ const FWPM_NET_EVENT* FwEvent)
{

	CFirewallMonitor* This = (CFirewallMonitor*)FwContext;
	SFirewallMonitor* m = This->M();

	quint64 AddedTime = GetTime();
	int RuleEventType = FwEvent->type;
	bool IsLoopback = false;
	quint32 FwRuleEventDirection = -1;
	QString FwRuleLayerNameString;
	//QString FwRuleLayerDescriptionString;
	QString FwRuleNameString;
	QString FwRuleDescriptionString;
	QHostAddress LocalAddress;
	QHostAddress RemoteAddress;
	quint16 LocalPort = 0;
	quint16 RemotePort = 0;
	QString ProcessFileNameString;
	//QByteArray UserSid;
	QString ProtocalString;
 
    if (FwEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_DROP)
    {
        FWPM_FILTER* fwFilterItem = NULL;
        FWPM_LAYER* fwLayerItem = NULL;
        FWPM_NET_EVENT_CLASSIFY_DROP* fwDropEvent = FwEvent->classifyDrop;

        switch (fwDropEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            break;
        default:
            return;
        }

        if (IsLoopback = fwDropEvent->isLoopback)
            return;

        switch (fwDropEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
            FwRuleEventDirection = FWP_DIRECTION_INBOUND;
            break;
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            FwRuleEventDirection = FWP_DIRECTION_OUTBOUND;
            break;
        }

        if (fwDropEvent->layerId && (FwpmLayerGetById(m->FwEngineHandle, fwDropEvent->layerId, &fwLayerItem) == ERROR_SUCCESS))
        {
            if (
                IsEqualGUID(*(_GUID*)&fwLayerItem->layerKey, *(_GUID*)&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4) ||
                IsEqualGUID(*(_GUID*)&fwLayerItem->layerKey, *(_GUID*)&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6)
                )
            {
                FwpmFreeMemory((void**)&fwLayerItem);
                return;
            }

            if (fwLayerItem->displayData.name && PhCountStringZ(fwLayerItem->displayData.name) > 0)
            {
				FwRuleLayerNameString = QString::fromWCharArray(fwLayerItem->displayData.name);
            }

            //FwRuleLayerDescriptionString = QString::fromWCharArray(fwLayerRuleItem->displayData.description);

            FwpmFreeMemory((void**)&fwLayerItem);
        }

        if (fwDropEvent->filterId && (FwpmFilterGetById(m->FwEngineHandle, fwDropEvent->filterId, &fwFilterItem) == ERROR_SUCCESS))
        {
            if (fwFilterItem->displayData.name && PhCountStringZ(fwFilterItem->displayData.name) > 0)
                FwRuleNameString = QString::fromWCharArray(fwFilterItem->displayData.name);

            if (fwFilterItem->displayData.description && PhCountStringZ(fwFilterItem->displayData.description) > 0)
                FwRuleDescriptionString = QString::fromWCharArray(fwFilterItem->displayData.description);

            FwpmFreeMemory((void**)&fwFilterItem);
        }
    }
    else if (FwEvent->type == FWPM_NET_EVENT_TYPE_CLASSIFY_ALLOW)
    {
        FWPM_FILTER* fwFilterItem = NULL;
        FWPM_LAYER* fwLayerItem = NULL;
        FWPM_NET_EVENT_CLASSIFY_ALLOW* fwAllowEvent = FwEvent->classifyAllow;

        if (IsLoopback = fwAllowEvent->isLoopback)
            return;

        switch (fwAllowEvent->msFwpDirection)
        {
        case FWP_DIRECTION_IN:
        case FWP_DIRECTION_INBOUND:
            FwRuleEventDirection = FWP_DIRECTION_INBOUND;
            break;
        case FWP_DIRECTION_OUT:
        case FWP_DIRECTION_OUTBOUND:
            FwRuleEventDirection = FWP_DIRECTION_OUTBOUND;
            break;
        }

        if (fwAllowEvent->layerId && (FwpmLayerGetById(m->FwEngineHandle, fwAllowEvent->layerId, &fwLayerItem) == ERROR_SUCCESS))
        {
            if (
                IsEqualGUID(*(_GUID*)&fwLayerItem->layerKey, *(_GUID*)&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V4) ||
                IsEqualGUID(*(_GUID*)&fwLayerItem->layerKey, *(_GUID*)&FWPM_LAYER_ALE_FLOW_ESTABLISHED_V6)
                )
            {
                FwpmFreeMemory((void**)&fwLayerItem);
                return;
            }

            if (fwLayerItem->displayData.name && PhCountStringZ(fwLayerItem->displayData.name) > 0)
            {
                FwRuleLayerNameString = QString::fromWCharArray(fwLayerItem->displayData.name);
            }

            //FwRuleLayerDescriptionString = QString::fromWCharArray(fwLayerRuleItem->displayData.description);

            FwpmFreeMemory((void**)&fwLayerItem);
        }

        if (fwAllowEvent->filterId && (FwpmFilterGetById(m->FwEngineHandle, fwAllowEvent->filterId, &fwFilterItem) == ERROR_SUCCESS))
        {
            if (fwFilterItem->displayData.name && PhCountStringZ(fwFilterItem->displayData.name) > 0)
            {
                FwRuleNameString = QString::fromWCharArray(fwFilterItem->displayData.name);
            }

            if (fwFilterItem->displayData.description && PhCountStringZ(fwFilterItem->displayData.description) > 0)
            {
                FwRuleDescriptionString = QString::fromWCharArray(fwFilterItem->displayData.description);
            }

            FwpmFreeMemory((void**)&fwFilterItem);
        }
    }
    else
    {
        return;
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_IP_VERSION_SET) != 0)
    {
        if (FwEvent->header.ipVersion == FWP_IP_VERSION_V4)
        {
            if (IN4_IS_ADDR_UNSPECIFIED((PIN_ADDR)&FwEvent->header.localAddrV4))
                return;

            if (IN4_IS_ADDR_LOOPBACK((PIN_ADDR)&FwEvent->header.localAddrV4))
                return;

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            /*PPH_STRING LocalAddressString = PhFormatString(
                L"%lu.%lu.%lu.%lu",
                ((PBYTE)&FwEvent->header.localAddrV4)[3],
                ((PBYTE)&FwEvent->header.localAddrV4)[2],
                ((PBYTE)&FwEvent->header.localAddrV4)[1],
                ((PBYTE)&FwEvent->header.localAddrV4)[0]
                );*/
			LocalAddress = QHostAddress(FwEvent->header.localAddrV4);

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            /*PPH_STRING RemoteAddressString = PhFormatString(
                L"%lu.%lu.%lu.%lu",
                ((PBYTE)&FwEvent->header.remoteAddrV4)[3],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[2],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[1],
                ((PBYTE)&FwEvent->header.remoteAddrV4)[0]
                );*/
			RemoteAddress = QHostAddress(FwEvent->header.remoteAddrV4);

            //IN_ADDR ipv4Address = { 0 };
            //PWSTR ipv4StringTerminator = 0;
            //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
            //ULONG localAddrV4 = _byteswap_ulong(FwEvent->header.localAddrV4);
            //RtlIpv4AddressToString((PIN_ADDR)&localAddrV4, ipv4AddressString);
            //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
            //fwEventItem->LocalAddressString = PhFormatString(L"%s", ipv4AddressString);
            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)           
            //IN_ADDR ipv4Address = { 0 };
            //PWSTR ipv4StringTerminator = 0;
            //WCHAR ipv4AddressString[INET_ADDRSTRLEN] = L"";
            //ULONG remoteAddrV4 = _byteswap_ulong(FwEvent->header.remoteAddrV4);
            //RtlIpv4AddressToString((PIN_ADDR)&remoteAddrV4, ipv4AddressString);
            //RtlIpv4StringToAddress(ipv4AddressString, TRUE, &ipv4StringTerminator, &ipv4Address);
            //fwEventItem->RemoteAddressString = PhCreateString(ipv4AddressString);
        }
        else if (FwEvent->header.ipVersion == FWP_IP_VERSION_V6)
        {
            if (IN6_IS_ADDR_UNSPECIFIED((PIN6_ADDR)&FwEvent->header.localAddrV6))
                return;

            if (IN6_IS_ADDR_LOOPBACK((PIN6_ADDR)&FwEvent->header.localAddrV6))
                return;
           
            IN6_ADDR ipv6Address = { 0 };
            const wchar_t* ipv6StringTerminator = 0;
            WCHAR ipv6AddressString[INET6_ADDRSTRLEN] = L"";

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_ADDR_SET) != 0)
            //RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.localAddrV6, ipv6AddressString);
            //RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);
            //PPH_STRING LocalAddressString = PhFormatString(L"%s", ipv6AddressString);
			LocalAddress = QHostAddress(ipv6Address.u.Byte);

            //if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_ADDR_SET) != 0)
            //RtlIpv6AddressToString((struct in6_addr*)&FwEvent->header.remoteAddrV6, ipv6AddressString);
            //RtlIpv6StringToAddress(ipv6AddressString, &ipv6StringTerminator, &ipv6Address);
            //PPH_STRING RemoteAddressString = PhFormatString(L"%s", ipv6AddressString);
			RemoteAddress = QHostAddress(ipv6Address.u.Byte);
        }
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_LOCAL_PORT_SET) != 0)
    {
        LocalPort = FwEvent->header.localPort;
    }

    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_REMOTE_PORT_SET) != 0)
    {
        RemotePort = FwEvent->header.remotePort;
    }
 
    if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_APP_ID_SET) != 0)
    {
        if (FwEvent->header.appId.data && FwEvent->header.appId.size > 0)
        {
            PPH_STRING fileName = PhCreateStringEx((PWSTR)FwEvent->header.appId.data, (SIZE_T)FwEvent->header.appId.size);
            ProcessFileNameString = CastPhString(PhResolveDevicePrefix(fileName));
            PhDereferenceObject(fileName);
        }

		// WTF there is no PID !? useless

        /*if (fwEventItem->ProcessFileNameString)
        {
            ProcessNameString = PhGetFileName(fwEventItem->ProcessFileNameString);
            ProcessBaseString = PhGetBaseName(fwEventItem->ProcessFileNameString);

            //FWP_BYTE_BLOB* fwpApplicationByteBlob = NULL;
            //if (FwpmGetAppIdFromFileName(fileNameString->Buffer, &fwpApplicationByteBlob) == ERROR_SUCCESS)
            //fwEventItem->ProcessBaseString = PhCreateStringEx(fwpApplicationByteBlob->data, fwpApplicationByteBlob->size);        
            //fwEventItem->Icon = PhGetFileShellIcon(PhGetString(fwEventItem->ProcessFileNameString), L".exe", FALSE);
        }*/
    }

    /*if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_USER_ID_SET) != 0)
    {
        //UserNameString = PhGetSidFullName(FwEvent->header.userId, TRUE, NULL);
		UserSid = QByteArray((char*)FwEvent->header.userId, RtlLengthSid(FwEvent->header.userId));
    }*/

	/*if ((FwEvent->header.flags & FWPM_NET_EVENT_FLAG_PACKAGE_ID_SET) != 0)
    {
		PackageSid = QByteArray((char*)FwEvent->header.userId, RtlLengthSid(FwEvent->header.packageSid));
    }*/
	

    switch (FwEvent->header.ipProtocol)
    {
    case IPPROTO_HOPOPTS:				ProtocalString = "HOPOPTS"; break;
    case IPPROTO_ICMP:					ProtocalString = "ICMP"; break;
    case IPPROTO_IGMP:					ProtocalString = "IGMP"; break;
    case IPPROTO_GGP:					ProtocalString = "GGP"; break;
    case IPPROTO_IPV4:					ProtocalString = "IPv4"; break;
    case IPPROTO_ST:					ProtocalString = "ST"; break;
    case IPPROTO_TCP:					ProtocalString = "TCP"; break;
    case IPPROTO_CBT:					ProtocalString = "CBT"; break;
    case IPPROTO_EGP:					ProtocalString = "EGP"; break;
    case IPPROTO_IGP:					ProtocalString = "IGP"; break;
    case IPPROTO_PUP:					ProtocalString = "PUP"; break;
    case IPPROTO_UDP:					ProtocalString = "UDP"; break;
    case IPPROTO_IDP:					ProtocalString = "IDP"; break;
    case IPPROTO_RDP:					ProtocalString = "RDP"; break;
    case IPPROTO_IPV6:					ProtocalString = "IPv6"; break;
    case IPPROTO_ROUTING:				ProtocalString = "ROUTING"; break;
    case IPPROTO_FRAGMENT:				ProtocalString = "FRAGMENT"; break;
    case IPPROTO_ESP:					ProtocalString = "ESP"; break;
    case IPPROTO_AH:					ProtocalString = "AH"; break;
    case IPPROTO_ICMPV6:				ProtocalString = "ICMPv6"; break;
    case IPPROTO_DSTOPTS:				ProtocalString = "DSTOPTS"; break;
    case IPPROTO_ND:					ProtocalString = "ND"; break;
    case IPPROTO_ICLFXBM:				ProtocalString = "ICLFXBM"; break;
    case IPPROTO_PIM:					ProtocalString = "PIM"; break;
    case IPPROTO_PGM:					ProtocalString = "PGM"; break;
    case IPPROTO_L2TP:					ProtocalString = "L2TP"; break;
    case IPPROTO_SCTP:					ProtocalString = "SCTP"; break;
    case IPPROTO_RESERVED_IPSEC:		ProtocalString = "IPSEC"; break;
    case IPPROTO_RESERVED_IPSECOFFLOAD:	ProtocalString = "IPSECOFFLOAD"; break;
    case IPPROTO_RESERVED_WNV:			ProtocalString = "WNV"; break; 
    case IPPROTO_RAW:
    case IPPROTO_RESERVED_RAW:			ProtocalString = "RAW"; break;
    case IPPROTO_NONE:
    default:							ProtocalString = "Unknown";
    }

}

bool CFirewallMonitor::StartFwMonitor()
{
    FWP_VALUE value = { FWP_EMPTY };
    FWPM_SESSION session = { 0 };
    FWPM_NET_EVENT_SUBSCRIPTION subscription = { 0 };
    FWPM_NET_EVENT_ENUM_TEMPLATE eventTemplate = { 0 };

    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        m->FwpmNetEventSubscribe4_I = (_FwpmNetEventSubscribe4)PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe4");
    }
    else if (WindowsVersion >= WINDOWS_10_RS4)
    {
        m->FwpmNetEventSubscribe3_I = (_FwpmNetEventSubscribe3)PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe3");
    }
    else
    {
        if (!(m->FwpmNetEventSubscribe2_I = (_FwpmNetEventSubscribe2)PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe2")))
        {
            m->FwpmNetEventSubscribe1_I = (_FwpmNetEventSubscribe1)PhGetModuleProcAddress(L"fwpuclnt.dll", "FwpmNetEventSubscribe1");
        }
    }
   
    session.flags = 0;// FWPM_SESSION_FLAG_DYNAMIC;
    session.displayData.name  = L"PhFirewallMonitoringSession";
    session.displayData.description = L"Non-Dynamic session for Task Explorer";

    // Create a non-dynamic BFE session
    if (FwpmEngineOpen(NULL, RPC_C_AUTHN_WINNT, NULL, &session, &m->FwEngineHandle) != ERROR_SUCCESS)
        return false;

    value.type = FWP_UINT32;
    value.uint32 = 1;

    // Enable collection of NetEvents
    if (FwpmEngineSetOption(m->FwEngineHandle, FWPM_ENGINE_COLLECT_NET_EVENTS, &value) != ERROR_SUCCESS)
        return false;

    if (WindowsVersion >= WINDOWS_8)
    {
        value.type = FWP_UINT32;
        value.uint32 =
            FWPM_NET_EVENT_KEYWORD_CAPABILITY_DROP |
            FWPM_NET_EVENT_KEYWORD_CAPABILITY_ALLOW |
            FWPM_NET_EVENT_KEYWORD_CLASSIFY_ALLOW |
            FWPM_NET_EVENT_KEYWORD_INBOUND_MCAST |
            FWPM_NET_EVENT_KEYWORD_INBOUND_BCAST;

        if (FwpmEngineSetOption(m->FwEngineHandle, FWPM_ENGINE_NET_EVENT_MATCH_ANY_KEYWORDS, &value) != ERROR_SUCCESS)
            return false;

        value.type = FWP_UINT32;
        value.uint32 = 1;

        if (FwpmEngineSetOption(m->FwEngineHandle, FWPM_ENGINE_MONITOR_IPSEC_CONNECTIONS, &value) != ERROR_SUCCESS)
            return false;
    }
  
    eventTemplate.numFilterConditions = 0; // get events for all conditions

    subscription.sessionKey = session.sessionKey;
    subscription.enumTemplate = &eventTemplate;

    // Subscribe to the events
    if (WindowsVersion >= WINDOWS_10_RS5)
    {
        if (!m->FwpmNetEventSubscribe4_I)
            return false;

        if (m->FwpmNetEventSubscribe4_I(m->FwEngineHandle, &subscription, FwMonEventCallback, this, &m->FwEventHandle) != ERROR_SUCCESS)
            return false;
    }
    else if (WindowsVersion >= WINDOWS_10_RS4)
    {
        if (!m->FwpmNetEventSubscribe3_I)
            return false;

        if (m->FwpmNetEventSubscribe3_I(m->FwEngineHandle, &subscription, (FWPM_NET_EVENT_CALLBACK3)FwMonEventCallback, this, &m->FwEventHandle) != ERROR_SUCCESS)
            return false;
    }
    else if (m->FwpmNetEventSubscribe2_I)
    {
        if (m->FwpmNetEventSubscribe2_I(m->FwEngineHandle, &subscription, (FWPM_NET_EVENT_CALLBACK2)FwMonEventCallback, this, &m->FwEventHandle) != ERROR_SUCCESS)
            return false;
    }
    else if (m->FwpmNetEventSubscribe1_I)
    {
        if (m->FwpmNetEventSubscribe1_I(m->FwEngineHandle, &subscription, (FWPM_NET_EVENT_CALLBACK1)FwMonEventCallback, this, &m->FwEventHandle) != ERROR_SUCCESS) // TODO: Use correct function.
            return false;
    }
    else if (FwpmNetEventSubscribe0 != NULL)
    {
        if (FwpmNetEventSubscribe0(m->FwEngineHandle, &subscription, (FWPM_NET_EVENT_CALLBACK0)FwMonEventCallback, this, &m->FwEventHandle) != ERROR_SUCCESS) // TODO: Use correct function.
            return false;
    }
    else
        return false;
    return true;
}

void CFirewallMonitor::StopFwMonitor()
{
    if (m->FwEventHandle)
    {
        FwpmNetEventUnsubscribe(m->FwEngineHandle, m->FwEventHandle);
        m->FwEventHandle = NULL;
    }

    if (m->FwEngineHandle)
    {
        //FWP_VALUE value = { FWP_EMPTY };
        //value.type = FWP_UINT32;
        //value.uint32 = 0;

        // TODO: return to previous state if other applications require event collection enabled??
        // Disable collection of NetEvents
        //FwpmEngineSetOption(FwEngineHandle, FWPM_ENGINE_COLLECT_NET_EVENTS, &value);

        FwpmEngineClose(m->FwEngineHandle);
        m->FwEngineHandle = NULL;
    }
}

/*void CFirewallMonitor::run()
{
	//if(WindowsVersion >= WINDOWS_10_RS1)
	//	SetThreadDescription(GetCurrentThread(), L"Firewall Monitor");

	//exec();


}*/