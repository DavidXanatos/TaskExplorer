#include "stdafx.h"
#include "ProcessInfo.h"
#include "SocketInfo.h"
#include "SystemAPI.h"
#include "../../MiscHelpers/Common/Settings.h"


SProcessUID::SProcessUID(quint64 uPid, quint64 msTime)
{
	//
	// Note: On Windows PID's are 32-bit and word aligned i.e. the least significant 2 bits are always 0 hence we can drop them
	//

	// Variant A
	//
	// 11111111 11111111 1111 1111 111111 00                                     - PID
	// 00000000 00000000 0000(0000 000000)00 00000000 00000000 00000000 00000000 - uint64 - PID (shared msb bits) timestamp in miliseconds
	//					      1111 111111 11 11111111 11111111 11111111 11111111 - unix timestamp (Jun 2527)
	//                           1 100101 00 00100110 01000000 01010010 01111101 - unix timestamp (Jan 2025)
	//
	// rev_PID_LSB       (rev_PID_MSB Time_MSB)                         Time_LSB
	//
	//PUID = reverseBits64(((uPid & 0xFFFFFFFC) >> 2)) ^ (msTime & 0x00000FFFFFFFFFFF);

	// Variant B
	//
	// 11111111 11111111 11111111 11111100                                     - PID
	// 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 - uint64 - 30 bit Pid, 34 bit timestamp in seconds
	//                                  11 11111111 11111111 11111111 11111111 - unix timestamp (May 2514)
	//										1100111 01110110 01010110 00011001 - unix timestamp (Jan 2025)
	//
	//PUID = ((uPid & 0xFFFFFFFC) << 32) | ((msTime/1000llu) & 0x00000003FFFFFFFF);

	// Variant C
	//
	// 11111111 11111111 11111100                                              - PID
	// 00000000 00000000 00000000 00000000 00000000 00000000 00000000 00000000 - uint64 - 22 bit Pid, 42 bit timestamp in seconds
	//                         11 11111111 11111111 11111111 11111111 11111111 - unix timestamp (May 2109) - 3FFFFFFFFFF - 4398046511103
	//                         01 10010100 00100110 01000000 01010010 01111101 - unix timestamp (Jan 2025)
	//
	PUID = (((uPid) << 40) & 0xFFFFFC0000000000) ^ (msTime & 0x000003FFFFFFFFFF);

	// Variant C seams best, 2109 is far enough in the future and we have no slow integer division, even though the PID gets truncated a bit
}



CProcessInfo::CProcessInfo(QObject *parent) : CAbstractTask(parent)
{
	// Basic
	m_ProcessId = -1;
	m_ParentProcessId = -1;

	// Dynamic
	m_NumberOfThreads = 0;
	m_NumberOfHandles = 0;

	m_PeakNumberOfThreads = 0;

	m_PeakPagefileUsage = 0;
	m_WorkingSetSize = 0;
	m_PeakWorkingSetSize = 0;
	m_WorkingSetPrivateSize = 0;
	m_VirtualSize = 0;
	m_PeakVirtualSize = 0;
	//m_PageFaultCount = 0;

	m_NetworkUsageFlags = 0;

	m_DebugMessageCount = 0;
}

CProcessInfo::~CProcessInfo()
{
}

QString CProcessInfo::GetNetworkUsageString() const
{
	QReadLocker Locker(&m_StatsMutex);
	QStringList NetworkUsage;
	if (m_NetworkUsageFlags & NET_TYPE_PROTOCOL_TCP_SRV)
		NetworkUsage.append(tr("TCP/Server"));
	else if (m_NetworkUsageFlags & NET_TYPE_PROTOCOL_TCP)
		NetworkUsage.append(tr("TCP"));
	if (m_NetworkUsageFlags & NET_TYPE_PROTOCOL_UDP)
		NetworkUsage.append(tr("UDP"));
	return NetworkUsage.join(", ");
}

void CProcessInfo::UpdateDns(const QString& HostName, const QList<QHostAddress>& Addresses)
{
	QWriteLocker Locker(&m_DnsMutex);

	CDnsLogEntryPtr& DnsEntry = m_DnsLog[HostName];
	QList<QHostAddress> NewAddresses;
	if (DnsEntry.isNull())
	{
		DnsEntry = CDnsLogEntryPtr(new CDnsLogEntry(HostName, Addresses));
		NewAddresses = Addresses;
	}
	else
		NewAddresses = DnsEntry->UpdateAddresses(Addresses);

	foreach(const QHostAddress& Address, NewAddresses)
		m_DnsRevLog.insertMulti(Address, HostName);
}

QString CProcessInfo::GetHostName(const QHostAddress& Address)
{
	QList<QString> HostNames = m_DnsRevLog.values(Address);
	return HostNames.join("; ");
}

void CProcessInfo::ClearPersistence()
{
	CAbstractTask::ClearPersistence();

	foreach(const CThreadPtr& pThread, GetThreadList())
		pThread->ClearPersistence();
		
	foreach(const CHandlePtr& pHandle, GetHandleList())
		pHandle->ClearPersistence();
}

void CProcessInfo::UpdatePresets()
{
	QWriteLocker Locker(&m_Mutex); 
	m_PersistentPreset.clear();
	InitPresets();
}

void CProcessInfo::InitPresets()
{
	m_PersistentPreset = theAPI->FindPersistentPreset(m_FileName, m_CommandLine);
	if (!m_PersistentPreset.isNull())
		QTimer::singleShot(0, this, SLOT(ApplyPresets()));
}

CPersistentPresetPtr CProcessInfo::GetPresets() const
{
	QReadLocker Locker(&m_Mutex); 
	return m_PersistentPreset;
}

void CProcessInfo::ApplyPresets()
{
	CPersistentPresetPtr PersistentPreset = GetPresets();
	if (PersistentPreset.isNull())
		return;
	CPersistentPresetDataPtr Preset = PersistentPreset->GetData();

	if (Preset->bTerminate) {
		Terminate(true);
		return;
	}

	if (Preset->bPriority)
		SetPriority(Preset->iPriority);
	if (Preset->bAffinity)
		SetAffinityMask(Preset->uAffinity);
	if (Preset->bIOPriority)
		SetIOPriority(Preset->iIOPriority);
	if (Preset->bPagePriority)
		SetPagePriority(Preset->iPagePriority);
}

void CProcessInfo::AddDebugMessage(const QString& Text, const QDateTime& TimeStamp)
{
	QWriteLocker Locker(&m_DebugMutex);

	int AddedCount = 0;

	QStringList Texts = Text.split("\n");

	if (m_DebugMessages.count() > 0)
	{
		SDebugMessage& Message = m_DebugMessages.last();
		if (Message.Text.right(1) != '\n')
		{
			Message.Text += Texts.takeFirst();
			if (!Texts.isEmpty())
				Message.Text += "\n";
		}
	}
	
	for(int i=0; i < Texts.count();)
	{
		QString Text = Texts[i++];
		bool Last = i >= Texts.count();
		if (Last && Text.isEmpty())
			break;

		SDebugMessage Message = { TimeStamp, Text };
		if (!Last)
			Message.Text += "\n";
		m_DebugMessages.append(Message);
		AddedCount++;
	}

	while (m_DebugMessages.count() > theConf->GetInt("Options/MaxDebugLog", 1000))
		m_DebugMessages.removeFirst();

	m_DebugMessageCount += AddedCount;
}


QList<CProcessInfo::SDebugMessage> CProcessInfo::GetDebugMessages(quint32* pDebugMessageCount) const 
{ 
	QReadLocker Locker(&m_DebugMutex);  
	if (pDebugMessageCount)
		*pDebugMessageCount = m_DebugMessageCount;
	return m_DebugMessages; 
}

void CProcessInfo::ClearDebugMessages()
{
	QReadLocker Locker(&m_DebugMutex);  
	m_DebugMessages.clear();
	m_DebugMessageCount = 0;
}