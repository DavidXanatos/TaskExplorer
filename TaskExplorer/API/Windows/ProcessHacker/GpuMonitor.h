#pragma once

#include "../../../Common/Common.h"
#include "../WinProcess.h"

class CGpuMonitor : public QObject
{
	Q_OBJECT
public:
	CGpuMonitor(QObject *parent = nullptr);
    virtual ~CGpuMonitor();

	bool		Init();

	static QString QueryDeviceProperty(/*DEVINST*/quint32 DeviceHandle, const /*DEVPROPKEY*/struct _DEVPROPKEY *DeviceProperty);
	static QString QueryDeviceRegistryProperty(/*DEVINST*/quint32 DeviceHandle, ulong DeviceProperty);
	static quint64 QueryGpuInstalledMemory(/*DEVINST*/quint32 DeviceHandle);
	static bool QueryDeviceProperties(wchar_t* DeviceInterface, QString* Description, QString* DriverDate, QString* DriverVersion, QString* LocationInfo, quint64* InstalledMemory);

	static QString GetNodeEngineTypeString(/*D3DKMT_NODEMETADATA**/struct _D3DKMT_NODEMETADATA* NodeMetaData);

	bool		UpdateGpuStats() { return UpdateSystemStats(); }

	bool		UpdateSystemStats();

	bool		UpdateProcessStats(const CProcessPtr& pProcess);

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

	struct SGpuInfo
	{
		SGpuInfo()
		{
		}

		QString DeviceInterface;
		QString Description;
		QStringList NodeNameList;
	};

	virtual int			GetGpuCount() { QReadLocker Locker(&m_StatsMutex); return m_GpuAdapterList.count(); }
	virtual SGpuInfo	GetGpuInfo(int Index);
	virtual float		GetGpuUsage(int Index);
	virtual SGpuMemory	GetGpuMemory(int Index);
	virtual SGpuMemory	GetGpuMemory();

protected:

	bool InitializeD3DStatistics();

	struct SGpuAdapter*	AddDisplayAdapter(wchar_t* DeviceInterface, /*D3DKMT_HANDLE*/quint32 AdapterHandle, /*LUID**/struct _LUID* AdapterLuid, ulong NumberOfSegments, ulong NumberOfNodes);

	void				UpdateProcessSegmentInformation(const CProcessPtr& pProcess);
	void				UpdateSystemSegmentInformation();
	void				UpdateProcessNodeInformation(const CProcessPtr& pProcess);
	void				UpdateSystemNodeInformation();

	
	//quint32				m_GpuTotalNodeCount;
	//quint32				m_GpuTotalSegmentCount;
	//quint32				m_GpuNextNodeIndex;

	//quint64				m_GpuDedicatedLimit;
	//quint64				m_GpuDedicatedUsage;
	//quint64				m_GpuSharedLimit;
	//quint64				m_GpuSharedUsage;

	//float				m_GpuNodeUsage;

	QList<struct SGpuAdapter*> m_GpuAdapterList;

	//QVector<SDelta64>	m_GpuNodesTotalRunningTimeDeltas;
	SDelta64			m_ClockTotalRunningTimeDelta;
	quint64				m_ClockTotalRunningTimeFrequency;

	QReadWriteLock		m_StatsMutex; // todo
};
