#pragma once
#include "../DriverInfo.h"

class CWinDriver : public CDriverInfo
{
	Q_OBJECT
public:
	CWinDriver(QObject *parent = nullptr);
	virtual ~CWinDriver();

	virtual quint64 GetImageBase() const { QReadLocker Locker(&m_Mutex); return m_ImageBase; }
	virtual quint64 GetImageSize() const { QReadLocker Locker(&m_Mutex); return m_ImageSize; }

public slots:
	void			OnAsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;

	bool			InitStaticData(struct _RTL_PROCESS_MODULE_INFORMATION* Module);
	//bool			UpdateDynamicData();

	quint64			m_ImageBase;
	quint64			m_ImageSize;
};
