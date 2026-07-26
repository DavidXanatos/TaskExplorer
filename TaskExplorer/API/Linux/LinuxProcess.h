#pragma once
#include "../ProcessInfo.h"
#include "ProcFs.h"
#include "X11Helper.h"

//
// A process as seen through /proc/<pid>.
//
// Identity, counters, threads, handles, modules, windows, the memory map,
// core dumps and debugger attach are implemented. What remains unimplemented is
// either genuinely absent on Linux (heaps, page priority, priority boost) or
// not built yet (module injection); those members carry a "linux-todo" comment
// or return an explanatory error rather than failing silently.
//
class CLinuxProcess : public CProcessInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxProcess)
public:
	CLinuxProcess(QObject *parent = nullptr);
	virtual ~CLinuxProcess();

	// ---- population, called by CLinuxAPI ----

	// One-shot fields: name, exe path, cmdline, user, start time.
	virtual bool			InitStaticData(quint64 Pid);
	//
	// Per-refresh fields: cpu/memory counters, thread & fd counts, state.
	//
	// SysTime is the divisor for this process's cpu percentage and follows the
	// "Linux style CPU" setting. SysTimePerCpu is always the single-cpu total
	// and is remembered for UpdateThreads(), because a thread can only occupy
	// one cpu and would otherwise be capped at 1/nproc.
	//
	virtual bool			UpdateDynamicData(bool bFullProcessInfo, quint64 SysTime, quint64 SysTimePerCpu);

	virtual bool			ValidateParent(CProcessInfo* pParent) const;

	// ---- identity ----
	virtual QString			GetArchString() const;
	virtual quint64			GetSessionID() const;
	virtual quint16			GetSubsystem() const;
	virtual QString			GetSubsystemString() const;
	virtual QString			GetWorkingDirectory() const;

	// ---- counters ----
	virtual quint32			GetPeakNumberOfHandles() const;
	// Cached whole-process residency totals; see GetRollup() for why.
	ProcFs::SMapDetail		GetRollup() const;
	virtual quint64			GetSharedWorkingSetSize() const;
	virtual quint64			GetShareableWorkingSetSize() const;
	virtual quint64			GetMinimumWS() const;
	virtual quint64			GetMaximumWS() const;

	virtual QString			GetStatusString() const;

	// ---- classification ----
	virtual bool			IsSystemProcess() const;
	virtual bool			IsServiceProcess() const;
	virtual bool			IsUserProcess() const;
	virtual bool			IsElevated() const;
	virtual bool			IsPowerThrottled() const;

	// ---- debugger ----
	virtual bool			HasDebugger() const;
	virtual STATUS			AttachDebugger();
	virtual STATUS			DetachDebugger();

	// ---- priority / scheduling ----
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

	// ---- lifetime ----
	virtual STATUS			Terminate(bool bForce);
	virtual bool			IsSuspended() const;
	virtual STATUS			Suspend();
	virtual STATUS			Resume();

	// ---- environment ----
	virtual QMap<QString, SEnvVar>	GetEnvVariables() const;
	virtual STATUS			DeleteEnvVariable(const QString& Name);
	virtual STATUS			EditEnvVariable(const QString& Name, const QString& Value);

	// ---- memory ----
	virtual QMap<quint64, CMemoryPtr>	GetMemoryMap() const;
	virtual QMap<quint64, CHeapPtr>		GetHeapList() const;
	virtual STATUS			FlushHeaps();

	// ---- windows ----
	virtual QList<CWndPtr>	GetWindows() const;
	virtual CWndPtr			GetMainWindow() const;

	virtual STATUS			LoadModule(const QString& Path);

	//
	// Applies the subset of the display's windows that belongs to this process.
	//
	// Windows are enumerated once per refresh by CLinuxAPI rather than per
	// process, because EnumWindows() returns the whole display and doing it per
	// process would be O(processes x windows) round trips. This also means the
	// window list is populated for every process, so the process tree's window
	// menu works without the Windows tab having been opened first.
	//
	virtual bool			SetWindows(const QList<X11Helper::SWindow>& Windows);

	// ---- Linux specifics used by the Linux backend itself ----
	virtual char			GetState() const		{ QReadLocker Locker(&m_Mutex); return m_State; }
	virtual qint32			GetNice() const			{ QReadLocker Locker(&m_Mutex); return m_Nice; }
	virtual quint32			GetUid() const			{ QReadLocker Locker(&m_Mutex); return m_Uid; }
	virtual quint32			GetGid() const			{ QReadLocker Locker(&m_Mutex); return m_Gid; }
	virtual bool			IsKernelThread() const	{ QReadLocker Locker(&m_Mutex); return m_IsKernelThread; }
	// The systemd unit this process belongs to, empty if it is not a service.
	virtual QString			GetServiceName() const	{ QReadLocker Locker(&m_Mutex); return m_ServiceName; }
	virtual QStringList		GetServiceList() const	{ QReadLocker Locker(&m_Mutex); return m_ServiceName.isEmpty() ? QStringList() : QStringList(m_ServiceName); }

	// The unified cgroup path, which is also the key to this process's
	// resource accounting - see ProcFs::ReadCGroupStats.
	virtual QString			GetCGroupPath() const	{ QReadLocker Locker(&m_Mutex); return m_CGroupPath; }

	//
	// Out of memory killer badness. The score is what the kernel currently
	// computes; the adjustment is the user-settable bias.
	//
	virtual int				GetOomScore() const		{ QReadLocker Locker(&m_Mutex); return m_OomScore; }
	virtual int				GetOomScoreAdj() const	{ QReadLocker Locker(&m_Mutex); return m_OomScoreAdj; }
	virtual STATUS			SetOomScoreAdj(int Value);

	// Container or sandbox this process runs in; empty when it is on the host.
	virtual QString			GetContainer() const	{ QReadLocker Locker(&m_Mutex); return m_Container; }

	// LSM profile, e.g. "snap.firefox.firefox (enforce)"; "unconfined" when an
	// LSM is active but does not confine this process.
	virtual QString			GetConfinement() const	{ QReadLocker Locker(&m_Mutex); return m_Confinement; }

	virtual ProcFs::SNamespaces	GetNamespaces() const { QReadLocker Locker(&m_Mutex); return m_Namespaces; }

	//
	// inotify watches held by this process.
	//
	// Sampled at a reduced rate rather than on every refresh: counting them
	// means a readlink per open file descriptor, which is affordable
	// occasionally but not several times a second for every process.
	//
	virtual quint64			GetInotifyWatches() const { QReadLocker Locker(&m_Mutex); return m_InotifyWatches; }

	//
	// Accepts I/O counters fetched on this process's behalf by an elevated
	// TaskHelper, for the case where /proc/<pid>/io is unreadable directly.
	//
	// Pushed in rather than pulled: the API layer collects the pids it could not
	// read and asks for all of them in one request, because a round trip per
	// process per refresh would be hundreds per second.
	//
	virtual void			SetHelperProcIo(const QByteArray& IoText);

	// Whether the last direct read of /proc/<pid>/io was refused.
	virtual bool			NeedsHelperProcIo() const { QReadLocker Locker(&m_Mutex); return m_bIoUnreadable; }

public slots:
	virtual bool			UpdateThreads();
	virtual bool			UpdateHandles();
	virtual bool			UpdateModules();
	virtual bool			UpdateWindows();

protected:
	char					m_State;
	qint32					m_Nice;
	qint32					m_SchedPolicy;
	quint32					m_Uid;
	quint64					m_CapEff;	// effective capabilities, see IsElevated()
	quint32					m_Gid;
	quint32					m_PeakNumberOfHandles;
	quint64					m_StartTimeTicks;
	bool					m_IsKernelThread;
	quint64					m_LastSysTimePerCpu;
	QString					m_ServiceName;
	bool					m_bSystemService;

	// Static for the life of the process; read once in InitStaticData.
	QString					m_CGroupPath;
	QString					m_Confinement;
	QString					m_Container;
	ProcFs::SNamespaces		m_Namespaces;

	int						m_OomScore;
	int						m_OomScoreAdj;

	bool					m_bIoUnreadable = false;

	quint64					m_InotifyWatches;
	quint64					m_InotifyTime;		// when last counted, ms
	int					m_IoPrio;

	//
	// Cached /proc/<pid>/smaps_rollup, refreshed on demand by GetRollup().
	// Mutable because the working-set getters are const but must be able to
	// populate the cache.
	//
	mutable ProcFs::SMapDetail	m_Rollup;
	mutable quint64			m_RollupTime;

	// Previous /proc/<pid>/stat sample, so cpu deltas can be computed without
	// re-reading the file.
	ProcFs::SStat			m_LastStat;
};

typedef QSharedPointer<CLinuxProcess> CLinuxProcessPtr;
typedef QWeakPointer<CLinuxProcess> CLinuxProcessRef;
