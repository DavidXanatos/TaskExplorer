
#include "stdafx.h"
#include "FwEventMonitor.h"
#include "../../../../MiscHelpers/Common/Common.h"
#include "../../../../MiscHelpers/Common/Settings.h"
#include "../WindowsAPI.h"

#include <windows.h>
#include <winevt.h>
#include <winmeta.h>
#include <NTSecAPI.h>

// defined in NTSecAPI
static GUID Audit_ObjectAccess_FirewallPacketDrops_ = { 0x0cce9225, 0x69ae, 0x11d9, { 0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };
static GUID Audit_ObjectAccess_FirewallConnection_ = { 0x0cce9226, 0x69ae, 0x11d9, { 0xbe, 0xd3, 0x50, 0x50, 0x54, 0x50, 0x30, 0x30 } };

#define AUDIT_POLICY_INFORMATION_TYPE_SUCCESS 1
#define AUDIT_POLICY_INFORMATION_TYPE_FAILURE 2

#pragma comment(lib, "wevtapi.lib")

#define WIN_LOG_EVENT_FW_BLOCKED 5157
#define WIN_LOG_EVENT_FW_ALLOWED 5156

#define WIN_LOG_EVENT_FW_OUTBOUN 1
#define WIN_LOG_EVENT_FW_INBOUND 2

struct SFwEventMonitor
{
	SFwEventMonitor()
	{
		hSubscription = NULL;
	}

	EVT_HANDLE hSubscription;
};

CFwEventMonitor::CFwEventMonitor(QObject *parent) : QObject(parent) //QThread(parent)
{
	m_bRunning = false;

	m = new SFwEventMonitor();
}

bool CFwEventMonitor::Init()
{
	// start thread
	//m_bRunning = true;
	//start();

	m_bRunning = StartLogMonitor();
	
	return m_bRunning;
}

CFwEventMonitor::~CFwEventMonitor()
{
	//quit();

	/*m_bRunning = false;

	if (!wait(5 * 1000))
		terminate();*/

	if (m_bRunning)
		StopLogMonitor();

	delete m;
}

QString GetWinVariantString(const EVT_VARIANT& value, const QString& default = QString())
{
	if (value.Type == EvtVarTypeString && value.StringVal != NULL)
		return QString::fromWCharArray(value.StringVal);
	if (value.Type == EvtVarTypeAnsiString && value.StringVal != NULL)
		return QString(value.AnsiStringVal);
	return default;
}

template <class T>
T GetWinVariantNumber(const EVT_VARIANT& value, T default = 0)
{
	switch (value.Type)
	{
	case EvtVarTypeBoolean:	return value.BooleanVal;

	case EvtVarTypeSByte:	return value.SByteVal;
    case EvtVarTypeByte:	return value.ByteVal;
    case EvtVarTypeInt16:	return value.Int16Val;
    case EvtVarTypeUInt16:	return value.UInt16Val;
    case EvtVarTypeInt32:	return value.Int32Val;
    case EvtVarTypeUInt32:	return value.UInt32Val;
    case EvtVarTypeInt64:	return value.Int64Val;
    case EvtVarTypeUInt64:	return value.UInt64Val;

	case EvtVarTypeFileTime:return value.FileTimeVal; // UInt64Val

    case EvtVarTypeSingle:	return value.SingleVal;
    case EvtVarTypeDouble:	return value.DoubleVal;
	}
	return default;
}

DWORD CALLBACK CFwEventMonitor__Callback(EVT_SUBSCRIBE_NOTIFY_ACTION action, PVOID pContext, EVT_HANDLE hEvent) 
{
	CFwEventMonitor* This = (CFwEventMonitor*)pContext;

	/*// Get the entire event as XML
	WCHAR msg[4096];
	DWORD reqSize, propCount = 0;
	if (!EvtRender(NULL, hEvent, EvtRenderEventXml, ARRSIZE(msg), msg, &reqSize, &propCount))
	{
		qDebug() << QString::fromWCharArray(msg);
		return 0;
	}*/

	/*
<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
    <System>
        <Provider Name="Microsoft-Windows-Security-Auditing" Guid="{54849625-5478-4994-a5ba-3e3b0328c30d}" />
        <EventID>5157</EventID>
        <Version>1</Version>
        <Level>0</Level>
        <Task>12810</Task>
        <Opcode>0</Opcode>
        <Keywords>0x8010000000000000</Keywords>
        <TimeCreated SystemTime="2019-08-17T19:38:51.674547600Z" />
        <EventRecordID>38795210</EventRecordID>
        <Correlation />
        <Execution ProcessID="4" ThreadID="27028" />
        <Channel>Security</Channel>
        <Computer>Edison</Computer>
        <Security />
    </System>
    <EventData>
        <Data Name="ProcessID">23352</Data>
        <Data Name="Application">\device\harddiskvolume7\program files (x86)\microsoft visual studio\2017\enterprise\vc\tools\msvc\14.16.27023\bin\hostx86\x64\vctip.exe</Data>
        <Data Name="Direction">%%14593</Data>
        <Data Name="SourceAddress">10.70.0.12</Data>
        <Data Name="SourcePort">60237</Data>
        <Data Name="DestAddress">152.199.19.161</Data>
        <Data Name="DestPort">443</Data>
        <Data Name="Protocol">6</Data>
        <Data Name="FilterRTID">226530</Data>
        <Data Name="LayerName">%%14611</Data>
        <Data Name="LayerRTID">48</Data>
        <Data Name="RemoteUserID">S-1-0-0</Data>
        <Data Name="RemoteMachineID">S-1-0-0</Data>
    </EventData>
</Event>
	*/

	int Type = EventTypeUnknow;
	QString strDirection;
	int Direction = -1;

	quint64 ProcessId = -1;
	QString ProcessFileName;

	quint32 Protocol = 0;
	QString SourceAddress;
	QString SourcePort;
	QString DestAddress;
	QString DestPort;
	//quint32 LayerRTID = 0;

	quint32 ProtocolType;
	QHostAddress LocalAddress;
	quint16 LocalPort = 0;
	QHostAddress RemoteAddress;
	quint16 RemotePort = 0;


	// Create the render context 
	static PCWSTR eventProperties[] = { 
		L"Event/System/Provider/@Name",		// 0
		L"Event/System/EventID",			// 1
		L"Event/System/Level",				// 2
		L"Event/System/Keywords",			// 3

		L"Event/EventData/Data[@Name='ProcessID']",		// 4
		L"Event/EventData/Data[@Name='Application']",	// 5
		L"Event/EventData/Data[@Name='Direction']",		// 6
		L"Event/EventData/Data[@Name='SourceAddress']",	// 7
		L"Event/EventData/Data[@Name='SourcePort']",	// 8
		L"Event/EventData/Data[@Name='DestAddress']",	// 9
		L"Event/EventData/Data[@Name='DestPort']",		// 10
		L"Event/EventData/Data[@Name='Protocol']",		// 11
		L"Event/EventData/Data[@Name='LayerRTID']"		// 12

		/*L"Event/EventData/Data[@Name='FilterRTID']",		// 13
		L"Event/EventData/Data[@Name='LayerName']",			// 14
		L"Event/EventData/Data[@Name='RemoteUserID']",		// 15
		L"Event/EventData/Data[@Name='RemoteMachineID']"*/	// 16
	};
	EVT_HANDLE renderContext = EvtCreateRenderContext(ARRSIZE(eventProperties), eventProperties, EvtRenderContextValues);
	//EVT_HANDLE renderContext = EvtCreateRenderContext(0, NULL , EvtRenderContextSystem);
	//EVT_HANDLE renderContext = EvtCreateRenderContext(0, NULL , EvtRenderContextUser);
	if (renderContext == NULL)
	{
		/*
		wchar_t* lpMsgBuf;
		wchar_t* lpDisplayBuf;
		DWORD dw = GetLastError(); 
		FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, dw, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR) &lpMsgBuf, 0, NULL);
		qDebug() << "CFwEventMonitor: Call to EvtCreateRenderContext failed:" << QString::fromWCharArray(lpMsgBuf);
		LocalFree(lpMsgBuf);
		LocalFree(lpDisplayBuf);
		*/
		qDebug() << "CFwEventMonitor: Call to EvtCreateRenderContext failed:" << (quint32)GetLastError();
		return 0;
	}

	// Get event values
	DWORD buffSize = 4096;
	void* buffer = malloc(4096);
	DWORD reqSize = 0;
	DWORD propCount = 0;
	while (!EvtRender(renderContext, hEvent, EvtRenderEventValues, buffSize, buffer, &reqSize, &propCount))
	{
		DWORD error = GetLastError();
		if (error != ERROR_INSUFFICIENT_BUFFER || buffSize > reqSize)
		{
			qDebug() << "CFwEventMonitor: Call to EvtRender failed:" << (quint32)error;
			goto cleanup;
		}

		free(buffer);
		buffSize = reqSize + 128;
		buffer = malloc(buffSize);
	}

	PEVT_VARIANT values = PEVT_VARIANT(buffer);


	////////////////////////////////
	// Evaluate the event values

	// Publisher name
	//qDebug() << GetWinVariantString(values[0]);

	// Event ID
	uint16_t EventId = GetWinVariantNumber(values[1], (uint16_t)0);
	switch (EventId)
	{
	case WIN_LOG_EVENT_FW_BLOCKED:	Type = EvlFirewallBlocked; break;
	case WIN_LOG_EVENT_FW_ALLOWED:	Type = EvlFirewallAllowed; break;
	}

	/*
	// Severity level
	uint16_t Level = GetWinVariantNumber(values[2], (byte)0);
	switch(values[2].ByteVal)
	{
		case WINEVENT_LEVEL_CRITICAL:
		case WINEVENT_LEVEL_ERROR:			Level = EVENTLOG_ERROR_TYPE;	break;
		case WINEVENT_LEVEL_WARNING :		Level = EVENTLOG_WARNING_TYPE;	break;
		case WINEVENT_LEVEL_INFO:
		case WINEVENT_LEVEL_VERBOSE:
		default:							Level = EVENTLOG_INFORMATION_TYPE;
	}

	// 
	uint64_t Keywords = GetWinVariantNumber(values[3], (uint64_t)0);
	if (Keywords & WINEVENT_KEYWORD_AUDIT_SUCCESS)
		//Level = EVENTLOG_AUDIT_SUCCESS;
		Type = EvlFirewallAllowed;
	else if (Keywords & WINEVENT_KEYWORD_AUDIT_FAILURE)
		//Level = EVENTLOG_AUDIT_FAILURE;
		Type = EvlFirewallBlocked;
	*/

	// Direction
	strDirection = GetWinVariantString(values[6]);
	if (strDirection == "%%14592")
		Direction = WIN_LOG_EVENT_FW_INBOUND;
	else if (strDirection == "%%14593")
		Direction = WIN_LOG_EVENT_FW_OUTBOUN;
	else
		qDebug() << "CFwEventMonitor: Unknown direction:" << strDirection;
	
	ProcessId = GetWinVariantNumber(values[4], (uint64_t)-1);
	ProcessFileName = GetWinVariantString(values[5]);


	Protocol = GetWinVariantNumber(values[11], (uint32_t)0);
	SourceAddress = GetWinVariantString(values[7]);
	SourcePort = GetWinVariantString(values[8]);
	DestAddress = GetWinVariantString(values[9]);
	DestPort = GetWinVariantString(values[10]);
	//LayerRTID = GetWinVariantNumber(values[12], (uint32_t)0);

	if (Direction == WIN_LOG_EVENT_FW_OUTBOUN)
	{
		LocalAddress = QHostAddress(SourceAddress);
		LocalPort = SourcePort.toUInt();
		RemoteAddress = QHostAddress(DestAddress);
		RemotePort = DestPort.toUInt();
	}
	else if (Direction == WIN_LOG_EVENT_FW_INBOUND)
	{
		LocalAddress = QHostAddress(DestAddress);
		LocalPort = DestPort.toUInt();
		RemoteAddress = QHostAddress(SourceAddress);
		RemotePort = SourcePort.toUInt();
	}

	if (Protocol == IPPROTO_TCP) // TCP
		ProtocolType = NET_TYPE_PROTOCOL_TCP;
	else if (Protocol == IPPROTO_UDP) // UDP
		ProtocolType = NET_TYPE_PROTOCOL_UDP;

	if(LocalAddress.protocol() == QAbstractSocket::IPv4Protocol)
		ProtocolType |= NET_TYPE_NETWORK_IPV4;
	else if(LocalAddress.protocol() == QAbstractSocket::IPv6Protocol)
		ProtocolType |= NET_TYPE_NETWORK_IPV6;

	/*qDebug() << ((Type == EvlFirewallBlocked) ? "Blocked" : "Allowed") << ((Direction == WIN_LOG_EVENT_FW_INBOUND) ? "Inbound" : "Outbound")
		<< "; Src:" << SourceAddress << SourcePort << "; Dest:" << DestAddress << DestPort
		//<< "; Layer" << LayerRTID
		<< ProcessFileName;*/

	emit This->NetworkEvent(Type, ProcessId, -1, ProtocolType, 0, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	////////////////////////////////
	// cleanup

cleanup:
	EvtClose(renderContext);
	if (buffer)
		free(buffer);
	return 0;
}

bool CFwEventMonitor::StartLogMonitor()
{
	PAUDIT_POLICY_INFORMATION AuditPolicyInformation;
	BOOLEAN success = AuditQuerySystemPolicy(&Audit_ObjectAccess_FirewallConnection_, 1, &AuditPolicyInformation);
	if (success)
	{
		int AuditingMode = AUDIT_POLICY_INFORMATION_TYPE_FAILURE;	// blocked connections
		if (theConf->GetInt("Options/FwAuditingFull", false))
			AuditingMode |= AUDIT_POLICY_INFORMATION_TYPE_SUCCESS;	// allowed connections

		if ((AuditPolicyInformation->AuditingInformation & AuditingMode) != AuditingMode)
		{
			int OriginalAuditingMode = theConf->GetInt("Options/FwAuditingModeOld", -1);
			if (OriginalAuditingMode == -1) // check if its not already set
				theConf->SetValue("Options/FwAuditingModeOld", (int)AuditPolicyInformation->AuditingInformation);

			AuditPolicyInformation->AuditingInformation |= AuditingMode; // just enable additional modes

			success = AuditSetSystemPolicy(AuditPolicyInformation, 1);
		}

		AuditFree(AuditPolicyInformation);
	}

	if (!success)
	{
		qDebug() << "Failed to configure the auditing policy";
		return false;
	}

	// *[System[Provider[ @Name='%2'] and (EventID=%3)]]
	// Note: 
	//			Alowed connections LayerRTID == 48 
	//			Blocked connections LayerRTID == 44
	//			Opening a TCP port for listening LayerRTID == 38 and 36 // Resource allocation
	//			Opening a UDP port LayerRTID == 38 and 36 // Resource allocation
	quint32 LayerRTID = 44;
	QString Query = QString("*[System[(Level=4 or Level=0) and (EventID=" STR(WIN_LOG_EVENT_FW_BLOCKED) " or EventID=" STR(WIN_LOG_EVENT_FW_ALLOWED) ")]] and *[EventData[Data[@Name='LayerRTID']>='%1']]").arg(LayerRTID);
	QString XmlQuery = QString("<QueryList><Query Id=\"0\" Path=\"%1\"><Select Path=\"%1\">%2</Select></Query></QueryList>").arg("Security").arg(Query);

	m->hSubscription = EvtSubscribe(NULL, NULL, L"", XmlQuery.toStdWString().c_str(), NULL, this, (EVT_SUBSCRIBE_CALLBACK)CFwEventMonitor__Callback, EvtSubscribeToFutureEvents);
	if (m->hSubscription == NULL)
	{
		DWORD status = GetLastError();

		if (ERROR_EVT_CHANNEL_NOT_FOUND == status)
			qDebug() << "Channel  was not found.";
		else if (ERROR_EVT_INVALID_QUERY == status)
			// You can call EvtGetExtendedStatus to get information as to why the query is not valid.
			qDebug() << "The query is not valid.";
		else
			qDebug() << "EvtSubscribe failed with" << (quint32)status << ".";

		return false;
	}

	return true;
}

void CFwEventMonitor::StopLogMonitor()
{
	if (m->hSubscription)
		EvtClose(m->hSubscription);

	int OriginalAuditingMode = theConf->GetInt("Options/FwAuditingModeOld", -1);
	if (OriginalAuditingMode != -1)
	{
		PAUDIT_POLICY_INFORMATION AuditPolicyInformation;
		if (AuditQuerySystemPolicy(&Audit_ObjectAccess_FirewallConnection_, 1, &AuditPolicyInformation))
		{
			AuditPolicyInformation->AuditingInformation = OriginalAuditingMode; // restore original auditing mode

			AuditSetSystemPolicy(AuditPolicyInformation, 1);

			AuditFree(AuditPolicyInformation);
		}
	}
}

/*void CFwEventMonitor::run()
{
	//if(WindowsVersion >= WINDOWS_10_RS1)
	//	SetThreadDescription(GetCurrentThread(), L"Firewall Monitor");

	//exec();

}*/
