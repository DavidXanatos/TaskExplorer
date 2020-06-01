#pragma once

#include "../../../MiscHelpers/Common/Common.h"
#include "../MiscStats.h"

class CNetMonitor : public QObject
{
	Q_OBJECT
public:
	CNetMonitor(QObject *parent = nullptr);
	virtual ~CNetMonitor();

	virtual bool		Init() = 0;

	virtual bool		UpdateAdapters() = 0;

	virtual void		UpdateNetStats() = 0;

	struct SNicInfo: SNetStats
	{
		enum ELinkState
		{
			eUnknown,
			eConnected,
			eDisconnected
		};

		SNicInfo()
		{
			DevicePresent = false;
			DeviceLuid = 0;
			DeviceIndex = 0;

			DeviceSupported = 2;
			LinkSpeed = 0;
			LinkState = eUnknown;

			IsRAS = false;

			LastStatUpdate = GetCurTick();
		}

		bool DevicePresent;
		quint64 DeviceLuid;
		quint32 DeviceIndex;
		QString DeviceGuid;
		QString DeviceName;
		QString DeviceInterface;

		int DeviceSupported;
		quint64	LinkSpeed;
		ELinkState LinkState;

		bool IsRAS;

		quint64	LastStatUpdate;

		QList<QHostAddress> Addresses;
		QList<QHostAddress> NetMasks;
		QList<QHostAddress> Gateways;
		QList<QHostAddress> DNS;
		QSet<QString> Domains;
	};

	virtual QMap<QString, SNicInfo>	GetAllNicList() const { QReadLocker Locker(&m_StatsMutex); return m_NicList; }
	virtual QMap<QString, SNicInfo>	GetNicList(bool bFilterDisconnected = true) const;
	virtual SNicInfo GetNicInfo(const QString& DeviceInterface) const { QReadLocker Locker(&m_StatsMutex); return m_NicList.value(DeviceInterface); }
	virtual bool IsLanIP(const QHostAddress& Address) const;

	struct SDataRates
	{
		SDataRates()
		{
			ReceiveRate = 0;
			SendRate = 0;
		}

		quint64 ReceiveRate;
		quint64 SendRate;
	};

	enum ERates
	{
		eNet = 0,
		eRas,
		eAll
	};

	virtual SDataRates GetTotalDataRate(ERates RAS) const;

protected:
	
	QMap<QString, SNicInfo>		m_NicList;

	mutable QReadWriteLock		m_StatsMutex;
};