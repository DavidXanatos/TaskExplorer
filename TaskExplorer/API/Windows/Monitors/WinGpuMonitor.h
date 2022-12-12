#pragma once

#include "../../Monitors/GpuMonitor.h"

class CWinGpuMonitor : public CGpuMonitor
{
	Q_OBJECT
public:
	CWinGpuMonitor(QObject *parent = nullptr);
    virtual ~CWinGpuMonitor();

	virtual bool		Init();

	virtual bool		UpdateAdapters();

	static QString QueryDeviceProperty(/*DEVINST*/quint32 DeviceHandle, const /*DEVPROPKEY*/struct _DEVPROPKEY *DeviceProperty);
	static QString QueryDeviceRegistryProperty(/*DEVINST*/quint32 DeviceHandle, quint32 DeviceProperty);
	static quint64 QueryGpuInstalledMemory(/*DEVINST*/quint32 DeviceHandle);
	static bool QueryDeviceProperties(const wchar_t* DeviceInterface, QString* Description, QString* DriverDate, QString* DriverVersion, QString* LocationInfo, quint64* InstalledMemory);

	static QString GetNodeEngineTypeString(/*D3DKMT_NODEMETADATA**/struct _D3DKMT_NODEMETADATA* NodeMetaData);

	virtual bool		UpdateGpuStats();

	virtual QMap<QString, CGpuMonitor::SGpuInfo> GetAllGpuList();
	virtual SGpuMemory			GetGpuMemory();

protected:

	QMap<QString, struct SGpuAdapter*> GetAdapterList() { QReadLocker Locker(&m_StatsMutex); return m_GpuAdapterList; }

	struct SGpuAdapter*	AddDisplayAdapter(const wchar_t* DeviceInterface, /*D3DKMT_HANDLE*/quint32 AdapterHandle, /*LUID**/struct _LUID* AdapterLuid, quint32 NumberOfSegments, quint32 NumberOfNodes);

	void				UpdateSystemStats();
	void				UpdateProcessStats(const CProcessPtr& pProcess, quint64 elapsedTime);
	
	bool				m_UseCounters;

	
	//quint32				m_GpuTotalNodeCount;
	//quint32				m_GpuTotalSegmentCount;
	//quint32				m_GpuNextNodeIndex;

	//quint64				m_GpuDedicatedLimit;
	//quint64				m_GpuDedicatedUsage;
	//quint64				m_GpuSharedLimit;
	//quint64				m_GpuSharedUsage;

	//float				m_GpuNodeUsage;

	QMap<QString, struct SGpuAdapter*> m_GpuAdapterList;

	SDelta64			m_ClockTotalRunningTimeDelta;
	quint64				m_ClockTotalRunningTimeFrequency;

	mutable QReadWriteLock		m_StatsMutex;
};
