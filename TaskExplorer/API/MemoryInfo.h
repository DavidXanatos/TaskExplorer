#pragma once
#include <qobject.h>
#include "AbstractInfo.h"
#include "..\Common\FlexError.h"

class CMemoryInfo: public CAbstractInfoEx
{
	Q_OBJECT

public:
	CMemoryInfo(QObject *parent = nullptr);
	virtual ~CMemoryInfo();

	virtual quint64 GetProcessId() const			{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual quint64 GetBaseAddress() const			{ QReadLocker Locker(&m_Mutex); return m_BaseAddress; }
	virtual quint64 GetAllocationBase() const		{ QReadLocker Locker(&m_Mutex); return m_AllocationBaseItem.isNull() ? 0 : m_AllocationBaseItem.objectCast<CMemoryInfo>()->GetBaseAddress(); }
	virtual bool IsAllocationBase() const;
	virtual QString GetTypeString() const = 0;
	virtual quint64 GetRegionSize() const			{ QReadLocker Locker(&m_Mutex); return m_RegionSize; }
	virtual QString GetProtectString() const = 0;
	virtual quint32 GetState() const				{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual bool IsFree() const = 0;
	virtual quint32 GetProtect() const				{ QReadLocker Locker(&m_Mutex); return m_Protect; }
	virtual bool IsExecutable() const = 0;
	virtual quint32 GetType() const					{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual bool IsMapped() const = 0;
	virtual bool IsPrivate() const = 0;

	virtual quint64 GetCommittedSize() const		{ QReadLocker Locker(&m_Mutex); return m_CommittedSize; }
	virtual quint64 GetPrivateSize() const			{ QReadLocker Locker(&m_Mutex); return m_PrivateSize; }

	virtual int GetRegionType() const				{ QReadLocker Locker(&m_Mutex); return m_RegionType; }

	virtual QString GetUseString() const = 0;

	virtual quint64 GetTotalWorkingSet() const		{ QReadLocker Locker(&m_Mutex); return m_TotalWorkingSet; }
	virtual quint64 GetPrivateWorkingSet() const	{ QReadLocker Locker(&m_Mutex); return m_PrivateWorkingSet; }
	virtual quint64 GetSharedWorkingSet() const		{ QReadLocker Locker(&m_Mutex); return m_SharedWorkingSet; }
	virtual quint64 GetShareableWorkingSet() const	{ QReadLocker Locker(&m_Mutex); return m_ShareableWorkingSet; }
	virtual quint64 GetLockedWorkingSet() const		{ QReadLocker Locker(&m_Mutex); return m_LockedWorkingSet; }

	virtual STATUS SetProtect(quint32 Protect) = 0;
	virtual STATUS DumpMemory(QIODevice* pFile) = 0;
	virtual STATUS FreeMemory(bool Free) = 0;

	virtual QIODevice* MkDevice() = 0;

protected:
	quint64				m_ProcessId;

	quint64				m_BaseAddress;
	quint64				m_AllocationBase;
    quint32				m_AllocationProtect;
    quint64 			m_RegionSize;
    quint32 			m_State;
    quint32 			m_Protect;
    quint32 			m_Type;

	quint64				m_CommittedSize;
	quint64				m_PrivateSize;

	int					m_RegionType;

	quint64				m_TotalWorkingSet;
    quint64				m_PrivateWorkingSet;
    quint64				m_SharedWorkingSet;
    quint64				m_ShareableWorkingSet;
    quint64				m_LockedWorkingSet;

	QSharedPointer<QObject>	m_AllocationBaseItem;
};

typedef QSharedPointer<CMemoryInfo> CMemoryPtr;
typedef QWeakPointer<CMemoryInfo> CMemoryRef;