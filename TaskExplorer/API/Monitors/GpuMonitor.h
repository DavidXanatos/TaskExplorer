#pragma once

#include "../../../MiscHelpers/Common/Common.h"
#include "../ProcessInfo.h"

class CGpuMonitor : public QObject
{
	Q_OBJECT
public:
	CGpuMonitor(QObject *parent = nullptr);
    virtual ~CGpuMonitor();

	virtual bool		Init() = 0;

	virtual bool		UpdateAdapters() = 0;

	virtual bool		UpdateGpuStats() = 0;

	struct SGpuMemory
	{
		SGpuMemory()
		{
			DedicatedLimit = 0;
			DedicatedUsage = 0;
			SharedLimit = 0;
			SharedUsage = 0;
		}

		quint64 DedicatedLimit;
		quint64 DedicatedUsage;
		quint64	SharedLimit;
		quint64	SharedUsage;
	};

	struct SGpuNode
	{
		SGpuNode(const QString& name = QString())
		{
			Name = name;
			TimeUsage = 0;
		}
		QString Name;
		float TimeUsage;
	};

	struct SGpuInfo
	{
		SGpuInfo()
		{
			InstalledMemory = 0;
			VendorID = 0;
			DeviceID = 0;

			TimeUsage = 0;
		}

		QString DeviceInterface;
		QString Description;
		QString DriverDate;
		QString DriverVersion;
		QString LocationInfo;
		quint64 InstalledMemory;
		//QString WDDMVersion;
		quint32 VendorID;
		quint32 DeviceID;

		float TimeUsage;

		SGpuMemory Memory;
		QList<SGpuNode> Nodes;
	};

	virtual QMap<QString, SGpuInfo>	GetAllGpuList() = 0;
	virtual SGpuMemory				GetGpuMemory() = 0;

protected:

	mutable QReadWriteLock		m_StatsMutex;
};
