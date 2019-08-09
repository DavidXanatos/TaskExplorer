#include "stdafx.h"
#include "NetMonitor.h"

CNetMonitor::CNetMonitor(QObject *parent)
	: QObject(parent)
{
}

CNetMonitor::~CNetMonitor()
{
}


QMap<QString, CNetMonitor::SNicInfo> CNetMonitor::GetNicList(bool bFilterDisconnected) const 
{ 
	QReadLocker Locker(&m_StatsMutex);

	QMap<QString, SNicInfo> NicList;
	for (QMap<QString, SNicInfo>::const_iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		const SNicInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;
		
		if(bFilterDisconnected && entry->LinkState == SNicInfo::eDisconnected)
			continue;

		NicList.insert(I.key(), I.value());
	}
	return NicList;
}

CNetMonitor::SDataRates CNetMonitor::GetTotalDataRate(ERates RAS) const
{
	QReadLocker Locker(&m_StatsMutex);

	SDataRates DataRates;

	for (QMap<QString, SNicInfo>::const_iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		const SNicInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		if (RAS != eAll && (RAS == eRas) != entry->IsRAS)
			continue;

		DataRates.ReceiveRate += entry->ReceiveRate.Get();
		DataRates.SendRate += entry->SendRate.Get();
	}

	return DataRates;
}