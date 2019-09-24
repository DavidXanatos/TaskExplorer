#include "stdafx.h"
#include "ProcessInfo.h"
#include "SocketInfo.h"


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

	m_GpuDedicatedUsage = 0;
	m_GpuSharedUsage = 0;

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