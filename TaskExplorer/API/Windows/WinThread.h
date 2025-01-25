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
	virtual QString GetBasePriorityString() const;
	virtual QString GetPagePriorityString() const;
	virtual QString GetIOPriorityString() const;
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

	enum ETokenState
	{
		PH_THREAD_TOKEN_STATE_UNKNOWN,
		PH_THREAD_TOKEN_STATE_NOT_PRESENT,
		PH_THREAD_TOKEN_STATE_ANONYMOUS,
		PH_THREAD_TOKEN_STATE_PRESENT
	};

	virtual ETokenState GetTokenState() const			{ QReadLocker Locker(&m_Mutex); return m_TokenState; }
	virtual QString GetTokenStateString() const;
	virtual bool HasToken2() const						{ QReadLocker Locker(&m_Mutex); return m_HasToken2; }
	virtual void SetSandboxed()							{ QWriteLocker Locker(&m_Mutex); m_IsSandboxed = true; }
	virtual bool IsSandboxed() const					{ QReadLocker Locker(&m_Mutex); return m_IsSandboxed; }

	virtual STATUS SetCriticalThread(bool bSet, bool bForce = false);
	virtual STATUS CancelIO();
	virtual QString GetAppDomain() const;

	virtual QString GetIdealProcessor() const;

	virtual QString GetTypeString() const;

	virtual quint64 TraceStack();

	virtual void OpenPermissions();

	virtual bool HasPendingIrp() const		{ QReadLocker Locker(&m_Mutex); return m_PendingIrp; }
	virtual bool IsFiber() const			{ QReadLocker Locker(&m_Mutex); return m_IsFiber; }
	virtual bool HasPriorityBoost() const	{ QReadLocker Locker(&m_Mutex); return m_PriorityBoost; }
	virtual STATUS SetPriorityBoost(bool Value);
	virtual bool IsPowerThrottled() const	{ QReadLocker Locker(&m_Mutex); return m_IsPowerThrottled; }


	virtual int GetApartmentState() const { QReadLocker Locker(&m_Mutex); return m_ApartmentState; }
	virtual QString GetApartmentStateString() const;

	virtual QString GetLastSysCallInfoString() const;
	virtual QString GetLastSysCallStatusString() const;

private slots:
	void		OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName);

	void		UpdateAppDomain();
	void		OnAppDomain(int Index);

protected:
	friend class CWinProcess;

	bool InitStaticData(void* ProcessHandle, struct _SYSTEM_THREAD_INFORMATION* thread);
	bool UpdateDynamicData(struct _SYSTEM_THREAD_INFORMATION* thread, quint64 sysTotalTime, quint64 sysTotalCycleTime);
	void CloseHandle();
	void UnInit();

	virtual void SetAppDomain(const QString& AppDomain) { QWriteLocker Locker(&m_Mutex); m_AppDomain = AppDomain; }

	QString		m_ServiceName;
	QString		m_ThreadName;

	quint64		m_StartAddress;
	QString		m_StartAddressString;
	QString		m_StartAddressFileName;

	long		m_BasePriorityIncrement;

	union {
				quint32 m_MiscStates;
				struct {
					quint32 m_IsGuiThread : 1,
							m_IsCritical : 1,
							m_IsSandboxed : 1,
							m_PendingIrp : 1,
							m_IsFiber : 1,
							m_PriorityBoost : 1,
							m_IsPowerThrottled : 1,
							m_Reserved : 25;
				};
	};
	
	ETokenState	m_TokenState;
	bool		m_HasToken2;

	int			m_ApartmentState;

	QString		m_AppDomain;

	struct SWinThread*	m;
};

QString SvcGetClrThreadAppDomain(const QVariantMap& Parameters);