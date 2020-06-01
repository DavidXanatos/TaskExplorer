#include "stdafx.h"
#include "ProcessInfo.h"
#include "SocketInfo.h"
#include "SystemAPI.h"
#include "../../MiscHelpers/Common/Settings.h"

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