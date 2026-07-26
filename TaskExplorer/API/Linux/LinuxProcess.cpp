#include "stdafx.h"
#include "LinuxProcess.h"
#include "LinuxHandle.h"
#include "LinuxHelper.h"
#include "LinuxMemory.h"
#include "LinuxModule.h"
#include "LinuxThread.h"
#include "LinuxWnd.h"
#include "ProcFs.h"
#include "X11Helper.h"
#include "../SystemAPI.h"
#include "../../../MiscHelpers/Common/Settings.h"

#include <QFileInfo>
#include <QStandardPaths>

#include <errno.h>
#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <unistd.h>

CLinuxProcess::CLinuxProcess(QObject *parent)
	: CProcessInfo(parent)
{
	m_State = '\0';
	m_Nice = 0;
	m_SchedPolicy = 0;
	m_Uid = 0;
	m_CapEff = 0;
	m_Gid = 0;
	m_PeakNumberOfHandles = 0;
	m_StartTimeTicks = 0;
	m_IsKernelThread = false;
	m_LastSysTimePerCpu = 0;
	m_bSystemService = false;
	m_IoPrio = -1;
	m_RollupTime = 0;
	m_OomScore = 0;
	m_OomScoreAdj = 0;
	m_InotifyWatches = 0;
	m_InotifyTime = 0;
}

CLinuxProcess::~CLinuxProcess()
{
}

bool CLinuxProcess::InitStaticData(quint64 Pid)
{
	const ProcFs::SStat Stat = ProcFs::ReadStat(Pid);
	if (!Stat.Valid)
		return false; // process exited between enumeration and this read

	//
	// Read everything that needs no lock before taking one - these are all
	// separate /proc files and any of them can fail if the process exits
	// underneath us.
	//
	const QString ExePath = ProcFs::ReadLink(ProcFs::ProcPath(Pid, "exe"));
	const QStringList CmdLine = ProcFs::ReadNulList(ProcFs::ProcPath(Pid, "cmdline"));
	const QMap<QString, QString> Status = ProcFs::ReadStatus(Pid);
	const ProcFs::SServiceUnit ServiceUnit = ProcFs::ReadServiceUnit(Pid);

	//
	// Read once: a process does not normally change cgroup, namespaces or LSM
	// profile after it has started, and all three would otherwise cost extra
	// reads on every refresh of every process.
	//
	const QString CGroupPath = ProcFs::ReadCGroupPath(Pid);
	const ProcFs::SNamespaces Namespaces = ProcFs::ReadNamespaces(Pid);
	const QString Confinement = ProcFs::ReadProcSecurity(Pid).Confinement;
	const QString Container = LinuxDescribeContainer(Pid, Namespaces, CGroupPath, Confinement);

	//
	// The Uid line is "real effective saved filesystem"; the effective uid is
	// what determines what the process can do, so that is what is shown.
	//
	quint32 Uid = 0;
	quint32 Gid = 0;
	const QStringList UidFields = Status.value("Uid").split('\t', Qt::SkipEmptyParts);
	if (UidFields.size() > 1)
		Uid = UidFields[1].toUInt();
	const QStringList GidFields = Status.value("Gid").split('\t', Qt::SkipEmptyParts);
	if (GidFields.size() > 1)
		Gid = GidFields[1].toUInt();

	const quint64 StartTimeMs = ProcFs::StartTimeToEpochMs(Stat.StartTime);

	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = Pid;
	m_ParentProcessId = Stat.PPid;
	m_ProcessUId = SProcessUID(Pid, StartTimeMs);

	m_FileName = ExePath;

	m_IsKernelThread = Stat.IsKernelThread;

	//
	// Naming. Neither source is right on its own:
	//
	//   comm is what the process calls itself and what ps/top/ss display, but
	//   the kernel truncates it to 15 characters ("systemd-journald" arrives as
	//   "systemd-journal").
	//
	//   The exe basename is never truncated, but it is the file name rather
	//   than the program name, and those diverge for version-directory layouts
	//   - Claude Code installs as ".../claude/versions/2.1.220", so the exe
	//   basename is "2.1.220" where comm correctly says "claude".
	//
	// So: use the exe basename only when comm looks like a truncation of it
	// (i.e. comm is a prefix), which recovers the untruncated name. When the
	// two genuinely disagree, comm is the program's own idea of its name and
	// wins.
	//
	// Kernel threads get the conventional [brackets]. Note that an unreadable
	// exe link does NOT mean kernel thread - see ProcFs::SStat::IsKernelThread.
	//
	if (Stat.IsKernelThread)
	{
		m_ProcessName = "[" + Stat.Comm + "]";
	}
	else if (!ExePath.isEmpty())
	{
		const QString ExeName = QFileInfo(ExePath).fileName();
		m_ProcessName = ExeName.startsWith(Stat.Comm) ? ExeName : Stat.Comm;
	}
	else
	{
		m_ProcessName = Stat.Comm;
	}

	// argv is NUL separated; joining with spaces matches what the Windows
	// backend shows, at the cost of being ambiguous for arguments containing
	// spaces. The raw list is still available from /proc if ever needed.
	m_CommandLine = CmdLine.join(' ');

	m_Uid = Uid;
	m_Gid = Gid;
	m_UserName = ProcFs::UserNameFromUid(Uid);

	// Read once: a process does not normally migrate between cgroups, and this
	// would otherwise be an extra file read per process per refresh.
	m_ServiceName = ServiceUnit.Name;
	m_bSystemService = ServiceUnit.bSystemSlice;

	m_CGroupPath = CGroupPath;
	m_Namespaces = Namespaces;
	m_Confinement = Confinement;
	m_Container = Container;

	m_StartTimeTicks = Stat.StartTime;
	// Real start time rather than "when we first saw it", so the new-process
	// highlight is correct for processes that predate our startup.
	m_CreateTimeStamp = StartTimeMs;

	m_LastStat = Stat;

	return true;
}

bool CLinuxProcess::UpdateDynamicData(bool bFullProcessInfo, quint64 SysTime, quint64 SysTimePerCpu)
{
	const quint64 Pid = GetProcessId();
	m_LastSysTimePerCpu = SysTimePerCpu;

	const ProcFs::SStat Stat = ProcFs::ReadStat(Pid);
	if (!Stat.Valid)
		return false;

	const ProcFs::SStatM StatM = ProcFs::ReadStatM(Pid);
	const ProcFs::SProcIo Io = ProcFs::ReadProcIo(Pid);
	// Refused for other users' processes unless privileged; the API layer
	// collects these and asks TaskHelper for them in one batch.
	const bool bIoUnreadable = !Io.Valid;
	const QMap<QString, QString> Status = ProcFs::ReadStatus(Pid);

	const quint64 PageSize = ProcFs::PageSize();

	// /proc/<pid>/status reports these in kB; VmPeak/VmHWM have no counterpart
	// in stat/statm, which is why status is read at all.
	auto StatusKb = [&Status](const char* Key) -> quint64 {
		const QString Value = Status.value(Key);
		if (Value.isEmpty())
			return 0;
		return Value.split(' ', Qt::SkipEmptyParts).value(0).toULongLong() * 1024;
	};

	// Both are syscalls rather than file reads, and both are cheap; sampling
	// them here keeps the const getters free of side effects.
	const quint64 AffinityMask = LinuxGetAffinity(Pid);
	const int IoPrio = LinuxGetIoPrio(Pid);

	//
	// The effective capability set, as a hex mask. A process can drop
	// capabilities at any time, so this is sampled per refresh rather than once
	// - and status is being read here anyway, so it is free.
	//
	const quint64 CapEff = Status.value("CapEff").trimmed().toULongLong(nullptr, 16);

	// Two tiny reads, and the score moves with memory usage, so it is sampled
	// every refresh like the rest of the counters.
	const ProcFs::SOomInfo OomInfo = ProcFs::ReadOomInfo(Pid);

	//
	// Counting inotify watches costs a readlink per open descriptor, so it runs
	// on a slow cadence of its own rather than on every refresh. The number
	// moves rarely, and the reason to look at it - finding what is exhausting
	// fs.inotify.max_user_watches - does not need second-by-second resolution.
	//
	quint64 InotifyWatches = m_InotifyWatches;
	const quint64 Now = QDateTime::currentMSecsSinceEpoch();
	const bool bCountInotify = (Now - m_InotifyTime > 10000);
	if (bCountInotify)
		InotifyWatches = ProcFs::CountInotifyWatches(Pid);

	const quint32 NumThreads = (quint32)Stat.NumThreads;
	// There is no cheap fd count; the kernel exposes it only by listing the
	// directory, which is a syscall per process per refresh.
	const quint32 NumHandles = (quint32)ProcFs::EnumFds(Pid).count();

	bool bChanged = false;

	QWriteLocker Locker(&m_Mutex);

	bChanged |= (m_State != Stat.State);
	m_State = Stat.State;
	m_Nice = (qint32)Stat.Nice;
	m_SchedPolicy = (qint32)Stat.Policy;

	bChanged |= (m_NumberOfThreads != NumThreads);
	m_NumberOfThreads = NumThreads;
	if (NumThreads > m_PeakNumberOfThreads)
		m_PeakNumberOfThreads = NumThreads;

	bChanged |= (m_NumberOfHandles != NumHandles);
	m_NumberOfHandles = NumHandles;
	if (NumHandles > m_PeakNumberOfHandles)
		m_PeakNumberOfHandles = NumHandles;

	m_VirtualSize = Stat.VSize;
	m_PeakVirtualSize = StatusKb("VmPeak");
	m_WorkingSetSize = (quint64)Stat.Rss * PageSize;
	m_PeakWorkingSetSize = StatusKb("VmHWM");
	// statm's "shared" counts pages backed by a file; the rest of the resident
	// set is private, which is the closest analogue to the Windows private
	// working set.
	if (StatM.Valid)
		m_WorkingSetPrivateSize = (StatM.Resident > StatM.Shared) ? (StatM.Resident - StatM.Shared) * PageSize : 0;
	m_PeakPagefileUsage = StatusKb("VmSwap");

	m_KernelTime = Stat.STime;
	m_UserTime = Stat.UTime;

	// The GUI shows the nice value where Windows shows a priority class.
	m_Priority = Stat.Nice;
	m_BasePriority = Stat.Priority;

	m_AffinityMask = AffinityMask;
	m_IoPrio = IoPrio;
	m_CapEff = CapEff;
	m_bIoUnreadable = bIoUnreadable;

	if (OomInfo.Valid)
	{
		m_OomScore = OomInfo.Score;
		m_OomScoreAdj = OomInfo.ScoreAdj;
	}

	if (bCountInotify)
	{
		m_InotifyWatches = InotifyWatches;
		m_InotifyTime = Now;
	}
	m_IOPriority = IoPrio;

	m_LastStat = Stat;

	Locker.unlock();

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_CpuStats.CpuKernelDelta.Update(Stat.STime);
	m_CpuStats.CpuUserDelta.Update(Stat.UTime);
	// Faults are cumulative counters, so the delta is what is interesting.
	m_CpuStats.PageFaultsDelta.Update64(Stat.MinFlt + Stat.MajFlt);
	m_CpuStats.HardFaultsDelta.Update64(Stat.MajFlt);
	m_CpuStats.PrivateBytesDelta.Update(m_WorkingSetPrivateSize);
	m_CpuStats.UpdateStats(SysTime);

	if (Io.Valid)
	{
		// read_bytes/write_bytes are actual block layer traffic; rchar/wchar
		// include cache hits. Disk gets the former, Io the latter, matching how
		// the Windows backend separates disk from total I/O.
		m_Stats.Disk.SetRead(Io.ReadBytes, Io.SysCr);
		m_Stats.Disk.SetWrite(Io.WriteBytes, Io.SysCw);
		m_Stats.Io.SetRead(Io.RChar, Io.SysCr);
		m_Stats.Io.SetWrite(Io.WChar, Io.SysCw);
	}
	m_Stats.UpdateStats();

	return bChanged;
}

bool CLinuxProcess::ValidateParent(CProcessInfo* pParent) const
{
	if (!pParent)
		return false;

	// A process cannot be its own parent; pid 1 reports ppid 0, and pid 0 is
	// not a real process, so both would otherwise form a cycle in the tree.
	if (pParent->GetProcessId() == GetProcessId())
		return false;

	// A recycled pid shows up as a "parent" that started after its child.
	return pParent->GetCreateTimeStamp() <= GetCreateTimeStamp();
}

QString CLinuxProcess::GetArchString() const
{
	QString FileName = GetFileName();
	if (FileName.isEmpty())
		return QString(); // kernel thread

	//
	// EI_CLASS, the fifth byte of the ELF header: 1 = 32 bit, 2 = 64 bit.
	//
	QFile File(FileName);
	if (!File.open(QIODevice::ReadOnly))
		return QString();

	const QByteArray Header = File.read(5);
	if (Header.size() < 5 || !Header.startsWith("\x7f" "ELF"))
		return QString();

	switch (Header[4])
	{
		case 1: return "32-bit";
		case 2: return "64-bit";
	}
	return QString();
}

quint64 CLinuxProcess::GetSessionID() const
{
	// The session id from stat, i.e. the setsid() group. Not the same thing as
	// a logind/desktop session, which lives in /proc/<pid>/cgroup.
	QReadLocker Locker(&m_Mutex);
	return m_LastStat.Session;
}

quint16 CLinuxProcess::GetSubsystem() const
{
	// linux-todo: distinguish native / Wine / WSL-style processes.
	return 0;
}

QString CLinuxProcess::GetSubsystemString() const
{
	return QString();
}

QString CLinuxProcess::GetWorkingDirectory() const
{
	// Readable only for our own processes unless privileged; an empty string is
	// the correct answer for "not permitted".
	const quint64 Pid = GetProcessId();

	const QString Directory = ProcFs::ReadLink(ProcFs::ProcPath(Pid, "cwd"));
	if (!Directory.isEmpty())
		return Directory;

	//
	// Another user's cwd needs privileges. Ask an already-running helper, but
	// never start one: this is a const getter called from the GUI thread while
	// the process panel fills in, and starting an elevated helper means an
	// authentication prompt plus a wait of up to a minute - which is
	// indistinguishable from the application having hung. Showing the field blank
	// is the right answer until the user turns the helper on deliberately.
	//
	if (LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
		return LinuxHelperReadProcLink(Pid, "cwd", false);

	return QString();
}

quint32 CLinuxProcess::GetPeakNumberOfHandles() const
{
	// The kernel does not track a peak fd count; this is accumulated by
	// UpdateDynamicData instead.
	QReadLocker Locker(&m_Mutex);
	return m_PeakNumberOfHandles;
}

ProcFs::SMapDetail CLinuxProcess::GetRollup() const
{
	//
	// smaps_rollup is aggregated by the kernel, so it is far cheaper than
	// summing smaps - but it is still a page table walk over the whole address
	// space, and these are column getters that the model calls for every
	// visible row on every repaint.
	//
	// So it is read on demand and cached briefly. The columns that use it are
	// off by default, which means an unprivileged user browsing the default
	// layout never pays for it at all.
	//
	const quint64 Now = GetCurTick();

	QReadLocker ReadLocker(&m_Mutex);
	if (m_RollupTime && (Now - m_RollupTime) < 1000)
		return m_Rollup;
	const quint64 Pid = m_ProcessId;
	ReadLocker.unlock();

	const ProcFs::SMapDetail Rollup = ProcFs::ReadMapRollup(Pid);

	QWriteLocker WriteLocker(&m_Mutex);
	m_Rollup = Rollup;
	m_RollupTime = Now;
	return m_Rollup;
}

quint64 CLinuxProcess::GetSharedWorkingSetSize() const
{
	const ProcFs::SMapDetail Rollup = GetRollup();
	return Rollup.SharedClean + Rollup.SharedDirty;
}

quint64 CLinuxProcess::GetShareableWorkingSetSize() const
{
	// Resident minus what is exclusively ours: the portion that is, or could
	// be, shared with another process.
	const ProcFs::SMapDetail Rollup = GetRollup();
	const quint64 Private = Rollup.PrivateClean + Rollup.PrivateDirty;
	return (Rollup.Rss > Private) ? (Rollup.Rss - Private) : 0;
}

quint64 CLinuxProcess::GetMinimumWS() const
{
	// No Linux equivalent of a working set minimum; RLIMIT_RSS is advisory and
	// unused by modern kernels.
	return 0;
}

quint64 CLinuxProcess::GetMaximumWS() const
{
	// linux-todo: RLIMIT_RSS from /proc/<pid>/limits, if it is ever meaningful.
	return 0;
}

QString CLinuxProcess::GetStatusString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxStateToString(m_State);
}

bool CLinuxProcess::IsSystemProcess() const
{
	QReadLocker Locker(&m_Mutex);
	return m_IsKernelThread || m_Uid == 0;
}

bool CLinuxProcess::IsServiceProcess() const
{
	QReadLocker Locker(&m_Mutex);
	//
	// Only system.slice units count here. See ProcFs::SServiceUnit for why
	// simply "belongs to a .service" is too broad on a desktop session.
	// GetServiceName() still reports the unit for any process that has one.
	//
	return m_bSystemService;
}

bool CLinuxProcess::IsUserProcess() const
{
	QReadLocker Locker(&m_Mutex);
	// Below UID_MIN (1000 on most distributions) uids belong to system accounts.
	return !m_IsKernelThread && m_Uid >= 1000;
}

bool CLinuxProcess::IsElevated() const
{
	QReadLocker Locker(&m_Mutex);

	//
	// Root, or holding capabilities a normal user does not.
	//
	// A uid check alone would miss the interesting cases: ping, dumpcap and
	// anything else with file capabilities runs as an ordinary user but is
	// genuinely privileged, which is exactly what this column is meant to
	// surface. CapEff is sampled in UpdateDynamicData.
	//
	return m_Uid == 0 || m_CapEff != 0;
}

bool CLinuxProcess::IsPowerThrottled() const
{
	// No direct Linux counterpart; cgroup cpu throttling is the closest.
	return false;
}

bool CLinuxProcess::HasDebugger() const
{
	// TracerPid is 0 unless something is ptrace-attached to this process.
	return ProcFs::ReadStatus(GetProcessId()).value("TracerPid").toULongLong() != 0;
}

STATUS CLinuxProcess::AttachDebugger()
{
	const quint64 Pid = GetProcessId();

	if (Pid == (quint64)getpid())
		return ERR(tr("TaskExplorer cannot debug itself."));

	if (HasDebugger())
		return ERR(tr("This process is already being traced."));

	//
	// Windows looks up the system's postmortem debugger in the AeDebug registry
	// key. Linux has no such registry, so the debugger is whichever of the two
	// usual ones is installed - both take "-p <pid>" to attach.
	//
	QString Debugger = QStandardPaths::findExecutable("gdb");
	if (Debugger.isEmpty())
		Debugger = QStandardPaths::findExecutable("lldb");
	if (Debugger.isEmpty())
		return ERR(tr("No debugger is installed. Install gdb or lldb."));

	//
	// A debugger is an interactive program, so it needs a terminal of its own;
	// started without one it would attach and immediately have nowhere to read
	// commands from.
	//
	const STATUS Status = LinuxRunInTerminal(Debugger, QStringList() << "-p" << QString::number(Pid));
	if (Status.IsError())
		return Status;

	//
	// Whether the attach itself succeeds is up to the debugger and the kernel:
	// under the default Yama policy (ptrace_scope=1) gdb can only attach to its
	// own descendants unless it is privileged. It reports that in its own
	// window, which is a better place for it than a dialog here.
	//
	return OK;
}

STATUS CLinuxProcess::DetachDebugger()
{
	//
	// Not possible, and not a gap in this implementation: on Linux only the
	// tracer itself can call PTRACE_DETACH. There is no interface for a third
	// party to break someone else's ptrace attachment.
	//
	// Naming the tracer at least makes the message actionable.
	//
	const quint64 TracerPid = ProcFs::ReadStatus(GetProcessId()).value("TracerPid").toULongLong();
	if (!TracerPid)
		return ERR(tr("This process is not being traced."));

	const ProcFs::SStat Tracer = ProcFs::ReadStat(TracerPid);
	const QString Name = Tracer.Valid ? Tracer.Comm : tr("unknown");

	return ERR(tr("Only the debugger itself can detach. This process is being traced by %1 (pid %2); "
	              "quit it there, or terminate it.").arg(Name).arg(TracerPid));
}

bool CLinuxProcess::HasPriorityBoost() const
{
	// Linux has no per-process priority boost toggle.
	return false;
}

STATUS CLinuxProcess::SetPriorityBoost(bool Value)
{
	return ERR(tr("Priority boost is not supported on Linux."));
}

QString CLinuxProcess::GetPriorityString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxNiceToPriorityString(m_Nice);
}

STATUS CLinuxProcess::SetPriority(qint32 Value)
{
	// Lowering the nice value (raising priority) requires CAP_SYS_NICE, so this
	// is expected to fail for unprivileged callers going below 0.
	errno = 0;
	if (setpriority(PRIO_PROCESS, (id_t)GetProcessId(), Value) != 0 && errno != 0)
		return ErrnoToStatus(tr("Failed to set process priority"));

	QWriteLocker Locker(&m_Mutex);
	m_Nice = Value;
	m_Priority = Value;
	return OK;
}

QString CLinuxProcess::GetBasePriorityString() const
{
	QReadLocker Locker(&m_Mutex);
	return LinuxSchedPolicyToString(m_SchedPolicy);
}

STATUS CLinuxProcess::SetBasePriority(qint32 Value)
{
	//
	// The scheduling policy would be sched_setscheduler, but nothing in the
	// shared GUI ever calls this - the Windows base priority is read only, so
	// no menu is wired to it. Left as an explicit refusal rather than an
	// unreachable implementation.
	//
	return ERR(tr("Setting the scheduling policy is not supported; use chrt(1)."));
}

QString CLinuxProcess::GetPagePriorityString() const
{
	// Linux exposes no page priority; oom_score_adj is the nearest analogue and
	// is reported separately.
	return QString();
}

STATUS CLinuxProcess::SetPagePriority(qint32 Value)
{
	return ERR(tr("Page priority is not supported on Linux."));
}

QString CLinuxProcess::GetIOPriorityString() const
{
	// Sampled in UpdateDynamicData rather than queried here, so that repainting
	// the process list does not issue a syscall per visible row.
	QReadLocker Locker(&m_Mutex);
	return LinuxIoPrioToString(m_IoPrio);
}

STATUS CLinuxProcess::SetIOPriority(qint32 Value)
{
	//
	// The GUI hands over a raw ioprio value as produced by LinuxMakeIoPrio.
	// Raising into the realtime class needs CAP_SYS_ADMIN, so that is expected
	// to fail unprivileged.
	//
	if (LinuxSetIoPrio(GetProcessId(), Value) != 0)
		return ErrnoToStatus(tr("Failed to set the I/O priority"));

	QWriteLocker Locker(&m_Mutex);
	m_IoPrio = Value;
	m_IOPriority = Value;
	return OK;
}

STATUS CLinuxProcess::SetOomScoreAdj(int Value)
{
	if (Value < -1000 || Value > 1000)
		return ERR(tr("The OOM adjustment must be between -1000 and 1000."));

	if (!ProcFs::WriteOomScoreAdj(GetProcessId(), Value))
	{
		//
		// Raising the value is unprivileged; lowering it below what the process
		// already has needs CAP_SYS_RESOURCE, because that would protect a
		// process from the OOM killer at everyone else's expense.
		//
		if (errno == EACCES || errno == EPERM)
			return ERR(tr("Lowering the OOM adjustment requires root (CAP_SYS_RESOURCE)."), errno);
		return ErrnoToStatus(tr("Failed to set the OOM adjustment"));
	}

	QWriteLocker Locker(&m_Mutex);
	m_OomScoreAdj = Value;
	return OK;
}

STATUS CLinuxProcess::SetAffinityMask(quint64 Value)
{
	if (Value == 0)
		return ERR(tr("The affinity mask must select at least one CPU."));

	if (!LinuxSetAffinity(GetProcessId(), Value))
		return ErrnoToStatus(tr("Failed to set the affinity mask"));

	QWriteLocker Locker(&m_Mutex);
	m_AffinityMask = Value;
	return OK;
}

STATUS CLinuxProcess::Terminate(bool bForce)
{
	// SIGTERM asks politely and can be caught or ignored; SIGKILL cannot be,
	// which matches what "force" means on the Windows side.
	if (kill((pid_t)GetProcessId(), bForce ? SIGKILL : SIGTERM) != 0)
		return ErrnoToStatus(tr("Failed to terminate process"));
	return OK;
}

bool CLinuxProcess::IsSuspended() const
{
	QReadLocker Locker(&m_Mutex);
	return m_State == 'T';
}

STATUS CLinuxProcess::Suspend()
{
	if (kill((pid_t)GetProcessId(), SIGSTOP) != 0)
		return ErrnoToStatus(tr("Failed to suspend process"));
	return OK;
}

STATUS CLinuxProcess::Resume()
{
	if (kill((pid_t)GetProcessId(), SIGCONT) != 0)
		return ErrnoToStatus(tr("Failed to resume process"));
	return OK;
}

void CLinuxProcess::SetHelperProcIo(const QByteArray& IoText)
{
	const ProcFs::SProcIo Io = ProcFs::ParseProcIo(IoText);
	if (!Io.Valid)
		return;

	QWriteLocker Locker(&m_Mutex);
	m_bIoUnreadable = false;
	Locker.unlock();

	QWriteLocker StatsLocker(&m_StatsMutex);

	//
	// The same assignment UpdateDynamicData makes, so the delta and rate
	// machinery sees an uninterrupted series of cumulative counters.
	//
	m_Stats.Disk.SetRead(Io.ReadBytes, Io.SysCr);
	m_Stats.Disk.SetWrite(Io.WriteBytes, Io.SysCw);
	m_Stats.Io.SetRead(Io.RChar, Io.SysCr);
	m_Stats.Io.SetWrite(Io.WChar, Io.SysCw);
}

QMap<QString, CProcessInfo::SEnvVar> CLinuxProcess::GetEnvVariables() const
{
	QMap<QString, SEnvVar> Variables;

	//
	// Readable only for our own processes unless privileged, so an empty result
	// is asked of TaskHelper instead of being reported as "no environment".
	//
	const quint64 Pid = GetProcessId();
	QStringList Entries = ProcFs::ReadNulList(ProcFs::ProcPath(Pid, "environ"));
	if (Entries.isEmpty() && LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
	{
		// Only an already-running helper; see GetWorkingDirectory for why this
		// must not start one.
		QByteArray Data = LinuxHelperReadProcFile(Pid, "environ", false);
		while (Data.endsWith('\0'))
			Data.chop(1);
		if (!Data.isEmpty())
		{
			Entries.clear();
			for (const QByteArray& Part : Data.split('\0'))
				Entries.append(QString::fromUtf8(Part));
		}
	}

	for (const QString& Entry : Entries)
	{
		const int Sep = Entry.indexOf('=');
		if (Sep <= 0)
			continue;

		SEnvVar Var;
		Var.Name = Entry.left(Sep);
		Var.Value = Entry.mid(Sep + 1);
		Var.Type = SEnvVar::eProcess;
		Variables.insert(Var.Name, Var);
	}

	return Variables;
}

STATUS CLinuxProcess::DeleteEnvVariable(const QString& Name)
{
	// The environment of a running process cannot be edited from outside on
	// Linux without ptrace-injecting code; this is likely to stay unsupported.
	return ERR(tr("Editing the environment of a running process is not supported on Linux."));
}

STATUS CLinuxProcess::EditEnvVariable(const QString& Name, const QString& Value)
{
	return ERR(tr("Editing the environment of a running process is not supported on Linux."));
}

QMap<quint64, CMemoryPtr> CLinuxProcess::GetMemoryMap() const
{
	QMap<quint64, CMemoryPtr> MemoryMap;

	const quint64 Pid = GetProcessId();

	//
	// smaps is expensive - the kernel walks the page tables of every region to
	// produce it - but this is only called when the memory view is populated,
	// not on the refresh path, so paying for the residency detail here is
	// worthwhile. An empty result (older kernel, or no permission) just leaves
	// the working set columns at zero.
	//
	const QMap<quint64, ProcFs::SMapDetail> Details = ProcFs::ReadMapDetails(Pid);

	for (const ProcFs::SMapEntry& Entry : ProcFs::ReadMaps(Pid))
	{
		QSharedPointer<CLinuxMemory> pMemory = QSharedPointer<CLinuxMemory>(new CLinuxMemory());
		pMemory->InitStaticData(Pid, Entry);

		auto Detail = Details.constFind(Entry.Start);
		if (Detail != Details.constEnd())
			pMemory->SetDetail(*Detail);

		MemoryMap.insert(Entry.Start, pMemory);
	}

	return MemoryMap;
}

QMap<quint64, CHeapPtr> CLinuxProcess::GetHeapList() const
{
	// glibc does not publish heap arenas the way the Windows heap manager does;
	// this may end up permanently empty.
	return QMap<quint64, CHeapPtr>();
}

STATUS CLinuxProcess::FlushHeaps()
{
	return ERR(tr("Flushing heaps is not supported on Linux."));
}

QList<CWndPtr> CLinuxProcess::GetWindows() const
{
	QReadLocker Locker(&m_WindowMutex);
	return m_WindowList.values();
}

CWndPtr CLinuxProcess::GetMainWindow() const
{
	QReadLocker Locker(&m_WindowMutex);

	//
	// There is no "main window" concept in X11. The first visible top-level
	// window is the closest useful approximation, falling back to any window at
	// all so that a fully minimized application still resolves to something.
	//
	for (const CWndPtr& pWnd : m_WindowList)
	{
		if (pWnd->IsVisible())
			return pWnd;
	}
	return m_WindowList.isEmpty() ? CWndPtr() : m_WindowList.first();
}

STATUS CLinuxProcess::LoadModule(const QString& Path)
{
	// linux-todo: inject via ptrace + dlopen, mirroring the Windows InjectDll.
	return ERR(tr("Loading a module into a running process is not yet implemented on Linux."));
}

bool CLinuxProcess::UpdateThreads()
{
	const quint64 Pid = GetProcessId();

	const QList<quint64> Tids = ProcFs::EnumThreads(Pid);
	if (Tids.isEmpty())
		return false; // process is gone

	// Always the single-cpu total; see the note on UpdateDynamicData().
	const quint64 SysTime = m_LastSysTimePerCpu;

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CThreadPtr> OldThreads = GetThreadList();

	for (quint64 Tid : Tids)
	{
		QSharedPointer<CLinuxThread> pThread = OldThreads.take(Tid).staticCast<CLinuxThread>();
		bool bAdd = false;
		if (pThread.isNull())
		{
			pThread = QSharedPointer<CLinuxThread>(new CLinuxThread());
			if (!pThread->InitStaticData(Pid, Tid))
				continue; // exited between the readdir and the stat read

			theAPI->AddThread(pThread);

			QWriteLocker Locker(&m_ThreadMutex);
			m_ThreadList.insert(Tid, pThread);
			Locker.unlock();

			bAdd = true;
		}

		const bool bChanged = pThread->UpdateDynamicData(SysTime);

		if (bAdd)
			Added.insert(Tid);
		else if (bChanged)
			Changed.insert(Tid);
	}

	QWriteLocker Locker(&m_ThreadMutex);
	foreach(quint64 Tid, OldThreads.keys())
	{
		CThreadPtr pThread = m_ThreadList.value(Tid);
		if (pThread.isNull())
			continue;

		if (pThread->CanBeRemoved())
		{
			m_ThreadList.remove(Tid);
			theAPI->ClearThread(Tid);
			Removed.insert(Tid);
		}
		else if (!pThread->IsMarkedForRemoval())
		{
			pThread->MarkForRemoval();
			Changed.insert(Tid);
		}
	}
	Locker.unlock();

	emit ThreadsUpdated(Added, Changed, Removed);

	return true;
}

bool CLinuxProcess::UpdateHandles()
{
	const quint64 Pid = GetProcessId();

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CHandlePtr> OldHandles = GetHandleList();

	//
	// /proc/<pid>/fd is mode 500 and owned by the process's user, so for anything
	// belonging to someone else the directory cannot even be listed. When that
	// happens an elevated helper is asked for the whole set at once - the symlink
	// targets and the fdinfo text for every descriptor in one reply, rather than
	// the two round trips per descriptor a direct read would need.
	//
	QList<QMap<QString, QVariant>> HelperFds;
	QList<quint64> Fds;

	if (::access(ProcFs::ProcPath(Pid, "fd").toLocal8Bit().constData(), R_OK) == 0)
		Fds = ProcFs::EnumFds(Pid);
	else if (LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
		HelperFds = LinuxHelperListFds(Pid);

	if (!HelperFds.isEmpty())
	{
		for (const QMap<QString, QVariant>& Entry : HelperFds)
		{
			const quint64 Fd = Entry.value("Fd").toULongLong();
			const QString Target = Entry.value("Target").toString();
			const QByteArray FdInfo = Entry.value("Info").toByteArray();

			QSharedPointer<CLinuxHandle> pHandle = OldHandles.take(Fd).staticCast<CLinuxHandle>();
			bool bAdd = false;
			if (pHandle.isNull())
			{
				pHandle = QSharedPointer<CLinuxHandle>(new CLinuxHandle());
				if (!pHandle->InitStaticData(Pid, Fd, Target, FdInfo))
					continue;

				QWriteLocker Locker(&m_HandleMutex);
				m_HandleList.insert(Fd, pHandle);
				Locker.unlock();

				bAdd = true;
			}

			const bool bChanged = pHandle->UpdateDynamicData(FdInfo);

			if (bAdd)
				Added.insert(Fd);
			else if (bChanged)
				Changed.insert(Fd);
		}
	}

	for (quint64 Fd : Fds)
	{
		QSharedPointer<CLinuxHandle> pHandle = OldHandles.take(Fd).staticCast<CLinuxHandle>();
		bool bAdd = false;
		if (pHandle.isNull())
		{
			pHandle = QSharedPointer<CLinuxHandle>(new CLinuxHandle());
			if (!pHandle->InitStaticData(Pid, Fd))
				continue; // closed between the readdir and the readlink

			QWriteLocker Locker(&m_HandleMutex);
			m_HandleList.insert(Fd, pHandle);
			Locker.unlock();

			bAdd = true;
		}

		const bool bChanged = pHandle->UpdateDynamicData();

		if (bAdd)
			Added.insert(Fd);
		else if (bChanged)
			Changed.insert(Fd);
	}

	QWriteLocker Locker(&m_HandleMutex);
	foreach(quint64 Fd, OldHandles.keys())
	{
		CHandlePtr pHandle = m_HandleList.value(Fd);
		if (pHandle.isNull())
			continue;

		if (pHandle->CanBeRemoved())
		{
			m_HandleList.remove(Fd);
			Removed.insert(Fd);
		}
		else if (!pHandle->IsMarkedForRemoval())
		{
			pHandle->MarkForRemoval();
			Changed.insert(Fd);
		}
	}
	Locker.unlock();

	emit HandlesUpdated(Added, Changed, Removed);

	return true;
}

bool CLinuxProcess::UpdateModules()
{
	const quint64 Pid = GetProcessId();

	const QList<ProcFs::SMapEntry> Maps = ProcFs::ReadMaps(Pid);
	if (Maps.isEmpty())
		return false; // gone, or /proc/<pid>/maps not readable for us

	//
	// A single shared object contributes several consecutive mappings (text,
	// rodata, data, bss) with different protections, which have to be collapsed
	// into one module.
	//
	// Grouping purely by path is wrong: several unrelated mappings can share a
	// name - most visibly SysV shared memory, where every segment appears as
	// "/SYSVxxxxxxxx (deleted)" - and merging those produces one bogus module
	// spanning every address between them.
	//
	// /proc/<pid>/maps is ordered by address, so instead a run is only extended
	// while the path keeps matching. A different file-backed path closes the
	// current run; anonymous mappings in between (guard pages, bss) do not,
	// since those belong to the same load segment reservation.
	//
	struct SModuleRange
	{
		QString	Path;
		quint64	Start = 0;
		quint64	End = 0;
		bool	Executable = false;
	};
	QList<SModuleRange> Ranges;

	for (const ProcFs::SMapEntry& Entry : Maps)
	{
		// Pseudo-regions ([heap], [stack], [vdso]) are memory, not modules;
		// they show up in the memory view instead.
		const bool bFileBacked = (Entry.Inode != 0) && !Entry.Path.isEmpty() && !Entry.Path.startsWith('[');

		if (!bFileBacked)
			continue;

		if (!Ranges.isEmpty() && Ranges.last().Path == Entry.Path)
		{
			SModuleRange& Range = Ranges.last();
			if (Entry.End > Range.End)
				Range.End = Entry.End;
			Range.Executable |= Entry.Exec;
			continue;
		}

		SModuleRange Range;
		Range.Path = Entry.Path;
		Range.Start = Entry.Start;
		Range.End = Entry.End;
		Range.Executable = Entry.Exec;
		Ranges.append(Range);
	}

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CModulePtr> OldModules = GetModuleList();

	// Keyed by base address, matching how the Windows backend keys modules.
	for (const SModuleRange& Range : Ranges)
	{
		const QString& Path = Range.Path;

		QSharedPointer<CLinuxModule> pModule = OldModules.take(Range.Start).staticCast<CLinuxModule>();
		if (!pModule.isNull() && pModule->GetFileName() != Path)
		{
			// Same base address, different file: the old one was unmapped and
			// something else took its place.
			pModule.clear();
		}

		if (pModule.isNull())
		{
			pModule = QSharedPointer<CLinuxModule>(new CLinuxModule());
			pModule->InitStaticData(Path, Range.Start, Range.End - Range.Start);
			pModule->SetLoaded(true);

			QWriteLocker Locker(&m_ModuleMutex);
			m_ModuleList.insert(Range.Start, pModule);
			Locker.unlock();

			Added.insert(Range.Start);
		}
	}

	//
	// The main executable is the module whose path matches the exe link; the
	// modules view uses it for the process's own file details.
	//
	const QString ExePath = GetFileName();
	if (!ExePath.isEmpty())
	{
		QReadLocker Locker(&m_ModuleMutex);
		for (const CModulePtr& pModule : m_ModuleList)
		{
			if (pModule->GetFileName() == ExePath)
			{
				pModule->SetFirst(true);
				Locker.unlock();
				QWriteLocker WriteLocker(&m_Mutex);
				m_pModuleInfo = pModule;
				break;
			}
		}
	}

	QWriteLocker Locker(&m_ModuleMutex);
	foreach(quint64 BaseAddress, OldModules.keys())
	{
		CModulePtr pModule = m_ModuleList.value(BaseAddress);
		if (pModule.isNull())
			continue;

		// CModuleInfo derives from CAbstractInfo rather than CAbstractInfoEx,
		// so it has no removal-persistence machinery - unmapped modules are
		// dropped immediately.
		m_ModuleList.remove(BaseAddress);
		Removed.insert(BaseAddress);
	}
	Locker.unlock();

	emit ModulesUpdated(Added, Changed, Removed);

	return true;
}

bool CLinuxProcess::UpdateWindows()
{
	if (!X11Helper::IsAvailable())
		return false; // built without X11, or a Wayland session with no XWayland

	//
	// The list is refreshed for every process by CLinuxAPI::UpdateProcessList
	// via SetWindows(), so an explicit refresh only has to re-read the ones we
	// already know about. Enumerating the whole display again here would repeat
	// work that was just done.
	//
	QSet<quint64> Changed;

	QReadLocker ReadLocker(&m_WindowMutex);
	const QList<CWndPtr> Windows = m_WindowList.values();
	ReadLocker.unlock();

	for (const CWndPtr& pWnd : Windows)
	{
		if (pWnd.staticCast<CLinuxWnd>()->UpdateDynamicData())
			Changed.insert(pWnd->GetHWnd());
	}

	//
	// Emitted unconditionally, even when nothing changed.
	//
	// CWindowsView calls this as a "send me the current state" request and only
	// populates itself from the resulting signal - so staying silent because
	// there was no change leaves the view permanently empty, since the window
	// list was already filled in by SetWindows() before the view ever asked.
	//
	emit WindowsUpdated(QSet<quint64>(), Changed, QSet<quint64>());

	return true;
}

bool CLinuxProcess::SetWindows(const QList<X11Helper::SWindow>& Windows)
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CWndPtr> OldWindows = GetWindowList();

	//
	// _NET_WM_PID is a hint the client sets voluntarily, so a window whose
	// application never set it has a pid of 0 and cannot be attributed to any
	// process. That is a protocol limitation, not a bug; such windows simply do
	// not appear under any process.
	//
	for (const X11Helper::SWindow& Info : Windows)
	{
		QSharedPointer<CLinuxWnd> pWnd = OldWindows.take(Info.Window).staticCast<CLinuxWnd>();
		bool bAdd = false;
		if (pWnd.isNull())
		{
			pWnd = QSharedPointer<CLinuxWnd>(new CLinuxWnd());
			if (!pWnd->InitStaticData(Info.Window))
				continue; // unmapped between the enumeration and the query

			QWriteLocker Locker(&m_WindowMutex);
			m_WindowList.insert(Info.Window, pWnd);
			Locker.unlock();

			bAdd = true;
		}

		const bool bChanged = pWnd->UpdateDynamicData();

		if (bAdd)
			Added.insert(Info.Window);
		else if (bChanged)
			Changed.insert(Info.Window);
	}

	QWriteLocker Locker(&m_WindowMutex);
	foreach(quint64 Window, OldWindows.keys())
	{
		// CWndInfo derives from CAbstractInfo, which has no removal-persistence
		// machinery, so closed windows are dropped immediately.
		m_WindowList.remove(Window);
		Removed.insert(Window);
	}
	Locker.unlock();

	emit WindowsUpdated(Added, Changed, Removed);

	return true;
}
