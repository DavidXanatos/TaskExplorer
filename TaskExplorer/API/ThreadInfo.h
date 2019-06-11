#pragma once
#include <qobject.h>
#include "StackTrace.h"
#include "..\Common\Common.h"

class CThreadInfo: public QObject
{
	Q_OBJECT

public:
	CThreadInfo(QObject *parent = nullptr);
	virtual ~CThreadInfo();

	virtual quint64 GetThreadId()	const			{ QReadLocker Locker(&m_Mutex); return m_ThreadId; }
	virtual quint64 GetProcessId()	const			{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }

	virtual QDateTime GetCreateTime()				{ QReadLocker Locker(&m_Mutex); return m_CreateTime; }
	virtual quint64 GetKernelTime()	const			{ QReadLocker Locker(&m_Mutex); return m_KernelTime; }
	virtual quint64 GetUserTime()	const			{ QReadLocker Locker(&m_Mutex); return m_UserTime; }

	virtual long GetPriority()	const				{ QReadLocker Locker(&m_Mutex); return m_Priority; }
	virtual long GetBasePriority()	const			{ QReadLocker Locker(&m_Mutex); return m_BasePriority; }
	virtual int GetState()	const					{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual int GetWaitReason()	const				{ QReadLocker Locker(&m_Mutex); return m_WaitReason; }

	virtual void SetMainThread(bool bSet = true)		{ QWriteLocker Locker(&m_Mutex); m_IsMainThread = bSet; }
	virtual bool IsMainThread() const					{ QReadLocker Locker(&m_Mutex); return m_IsMainThread; }

	virtual float GetCpuUsage() const					{ QReadLocker Locker(&m_Mutex); return m_CpuUsage; }
	virtual float GetCpuKernelUsage() const				{ QReadLocker Locker(&m_Mutex); return m_CpuKernelUsage; }
	virtual float GetCpuUserUsage() const				{ QReadLocker Locker(&m_Mutex); return m_CpuUserUsage; }

	virtual QString GetStateString() const = 0;
	virtual QString GetPriorityString() const = 0;

public slots:
	virtual void TraceStack() = 0;

signals:
	void			StackTraced(const CStackTracePtr& StackTrace);

protected:
	quint64			m_ThreadId;
	quint64			m_ProcessId;

	bool			m_IsMainThread;

	QDateTime		m_CreateTime;
	quint64			m_KernelTime;
	quint64			m_UserTime;

	long			m_Priority;
    long			m_BasePriority;
	int				m_State;
	int				m_WaitReason;

	SDelta64		m_CpuKernelDelta;
    SDelta64		m_CpuUserDelta;
	SDelta32		m_ContextSwitchesDelta;
	SDelta64		m_CyclesDelta;

	float			m_CpuUsage; 
	float			m_CpuKernelUsage;
	float			m_CpuUserUsage;

	mutable QReadWriteLock		m_Mutex;
};

typedef QSharedPointer<CThreadInfo> CThreadPtr;
typedef QWeakPointer<CThreadInfo> CThreadRef;