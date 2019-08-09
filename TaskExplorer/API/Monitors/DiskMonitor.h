#pragma once

#include "../../Common/Common.h"
#include "../MiscStats.h"

class CDiskMonitor : public QObject
{
	Q_OBJECT
public:
	CDiskMonitor(QObject *parent = nullptr);
	virtual ~CDiskMonitor();

	virtual bool		Init() = 0;

	virtual bool		UpdateDisks() = 0;

	virtual void		UpdateDiskStats() = 0;

	struct SDiskInfo: SIOStats
	{
		SDiskInfo()
		{
			DeviceIndex = ULONG_MAX;
			DevicePresent = false;
			DeviceSupported = true;

			LastStatUpdate = GetCurTick();

			ResponseTime = 0;
			ActiveTime = 0;
			QueueDepth = 0;
			SplitCount = 0;

			TotalSize = 0;

			DiskIndex = ULONG_MAX;
		}

		ulong DeviceIndex;
		bool DevicePresent;
		bool DeviceSupported;
		QString DevicePath;
		QString DeviceName;
		QString DeviceMountPoints;

		quint64	LastStatUpdate;
		//SDelta64 BytesReadDelta;
		//SDelta64 BytesWrittenDelta;
		SDelta64 ReadTimeDelta;
		SDelta64 WriteTimeDelta;
		SDelta64 IdleTimeDelta;
		//SDelta32 ReadCountDelta;
		//SDelta32 WriteCountDelta;
		SDelta64 QueryTimeDelta;

		float ResponseTime;
		float ActiveTime;
		ulong QueueDepth;
		ulong SplitCount;

		quint64 TotalSize;

		ulong DiskIndex;
	};

	virtual QMap<QString, SDiskInfo>	GetAllDiskList() const { QReadLocker Locker(&m_StatsMutex); return m_DiskList; }
	virtual SDiskInfo					GetDiskInfo(const QString& DevicePath) const { QReadLocker Locker(&m_StatsMutex); return m_DiskList.value(DevicePath); }
	virtual QMap<QString, SDiskInfo>	GetDiskList() const;

	struct SDataRates
	{
		SDataRates()
		{
			DiskCount = 0;
			Supported = 0;

			ReadRate = 0;
			WriteRate = 0;
		}

		int DiskCount;
		int Supported;

		quint64 ReadRate;
		quint64 WriteRate;
	};

	virtual SDataRates GetAllDiskDataRates() const;

protected:
	QMap<QString, SDiskInfo>	m_DiskList;

	mutable QReadWriteLock		m_StatsMutex;
};