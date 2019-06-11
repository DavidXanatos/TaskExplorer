#pragma once
#include "../ThreadInfo.h"

class CWinThread : public CThreadInfo
{
	Q_OBJECT
public:
	CWinThread(QObject *parent = nullptr);
	virtual ~CWinThread();

	virtual quint64 GetRawCreateTime() const;

	virtual quint64 GetStartAddress() const				{ QReadLocker Locker(&m_Mutex); return m_StartAddress; }
	virtual QString GetStartAddressString() const;

	virtual quint64 GetBasePriorityIncrement() const	{ QReadLocker Locker(&m_Mutex); return m_BasePriorityIncrement; }

	virtual QString GetStateString() const;
	virtual QString GetPriorityString() const;

	virtual QString GetServiceName() const				{ QReadLocker Locker(&m_Mutex); return m_ServiceName; }
	virtual QString GetThreadName() const				{ QReadLocker Locker(&m_Mutex); return m_ThreadName; }

	virtual bool IsGuiThread() const					{ QReadLocker Locker(&m_Mutex); return m_IsGuiThread; }

	virtual QString GetTypeString() const;

	virtual void TraceStack();

private slots:
	void	OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString/*, const QString& FileName, const QString& SymbolName*/);

protected:
	friend class CWinProcess;

	bool InitStaticData(void* pProcessHandle, struct _SYSTEM_THREAD_INFORMATION* thread);
	bool UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	bool UpdateExtendedData();
	void UnInit();

	QString		m_ServiceName;
	QString		m_ThreadName;

	quint64		m_StartAddress;
	QString		m_StartAddressString;

	long		m_BasePriorityIncrement;

	bool		m_IsGuiThread;

	struct SWinThread*	m;
};