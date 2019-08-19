
#include "stdafx.h"
#include "DiskMonitor.h"

CDiskMonitor::CDiskMonitor(QObject *parent)
	: QObject(parent)
{
}

CDiskMonitor::~CDiskMonitor()
{
}


QMap<QString, CDiskMonitor::SDiskInfo> CDiskMonitor::GetDiskList() const 
{ 
	QReadLocker Locker(&m_StatsMutex);

	QMap<QString, SDiskInfo> DiskList;
	for (QMap<QString, SDiskInfo>::const_iterator I = m_DiskList.begin(); I != m_DiskList.end(); I++)
	{
		const SDiskInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		if(entry->TotalSize == 0)
			continue;

		DiskList.insert(I.key(), I.value());
	}
	return DiskList;
}

CDiskMonitor::SDataRates CDiskMonitor::GetAllDiskDataRates() const
{
	QReadLocker Locker(&m_StatsMutex);

	SDataRates DataRates;

	for (QMap<QString, SDiskInfo>::const_iterator I = m_DiskList.begin(); I != m_DiskList.end(); I++)
	{
		const SDiskInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		if(entry->TotalSize == 0)
			continue;

		DataRates.DiskCount++;

		if (!entry->DeviceSupported)
			continue;

		DataRates.Supported++;

		DataRates.ReadRate += entry->ReadRate.Get();
		DataRates.WriteRate += entry->WriteRate.Get();
	}

	return DataRates;
}

bool CDiskMonitor::AllDisksSupported() const
{
	QReadLocker Locker(&m_StatsMutex);

	for (QMap<QString, SDiskInfo>::const_iterator I = m_DiskList.begin(); I != m_DiskList.end(); I++)
	{
		const SDiskInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		if(entry->TotalSize == 0)
			continue;

		if (!entry->DeviceSupported)
			return false;
	}

	return true;
}