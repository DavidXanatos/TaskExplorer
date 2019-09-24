#pragma once
#include "../ThreadInfo.h"

class CWinThread : public CThreadInfo
{
	Q_OBJECT
public:
	CWinThread(QObject *parent = nullptr);
	virtual ~CWinThread();

	virtual quint64 GetRawCreateTime() const;

	virtual QString GetName() const;

	virtual quint64 GetStartAddress() const				{ QReadLocker Locker(&m_Mutex); return m_StartAddress; }
	virtual QString GetStartAddressString() const;
	virtual QString GetStartAddressFileName() const		{ QReadLocker Locker(&m_Mutex); return m_StartAddressFileName; }

	virtual quint64 GetBasePriorityIncrement() const	{ QReadLocker Locker(&m_Mutex); return m_BasePriorityIncrement; }

	virtual QString GetStateString() const;
	virtual QString GetPriorityString() const;
	virtual STATUS SetPriority(long Value);
	virtual STATUS SetBasePriority(long Value) { return ERR(); }
	virtual STATUS SetPagePriority(long Value);
	virtual STATUS SetIOPriority(long Value);

	virtual STATUS SetAffinityMask(quint64 Value);

	virtual STATUS Terminate(bool bForce);

	virtual bool IsSuspended() const;
	virtual STATUS Suspend();
	virtual STATUS Resume();

	virtual QString GetServiceName() const				{ QReadLocker Locker(&m_Mutex); return m_ServiceName; }
	virtual QString GetThreadName() const				{ QReadLocker Locker(&m_Mutex); return m_ThreadName; }

	virtual bool IsGuiThread() const					{ QReadLocker Locker(&m_Mutex); return m_IsGuiThread; }
	virtual bool IsCriticalThread() const				{ QReadLocker Locker(&m_Mutex); return m_IsCritical; }
	virtual bool HasToken() const						{ QReadLocker Locker(&m_Mutex); return m_HasToken; }
	virtual STATUS SetCriticalThread(bool bSet, bool bForce = false);
	virtual STATUS CancelIO();
	virtual QString GetAppDomain() const;

	virtual QString GetIdealProcessor() const;

	virtual QString GetTypeString() const;

	virtual quint64 TraceStack();

	virtual void OpenPermissions();

private slots:
	void		OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName);

	void		UpdateAppDomain();
	void		OnAppDomain(int Index);

protected:
	friend class CWinProcess;

	bool InitStaticData(void* ProcessHandle, struct _SYSTEM_THREAD_INFORMATION* thread);
	bool UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread, quint64 sysTotalTime);
	bool UpdateExtendedData();
	quint64 GetCPUCycles();
	void UpdateCPUCycles(quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void UnInit();

	virtual void SetAppDomain(const QString& AppDomain) { QWriteLocker Locker(&m_Mutex); m_AppDomain = AppDomain; }

	QString		m_ServiceName;
	QString		m_ThreadName;

	quint64		m_StartAddress;
	QString		m_StartAddressString;
	QString		m_StartAddressFileName;

	long		m_BasePriorityIncrement;

	bool		m_IsGuiThread;
	bool		m_IsCritical;
	bool		m_HasToken;

	QString		m_AppDomain;

	struct SWinThread*	m;
};

QString SvcGetClrThreadAppDomain(const QVariantMap& Parameters);