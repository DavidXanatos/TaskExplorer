#pragma once
#include <qobject.h>
#include "AbstractInfo.h"

#include "ModuleInfo.h"

class CHeapInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CHeapInfo(QObject *parent = nullptr);
	virtual ~CHeapInfo();

	virtual quint32 GetFlags() const { QReadLocker Locker(&m_Mutex); return m_Flags; }
	virtual QString GetFlagsString() const = 0;
	virtual quint32 GetClass() const = 0;
	virtual QString GetClassString() const = 0;
	virtual quint32 GetType() const = 0;
	virtual QString GetTypeString() const = 0;
	virtual quint32 GetNumberOfEntries() const { QReadLocker Locker(&m_Mutex); return m_NumberOfEntries; }
	virtual quint64 GetBaseAddress() const { QReadLocker Locker(&m_Mutex); return m_BaseAddress; }
	virtual quint64 GetBytesAllocated() const { QReadLocker Locker(&m_Mutex); return m_BytesAllocated; }
	virtual quint64 GetBytesCommitted() const { QReadLocker Locker(&m_Mutex); return m_BytesCommitted; }

protected:

    quint32 m_Flags;
    quint32 m_NumberOfEntries;
    quint64 m_BaseAddress;
    quint64 m_BytesAllocated;
    quint64 m_BytesCommitted;
};

typedef QSharedPointer<CHeapInfo> CHeapPtr;
typedef QWeakPointer<CHeapInfo> CHeapRef;