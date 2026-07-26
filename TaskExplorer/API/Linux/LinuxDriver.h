#pragma once
#include "../DriverInfo.h"

//
// A loaded kernel module, from /proc/modules and /sys/module/<name>.
//
class CLinuxDriver : public CDriverInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxDriver)
public:
	CLinuxDriver(QObject *parent = nullptr);
	virtual ~CLinuxDriver();

	virtual bool			InitStaticData(const QString& Name);
	virtual bool			UpdateDynamicData(quint64 Size, quint32 RefCount, const QString& UsedBy, const QString& State);

	virtual QString			GetName() const		{ QReadLocker Locker(&m_Mutex); return m_Name; }
	virtual quint64			GetSize() const		{ QReadLocker Locker(&m_Mutex); return m_Size; }
	virtual quint32			GetRefCount() const	{ QReadLocker Locker(&m_Mutex); return m_RefCount; }
	virtual QString			GetUsedBy() const	{ QReadLocker Locker(&m_Mutex); return m_UsedBy; }
	virtual QString			GetStateString() const	{ QReadLocker Locker(&m_Mutex); return m_State; }

protected:
	QString					m_Name;
	quint64					m_Size;
	quint32					m_RefCount;
	QString					m_UsedBy;	// dependent modules
	QString					m_State;	// Live / Loading / Unloading
};

typedef QSharedPointer<CLinuxDriver> CLinuxDriverPtr;
typedef QWeakPointer<CLinuxDriver> CLinuxDriverRef;
