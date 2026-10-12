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

	//
	// ---- the kernel's threads ----
	//
	// Linux gives every kernel thread a pid of its own, so a machine with eight
	// cores and a couple of filesystems shows a hundred and fifty processes that
	// are not processes: no address space, no image, no command line, one task
	// each. Windows has exactly the same threads and shows them the way they
	// are - as the threads of one System process - and there is no reason for
	// the two views of the same thing to differ.
	//
	// So kthreadd hosts them. It is the right host rather than an invented one:
	// it is the kernel thread daemon, the real parent of every one of them, and
	// it is itself a task with a pid. CLinuxAPI hands it the list each refresh;
	// what this object then does is show them as its threads and count their cpu
	// time as its own.
	//
	void					SetKernelThreadTasks(const QList<quint64>& Pids, quint64 UTime, quint64 STime);

	// ---- identity ----
	virtual quint16			GetArchitecture() const;
	virtual quint64			GetSessionID() const;
	virtual quint16			GetSubsystem() const;
	virtual QString			GetWorkingDirectory() const;

	// ---- counters ----
	virtual quint32			GetPeakNumberOfHandles() const;
	// Cached whole-process residency totals; see GetRollup() for why.
	ProcFs::SMapDetail		GetRollup() const;
	virtual quint64			GetSharedWorkingSetSize() const;
	virtual quint64			GetShareableWorkingSetSize() const;
	virtual quint64			GetMinimumWS() const;
	virtual quint64			GetMaximumWS() const;

	virtual int				GetRunState() const;

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
	virtual qint32			GetSchedPolicy() const	{ QReadLocker Locker(&m_Mutex); return m_SchedPolicy; }
	virtual STATUS			SetPriority(qint32 Value);
	virtual STATUS			SetBasePriority(qint32 Value);
	virtual STATUS			SetPagePriority(qint32 Value);
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
	// resource accounting.
	virtual QString			GetCGroupPath() const	{ QReadLocker Locker(&m_Mutex); return m_CGroupPath; }

	//
	// Which display this process draws on - "wayland-0", ":0", or nothing.
	//
	// The nearest thing Linux has to the window station and desktop the Windows
	// column shows. It is not the same object - there is no securable desktop
	// here - but it answers the same question: which screen is this process
	// attached to, and is it the one in front of me.
	//
	// Empty is a real answer and the common one: a daemon has no display, and
	// two thirds of the processes on an ordinary machine are daemons.
	//
	virtual QString			GetUsedDesktop() const	{ QReadLocker Locker(&m_Mutex); return m_UsedDesktop; }

	//
	// Read on demand rather than on every refresh: these are a dozen extra file
	// reads per process and only one process's worth is ever on screen. The
	// caller is the cgroup view, which asks for the selected process only.
	//
	virtual SCGroupStats	GetCGroupStats() const;
	virtual SResourcePressure GetCGroupPressure(const QString& Resource) const;

	//
	// Out of memory killer badness. The score is what the kernel currently
	// computes; the adjustment is the user-settable bias.
	//
	virtual int				GetOomScore() const		{ QReadLocker Locker(&m_Mutex); return m_OomScore; }
	virtual int				GetOomScoreAdj() const	{ QReadLocker Locker(&m_Mutex); return m_OomScoreAdj; }
	virtual STATUS			SetOomScoreAdj(int Value);

	// Container or sandbox this process runs in; empty when it is on the host.
	virtual QString			GetContainer() const	{ QReadLocker Locker(&m_Mutex); return m_Container; }

	//
	// ---- what the desktop and the package manager say ----
	//
	// A Windows binary describes itself: version resources carry the
	// description, the company and the version, and the file carries an icon
	// and a signature. An ELF binary carries none of that, and the same facts
	// live outside it - in the desktop entry that launches it and in the
	// package that installed it. See DesktopInfo.h and PackageInfo.h.
	//
	virtual QString			GetPackageName() const	{ QReadLocker Locker(&m_Mutex); return m_PackageName; }
	virtual QString			GetAppID() const		{ QReadLocker Locker(&m_Mutex); return m_AppId; }
	virtual QStringList		GetSandboxPermissions() const { QReadLocker Locker(&m_Mutex); return m_SandboxPermissions; }
	virtual quint32			GetMitigationFlags() const { QReadLocker Locker(&m_Mutex); return m_MitigationFlags; }

	virtual SWineInfo		GetWineInfo() const		{ QReadLocker Locker(&m_Mutex); return m_Wine; }

	//
	// What the process calls itself, where that is not simply its image name.
	//
	// The name column carries the image, as on Windows; this is the other name
	// a Linux process has - comm, set with prctl(PR_SET_NAME) - and it is the
	// only thing that tells one firefox child from another. Where a program has
	// not renamed itself, comm is the image name again and says nothing, so the
	// image's own description is answered instead and the column reads as it
	// does on Windows.
	//
	// A comm that is a prefix of the image name is the kernel's 15-character
	// truncation of it rather than a name of its own, and is not repeated here.
	//
	virtual QString			GetDescription() const;

	//
	// A window of this process by its handle, which for a Wine window is not
	// what it is filed under - see c_WineWindowKey.
	//
	virtual CWndPtr			GetWindow(quint64 hWnd) const;

	//
	// A Linux process has a run state rather than a set of flags, so this is
	// empty for almost all of them - but Wine is a genuine flag and the status
	// column reads it from here, which is what puts it on every row rather than
	// only on the selected one.
	//
	//
	// What is notable about this process, as the same flags Windows uses.
	//
	// A Linux process has a run state where a Windows one has a set of flags,
	// and the status column used to show only that - one word, where the same
	// column on Windows says "Elevated, Service, Packaged". Everything here is
	// already a member read during the refresh, so this costs no extra file
	// reads; what it does is stop the column being nearly empty on one platform.
	//
	// Deliberately not everything that could be said. Being in a cgroup or
	// having a parent is true of every process and says nothing.
	//
	virtual quint32			GetStatusFlags() const;

	//
	// What Windows calls this process, once the bridge has paired the two
	// halves. Set from CLinuxAPI::UpdateWineIds and nowhere else.
	//
	//
	// The Windows token, for a process under Wine. Null everywhere else, which
	// is what the base class returns and what every view already handles.
	//
	virtual CTokenInfoPtr	GetToken() const	{ QReadLocker Locker(&m_Mutex); return m_pWineToken; }
	void					SetWineToken(const CTokenInfoPtr& pToken)
							{ QWriteLocker Locker(&m_Mutex); m_pWineToken = pToken; }

	void					SetWineIds(quint32 WinPid, quint32 WinParentPid)
							{ QWriteLocker Locker(&m_Mutex); m_Wine.WinPid = WinPid; m_Wine.WinParentPid = WinParentPid; }

	// LSM profile, e.g. "snap.firefox.firefox (enforce)"; "unconfined" when an
	// LSM is active but does not confine this process.
	virtual QString			GetConfinement() const	{ QReadLocker Locker(&m_Mutex); return m_Confinement; }

	virtual SProcessNamespaces	GetNamespaces() const { QReadLocker Locker(&m_Mutex); return m_Namespaces; }

	//
	// Read on demand: /proc/<pid>/status and attr/current, for the one process
	// the security view has selected.
	//
	virtual SProcessSecurity	GetProcessSecurity() const;

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

	QString					m_PackageName;
	QString					m_AppId;
	QStringList				m_SandboxPermissions;
	quint32					m_MitigationFlags = 0;

	//
	// Set only on kthreadd, and only while the grouping is on. The times are the
	// sums over every kernel thread, which is what makes the host's cpu column
	// mean what the System process's does on Windows.
	//
	bool					m_bKernelThreadHost = false;
	QList<quint64>			m_KernelTaskPids;
	quint64					m_KernelTaskUTime = 0;
	quint64					m_KernelTaskSTime = 0;
	quint64					m_LastSysTimePerCpu;
	QString					m_ServiceName;
	bool					m_bSystemService;

	// Static for the life of the process; read once in InitStaticData.
	QString					m_CGroupPath;
	QString					m_UsedDesktop;
	QString					m_Confinement;
	QString					m_Container;
	SWineInfo				m_Wine;
	QString					m_Comm;
	CTokenInfoPtr			m_pWineToken;
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
