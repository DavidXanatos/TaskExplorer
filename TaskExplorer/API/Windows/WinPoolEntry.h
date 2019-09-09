#pragma once
#include <qobject.h>
#include "../AbstractInfo.h"

struct SPoolEntryStats
{
    SDelta64 PagedAllocsDelta;
    SDelta64 PagedFreesDelta;
    SDelta64 PagedCurrentDelta;
    SDelta64 PagedTotalSizeDelta;
    SDelta64 NonPagedAllocsDelta;
    SDelta64 NonPagedFreesDelta;
    SDelta64 NonPagedCurrentDelta;
    SDelta64 NonPagedTotalSizeDelta;
};

class CWinPoolEntry: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CWinPoolEntry(QObject *parent = nullptr);
	virtual ~CWinPoolEntry();

	virtual quint64 GetTagID() const				{ QReadLocker Locker(&m_Mutex); return m_TagName; }
	virtual QString GetTagString() const;
	virtual QString GetDriver() const				{ QReadLocker Locker(&m_Mutex); return m_Driver; }
	virtual QString GetDescription() const			{ QReadLocker Locker(&m_Mutex); return m_Description; }

	virtual SPoolEntryStats GetEntryStats() const	{ QReadLocker Locker(&m_Mutex); return m_EntryStats; }

protected:
	friend class CWindowsAPI;

	bool InitStaticData(quint64 TagName, const QString& Driver, const QString& Description);
	bool UpdateDynamicData(struct _SYSTEM_POOLTAG* pPoolTagInfo);

	quint32			m_TagName;
	QString			m_Driver;
	QString			m_Description;

	SPoolEntryStats	m_EntryStats;
};

typedef QSharedPointer<CWinPoolEntry> CPoolEntryPtr;
typedef QWeakPointer<CWinPoolEntry> CPoolEntryRef;