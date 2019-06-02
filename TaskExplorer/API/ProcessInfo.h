#pragma once
#include <qobject.h>

#include "ThreadInfo.h"
#include "HandleInfo.h"

#ifndef PROCESS_PRIORITY_CLASS_UNKNOWN
#define PROCESS_PRIORITY_CLASS_UNKNOWN 0
#define PROCESS_PRIORITY_CLASS_IDLE 1
#define PROCESS_PRIORITY_CLASS_NORMAL 2
#define PROCESS_PRIORITY_CLASS_HIGH 3
#define PROCESS_PRIORITY_CLASS_REALTIME 4
#define PROCESS_PRIORITY_CLASS_BELOW_NORMAL 5
#define PROCESS_PRIORITY_CLASS_ABOVE_NORMAL 6
#endif

class CProcessInfo: public QObject
{
	Q_OBJECT

public:
	CProcessInfo(QObject *parent = nullptr);
	virtual ~CProcessInfo();

	// Basic
	virtual quint64 GetID() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual quint64 GetParentID() const				{ QReadLocker Locker(&m_Mutex); return m_ParentProcessId; }
	virtual QString GetName() const					{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }

	virtual QDateTime GetCreateTime() const			{ QReadLocker Locker(&m_Mutex); return m_CreateTime; }

	// Parameters
	virtual QString GetFileName() const				{ QReadLocker Locker(&m_Mutex); return m_FileName; }
	virtual QString GetCommandLine() const			{ QReadLocker Locker(&m_Mutex); return m_CommandLine; }

	// Other fields
	virtual QString GetUserName() const				{ QReadLocker Locker(&m_Mutex); return m_UserName; }

	// Dynamic
	virtual ulong GetPriorityClass() const			{ QReadLocker Locker(&m_Mutex); return m_PriorityClass; }
	virtual QString GetPriorityString() const;
	virtual ulong GetKernelTime() const				{ QReadLocker Locker(&m_Mutex); return m_KernelTime; }
	virtual qint64 GetUserTime() const				{ QReadLocker Locker(&m_Mutex); return m_UserTime; }
	virtual ulong GetNumberOfThreads() const		{ QReadLocker Locker(&m_Mutex); return m_NumberOfThreads; }

	virtual size_t GetWorkingSetPrivateSize() const	{ QReadLocker Locker(&m_Mutex); return m_WorkingSetPrivateSize; }
	virtual ulong GetPeakNumberOfThreads() const	{ QReadLocker Locker(&m_Mutex); return m_PeakNumberOfThreads; }
	virtual ulong GetHardFaultCount() const			{ QReadLocker Locker(&m_Mutex); return m_HardFaultCount; }

	// File
	virtual QPixmap GetFileIcon(bool bLarge = false) const	{ QReadLocker Locker(&m_Mutex); return bLarge ? m_LargeIcon : m_SmallIcon; }

	// Threads
	virtual QMap<quint64, CThreadPtr> GetThreadList()	{ QReadLocker Locker(&m_ThreadMutex); return m_ThreadList; }

	// Handles
	virtual QMap<quint64, CHandlePtr> GetHandleList()	{ QReadLocker Locker(&m_HandleMutex); return m_HandleList; }
	
signals:
	void			ThreadsUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);
	void			HandlesUpdated(QSet<quint64> Added, QSet<quint64> Changed, QSet<quint64> Removed);

public slots:
	virtual bool	UpdateThreads() = 0;
	virtual bool	UpdateHandles() = 0;

	virtual void	CleanUp();

protected:

	// Basic
	quint64			m_ProcessId;
	quint64			m_ParentProcessId;
	QString			m_ProcessName;

	QDateTime		m_CreateTime;

	// Parameters
	QString			m_FileName;
	QString			m_CommandLine;

	// File
	QPixmap			m_SmallIcon;
	QPixmap			m_LargeIcon;
	
	// Other fields
	QString			m_UserName;

	// Dynamic
	ulong			m_PriorityClass;
	qint64			m_KernelTime;
	qint64			m_UserTime;
	ulong			m_NumberOfThreads;

	size_t			m_WorkingSetPrivateSize;
	ulong			m_PeakNumberOfThreads;
	ulong			m_HardFaultCount;

	// Threads
	QMap<quint64, CThreadPtr>		m_ThreadList;

	// Handles
	QMap<quint64, CHandlePtr>		m_HandleList;



	mutable QReadWriteLock			m_Mutex;

	mutable QReadWriteLock			m_ThreadMutex;
	mutable QReadWriteLock			m_HandleMutex;
};

typedef QSharedPointer<CProcessInfo> CProcessPtr;
typedef QWeakPointer<CProcessInfo> CProcessRef;