#pragma once
#include "../ThreadInfo.h"
#include "ProcFs.h"

//
// A thread as seen through /proc/<pid>/task/<tid>.
//
class CLinuxThread : public CThreadInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxThread)
public:
	CLinuxThread(QObject *parent = nullptr);
	virtual ~CLinuxThread();

	virtual bool			InitStaticData(quint64 Pid, quint64 Tid);
	virtual bool			UpdateDynamicData(quint64 SysTime);

	virtual QString			GetName() const;
	virtual QString			GetStartAddressString() const;
	virtual QString			GetStateString() const;

	virtual bool			HasPriorityBoost() const;
	virtual STATUS			SetPriorityBoost(bool Value);
	virtual QString			GetPriorityString() const;
	virtual STATUS			SetPriority(qint32 Value);
	virtual QString			GetBasePriorityString() const;
	virtual STATUS			SetBasePriority(qint32 Value);
	virtual QString			GetPagePriorityString() const;
	virtual STATUS			SetPagePriority(qint32 Value);
	virtual QString			GetIOPriorityString() const;
	virtual STATUS			SetIOPriority(qint32 Value);
	virtual STATUS			SetAffinityMask(quint64 Value);

	virtual STATUS			Terminate(bool bForce);
	virtual bool			IsSuspended() const;
	virtual STATUS			Suspend();
	virtual STATUS			Resume();

public slots:
	//
	// Starts an asynchronous stack trace and returns a non-zero job id; the
	// result arrives later on the StackTraced() signal.
	//
	// Unwinding another process's stack requires ptrace access, so under the
	// default Yama policy (ptrace_scope=1) this only works for descendants
	// unless TaskExplorer is running elevated. The failure is reported as a
	// single explanatory frame rather than an empty list, so the reason is
	// visible in the stack view.
	//
	virtual quint64			TraceStack();

private slots:
	//
	// Runs on this object's own thread (the API worker), because the request to
	// TaskHelper is synchronous and must not block the GUI. See TraceStack().
	//
	void					RequestStackTrace();

protected:
	QString					m_ThreadName;	// /proc/<pid>/task/<tid>/comm
	char					m_State;
	qint32					m_Nice;
	qint32					m_SchedPolicy;
	qint32					m_IoPrio;
	quint64					m_StartAddress;
	QString					m_StartAddressString;	// resolved once, see InitStaticData

	// Non-zero while a trace is in flight; also the job id handed to the view.
	quint64					m_StackTraceJob;
};

typedef QSharedPointer<CLinuxThread> CLinuxThreadPtr;
typedef QWeakPointer<CLinuxThread> CLinuxThreadRef;
