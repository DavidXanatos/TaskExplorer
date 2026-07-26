#include "stdafx.h"
#include "LinuxThread.h"
#include "LinuxHelper.h"
#include "ProcFs.h"
#include "../StackTrace.h"

#include "../../SVC/TaskService.h"

#include <QMetaObject>

#include <errno.h>
#include <signal.h>
#include <sys/resource.h>

CLinuxThread::CLinuxThread(QObject *parent)
	: CThreadInfo(parent)
{
	m_State = '\0';
	m_Nice = 0;
	m_SchedPolicy = 0;
	m_IoPrio = -1;
	m_StartAddress = 0;
	m_StackTraceJob = 0;
}

CLinuxThread::~CLinuxThread()
{
}

bool CLinuxThread::InitStaticData(quint64 Pid, quint64 Tid)
{
	const ProcFs::SStat Stat = ProcFs::ReadThreadStat(Pid, Tid);
	if (!Stat.Valid)
		return false;

	// comm is per thread on Linux and is what pthread_setname_np() sets, so it
	// is usually more informative than the process name.
	const QString Name = ProcFs::ReadFileStr(ProcFs::TaskPath(Pid, Tid, "comm")).trimmed();

	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = Pid;
	m_ThreadId = Tid;

	// The thread whose tid equals the pid is the one that ran main().
	m_IsMainThread = (Pid == Tid);

	m_ThreadName = Name.isEmpty() ? Stat.Comm : Name;
	m_CreateTimeStamp = ProcFs::StartTimeToEpochMs(Stat.StartTime);

	//
	// Note this is 0 on any current kernel, for everyone including root:
	// /proc/<pid>/stat's kstkeip and kstkesp fields were hardcoded to zero to
	// stop kernel addresses leaking, rather than merely gated on ptrace access.
	// Verified here - even reading our own stat gives 0. The same applies to
	// m_StackUsage below, which is derived from startstack and kstkesp.
	//
	// The code is kept because the field is still specified, costs nothing, and
	// will populate again on a kernel that provides it; GetStartAddressString()
	// renders 0 as blank so the column is simply empty meanwhile.
	//
	// Note what this actually is: the thread's *current* instruction pointer,
	// sampled the first time TaskExplorer saw the thread. Linux does not record
	// the function a thread was created to run - that argument to
	// pthread_create is never handed to the kernel - so there is no true
	// equivalent of the Windows start address. Sampling once at first sight is
	// the closest honest approximation, and for a thread caught early it is
	// usually still inside its entry function.
	//
	// It is deliberately resolved here, once, rather than on every refresh:
	// the value would otherwise chase the running thread around and the column
	// would never hold still long enough to read.
	//
	m_StartAddress = Stat.KstkEip;
	m_StartAddressString = m_StartAddress ? LinuxResolveAddress(Pid, m_StartAddress) : QString();

	return true;
}

bool CLinuxThread::UpdateDynamicData(quint64 SysTime)
{
	const quint64 Pid = GetProcessId();
	const quint64 Tid = GetThreadId();

	const ProcFs::SStat Stat = ProcFs::ReadThreadStat(Pid, Tid);
	if (!Stat.Valid)
		return false;

	bool bChanged = false;

	QWriteLocker Locker(&m_Mutex);

	bChanged |= (m_State != Stat.State);
	m_State = Stat.State;
	m_Nice = (qint32)Stat.Nice;
	m_SchedPolicy = (qint32)Stat.Policy;

	m_KernelTime = Stat.STime;
	m_UserTime = Stat.UTime;

	m_Priority = Stat.Nice;
	m_BasePriority = Stat.Priority;

	m_IoPrio = LinuxGetIoPrio(Tid);
	m_IOPriority = m_IoPrio;

	m_AffinityMask = LinuxGetAffinity(Tid);

	//
	// startstack is the base of the thread's user stack and kstkesp its current
	// stack pointer, so the difference is how much of the stack is in use. Both
	// are zeroed for processes we cannot ptrace, in which case this stays 0.
	//
	if (Stat.StartStack && Stat.KstkEsp && Stat.StartStack > Stat.KstkEsp)
		m_StackUsage = Stat.StartStack - Stat.KstkEsp;

	Locker.unlock();

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_CpuStats.CpuKernelDelta.Update(Stat.STime);
	m_CpuStats.CpuUserDelta.Update(Stat.UTime);
	m_CpuStats.UpdateStats(SysTime);

	return bChanged;
}

QString CLinuxThread::GetName() const
{
	QReadLocker Locker(&m_Mutex);
	return m_ThreadName;
}

QString CLinuxThread::GetStartAddressString() const
{
	QReadLocker Locker(&m_Mutex);
	return m_StartAddressString;
}

QString CLinuxThread::GetStateString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxStateToString(m_State);
}

bool CLinuxThread::HasPriorityBoost() const
{
	return false;
}

STATUS CLinuxThread::SetPriorityBoost(bool Value)
{
	return ERR(tr("Priority boost is not supported on Linux."));
}

QString CLinuxThread::GetPriorityString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxNiceToPriorityString(m_Nice);
}

STATUS CLinuxThread::SetPriority(qint32 Value)
{
	//
	// Despite the PRIO_PROCESS name, the "who" argument of setpriority is a
	// tid on Linux, so this really does adjust one thread rather than the whole
	// thread group.
	//
	errno = 0;
	if (setpriority(PRIO_PROCESS, (id_t)GetThreadId(), Value) != 0 && errno != 0)
		return ErrnoToStatus(tr("Failed to set thread priority"));

	QWriteLocker Locker(&m_Mutex);
	m_Nice = Value;
	m_Priority = Value;
	return OK;
}

QString CLinuxThread::GetBasePriorityString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxSchedPolicyToString(m_SchedPolicy);
}

STATUS CLinuxThread::SetBasePriority(qint32 Value)
{
	// See CLinuxProcess::SetBasePriority - the shared GUI never calls this.
	return ERR(tr("Setting the scheduling policy is not supported; use chrt(1)."));
}

QString CLinuxThread::GetPagePriorityString() const
{
	return QString();
}

STATUS CLinuxThread::SetPagePriority(qint32 Value)
{
	return ERR(tr("Page priority is not supported on Linux."));
}

QString CLinuxThread::GetIOPriorityString() const
{
	// Sampled in UpdateDynamicData so that repainting the thread list does not
	// issue a syscall per visible row.
	QReadLocker Locker(&m_Mutex);
	return LinuxIoPrioToString(m_IoPrio);
}

STATUS CLinuxThread::SetIOPriority(qint32 Value)
{
	//
	// IOPRIO_WHO_PROCESS is a misnomer: like setpriority, the "who" argument is
	// really a tid, so this sets one thread's I/O priority rather than the whole
	// thread group's.
	//
	if (LinuxSetIoPrio(GetThreadId(), Value) != 0)
		return ErrnoToStatus(tr("Failed to set the I/O priority"));

	QWriteLocker Locker(&m_Mutex);
	m_IoPrio = Value;
	m_IOPriority = Value;
	return OK;
}

STATUS CLinuxThread::SetAffinityMask(quint64 Value)
{
	if (Value == 0)
		return ERR(tr("The affinity mask must select at least one CPU."));

	// sched_setaffinity is per thread; passing a tid is the documented way to
	// pin one thread without touching its siblings.
	if (!LinuxSetAffinity(GetThreadId(), Value))
		return ErrnoToStatus(tr("Failed to set the affinity mask"));

	QWriteLocker Locker(&m_Mutex);
	m_AffinityMask = Value;
	return OK;
}

STATUS CLinuxThread::Terminate(bool bForce)
{
	// Linux offers no safe way to kill a single thread of another process;
	// tgkill with SIGKILL takes the whole thread group down.
	return ERR(tr("Terminating an individual thread is not supported on Linux."));
}

bool CLinuxThread::IsSuspended() const
{
	QReadLocker Locker(&m_Mutex);
	return m_State == 'T';
}

STATUS CLinuxThread::Suspend()
{
	return ERR(tr("Suspending an individual thread is not supported on Linux."));
}

STATUS CLinuxThread::Resume()
{
	return ERR(tr("Resuming an individual thread is not supported on Linux."));
}

//
// Builds a one-frame trace whose symbol is an explanation. The stack view shows
// the symbol column, so this puts the reason in front of the user instead of
// leaving an empty list they cannot interpret.
//
static CStackTracePtr MakeMessageTrace(quint64 Pid, quint64 Tid, const QString& Message)
{
	CStackTracePtr Trace(new CStackTrace(Pid, Tid));
	quint64 Params[4] = { 0, 0, 0, 0 };
	Trace->AddFrame(Message, 0, 0, 0, 0, 0, Params, 0);
	return Trace;
}

quint64 CLinuxThread::TraceStack()
{
	// One trace in flight at a time; the view will not ask again until this one
	// reports back anyway.
	if (m_StackTraceJob)
		return m_StackTraceJob;

	static quint64 NextJob = 0;
	m_StackTraceJob = ++NextJob;

	//
	// The unwinding is done by TaskHelper, which links libdwfl directly. The
	// previous implementation shelled out to eu-stack and then eu-addr2line and
	// parsed their output; see TaskHelper/LinuxMain.cpp for why that moved.
	//
	// TraceStack() is called from the GUI thread, but this object lives on the
	// API worker thread, and the request to the helper is synchronous. So the
	// work is bounced onto this object's own thread: blocking a refresh cycle for
	// a few tens of milliseconds is acceptable, blocking the window is not.
	//
	// A queued invocation is also the lifetime guarantee - if the thread object
	// is destroyed first, Qt drops the pending call rather than running it
	// against freed memory.
	//
	QMetaObject::invokeMethod(this, "RequestStackTrace", Qt::QueuedConnection);

	return m_StackTraceJob;
}

void CLinuxThread::RequestStackTrace()
{
	const quint64 Pid = GetProcessId();
	const quint64 Tid = GetThreadId();

	//
	// Unprivileged deliberately. Under the default Yama policy this can only
	// unwind descendants, which is exactly what the old eu-stack path could do -
	// and asking for elevation here would put a password prompt in the way of
	// merely selecting a thread. Running TaskExplorer elevated gives the helper
	// root by inheritance, with no prompt.
	//
	const QString Socket = CTaskService::RunWorker(false);
	if (Socket.isEmpty())
	{
		m_StackTraceJob = 0;
		emit StackTraced(MakeMessageTrace(Pid, Tid,
			tr("The TaskHelper process could not be started, so stacks cannot be unwound.")));
		return;
	}

	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;
	Parameters["ThreadId"] = Tid;
	Parameters["MaxFrames"] = (quint32)64;

	QVariantMap Command;
	Command["Command"] = "TraceStack";
	Command["Parameters"] = Parameters;

	const QVariantMap Reply = CTaskService::SendCommand(Socket, Command, 15000).toMap();

	m_StackTraceJob = 0;

	if (Reply.isEmpty())
	{
		emit StackTraced(MakeMessageTrace(Pid, Tid,
			tr("The TaskHelper process did not answer.")));
		return;
	}

	const QVariantList Frames = Reply["Frames"].toList();
	if (Frames.isEmpty())
	{
		//
		// The usual cause is ptrace being refused: Yama's ptrace_scope=1 (the
		// default on Ubuntu) only permits tracing descendants. The helper reports
		// that as a code so the wording can be decided here, where it is known
		// whether we are elevated.
		//
		const QString Error = Reply["Error"].toString();
		QString Message;
		if (Error == "ptrace" || Error.contains("not permitted", Qt::CaseInsensitive))
		{
			Message = tr("Cannot read this thread's stack: ptrace access was denied. "
			             "Restart TaskExplorer elevated, or lower kernel.yama.ptrace_scope.");
		}
		else if (!Error.isEmpty())
			Message = tr("Stack trace failed: %1").arg(Error);
		else
			Message = tr("No stack frames were returned.");

		emit StackTraced(MakeMessageTrace(Pid, Tid, Message));
		return;
	}

	CStackTracePtr Trace(new CStackTrace(Pid, Tid));

	foreach(const QVariant& FrameVar, Frames)
	{
		const QVariantMap Frame = FrameVar.toMap();

		const quint64 Address = Frame["Address"].toULongLong();
		const QString Module = Frame["Module"].toString();
		const quint64 Offset = Frame["Offset"].toULongLong();

		//
		// The name if one was found, otherwise the position - "libxul.so+0x4b2e7c"
		// is still something that can be looked up by hand, where a blank is not.
		//
		QString Symbol = Frame["Symbol"].toString();
		if (Symbol.isEmpty())
		{
			if (!Module.isEmpty())
				Symbol = QString("%1+0x%2").arg(Module.section('/', -1)).arg(Offset, 0, 16);
			else
				Symbol = QString("0x%1").arg(Address, 0, 16);
		}
		else if (Frame.contains("SymbolOffset"))
		{
			//
			// The offset into the symbol, which is what distinguishes a real match
			// from a nearest-preceding-symbol guess in a stripped library: a name
			// with a large offset is not to be trusted.
			//
			const quint64 SymbolOffset = Frame["SymbolOffset"].toULongLong();
			if (SymbolOffset)
				Symbol += QString("+0x%1").arg(SymbolOffset, 0, 16);
		}

		//
		// Source location where the debug information provided one, otherwise the
		// module and the offset into it.
		//
		QString FileInfo;
		if (Frame.contains("File"))
		{
			FileInfo = Frame["File"].toString();
			const int Line = Frame["Line"].toInt();
			if (Line)
				FileInfo += ":" + QString::number(Line);
		}
		else if (!Module.isEmpty())
		{
			FileInfo = Offset ? QString("%1+0x%2").arg(Module).arg(Offset, 0, 16) : Module;
		}

		quint64 Params[4] = { 0, 0, 0, 0 };
		Trace->AddFrame(Symbol, Address, 0, 0, 0, 0, Params, 0, FileInfo);
	}

	emit StackTraced(Trace);
}
