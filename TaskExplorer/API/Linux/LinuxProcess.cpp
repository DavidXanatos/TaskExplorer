#include "stdafx.h"
#include "LinuxProcess.h"
#include "DesktopInfo.h"
#include "PackageInfo.h"
#include "LinuxHandle.h"
#include "LinuxHelper.h"
#include "LinuxWine.h"
#include "LinuxWineHandle.h"
#include "LinuxWineWnd.h"
#include "LinuxWineHelper.h"
#include "../../../MiscHelpers/Common/Variant.h"
#include "LinuxMemory.h"
#include "LinuxModule.h"
#include "LinuxThread.h"
#include "LinuxWnd.h"
#include "ProcFs.h"
#include "X11Helper.h"
#include "../SystemAPI.h"
#include "../../../MiscHelpers/Common/Settings.h"

#include <QFileInfo>
#include <QRegularExpression>
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
	//
	// Wine, and what the process really is when it is running under it.
	//
	// Read here with the rest of the once-only data: a process does not change
	// prefix or image, and this costs a maps read, which is not worth doing per
	// refresh for every process on the machine.
	//
	const SWineInfo Wine = LinuxDetectWine(Pid, ExePath, CmdLine);

	const QString CGroupPath = ProcFs::ReadCGroupPath(Pid);
	const ProcFs::SNamespaces Namespaces = ProcFs::ReadNamespaces(Pid);
	const QString Confinement = ProcFs::ReadProcSecurity(Pid).Confinement;
	const QString Container = LinuxDescribeContainer(Pid, Namespaces, CGroupPath, Confinement);

	//
	// And what that sandbox actually granted, which is written inside the
	// sandbox rather than guessed at from outside - see LinuxSandboxForProcess.
	//
	const SSandboxInfo Sandbox = LinuxSandboxForProcess(Pid, Confinement);

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

	//
	// The display this process is attached to, from its environment.
	//
	// Read here with the rest of the once-only data because an environment does
	// not change: /proc/<pid>/environ is what the process was started with, and
	// nothing outside it can alter that. Doing it per refresh would be a file
	// read per process for a value that cannot have moved.
	//
	// Wayland first. A session running Wayland with Xwayland beside it sets both
	// - WAYLAND_DISPLAY for native clients and DISPLAY for the X11 ones - and a
	// native client that reports ":0" would be naming the compatibility layer it
	// is not using.
	//
	// Readable only for our own processes unless privileged, like the working
	// directory and the environment tab. Empty is then the honest answer rather
	// than a guess, and it is indistinguishable from a daemon that genuinely has
	// no display - which is a limitation worth knowing rather than papering over.
	//
	QString UsedDesktop;
	{
		QString X11;
		foreach(const QString& Entry, ProcFs::ReadNulList(ProcFs::ProcPath(Pid, "environ")))
		{
			if (Entry.startsWith("WAYLAND_DISPLAY="))
			{
				UsedDesktop = Entry.mid(16);
				break;
			}
			if (X11.isEmpty() && Entry.startsWith("DISPLAY="))
				X11 = Entry.mid(8);
		}
		if (UsedDesktop.isEmpty())
			UsedDesktop = X11;
	}

	QWriteLocker Locker(&m_Mutex);

	m_ProcessId = Pid;
	m_ParentProcessId = Stat.PPid;
	m_ProcessUId = SProcessUID(Pid, StartTimeMs);

	//
	// The image, and for a Wine process that is not what /proc/<pid>/exe says.
	//
	// exe names the loader - every process in the prefix reports
	// wine-preloader - so the file this process actually *is* comes from its
	// memory map instead. It is a real path to a real file, so everything that
	// opens the image goes on working; what Windows calls it is in m_Wine.
	//
	m_Wine = Wine;
	m_FileName = (Wine.Valid && !Wine.UnixImagePath.isEmpty()) ? Wine.UnixImagePath : ExePath;

	m_UsedDesktop = UsedDesktop;

	m_IsKernelThread = Stat.IsKernelThread;

	//
	// Everything a Windows image would have carried in itself, gathered from
	// where Linux keeps it instead. Once per process rather than per refresh:
	// none of it changes while a process runs, and both lookups are cached by
	// path so a hundred processes of one image cost one read.
	//
	// A kernel thread has no image and no entry anywhere; asking would only
	// walk two indexes to be told so.
	//
	SDesktopEntry Desktop;
	SPackageInfo Package;
	if (!Stat.IsKernelThread && !m_FileName.isEmpty())
	{
		Desktop = LinuxDesktopEntry(m_FileName);
		Package = LinuxPackageForFile(m_FileName);

		m_AppId = Desktop.Id;
		m_PackageName = Package.Name;
	}

	//
	// A sandboxed application is its own package and names itself better than
	// the file system does: the flatpak id is what it is called everywhere it
	// is mentioned, and the snap name is what removes it again.
	//
	if (Sandbox.Valid && !Sandbox.AppId.isEmpty())
	{
		if (m_PackageName.isEmpty())
			m_PackageName = Sandbox.AppId;
		if (m_AppId.isEmpty())
			m_AppId = Sandbox.AppId;
	}

	//
	// ---- the image's own description ----
	//
	// The module list is filled on demand, so on a machine nobody has asked
	// about there is no module to hang this on - and the Description, Company
	// and Version columns read it from the main module. So the module is made
	// here, with what the desktop entry and the package say, and the module
	// list replaces it with the real mapping when somebody opens that view.
	//
	// The Windows backend fills the same three keys out of the image's version
	// resources. Two platforms, two places, one set of columns.
	//
	if (!Stat.IsKernelThread && !m_FileName.isEmpty() && m_pModuleInfo.isNull())
	{
		QSharedPointer<CLinuxModule> pModule = QSharedPointer<CLinuxModule>(new CLinuxModule());
		pModule->SetSystem(GetSystem());
		pModule->InitStaticData(m_FileName, 0, 0);

		QMap<QString, QString> FileInfo;

		//
		// GenericName first - "Web Browser" says what the thing is, which is
		// what this column is for - then the Comment, which is a sentence, then
		// the entry's Name, which at least beats nothing.
		//
		if (!Desktop.GenericName.isEmpty())	FileInfo["Description"] = Desktop.GenericName;
		else if (!Desktop.Comment.isEmpty()) FileInfo["Description"] = Desktop.Comment;
		else if (!Desktop.Name.isEmpty())	FileInfo["Description"] = Desktop.Name;

		if (!Package.Maintainer.isEmpty())	FileInfo["CompanyName"] = Package.Maintainer;
		if (!Package.Version.isEmpty())		FileInfo["FileVersion"] = Package.Version;
		if (!Desktop.Categories.isEmpty())	FileInfo["Categories"] = Desktop.Categories;

		pModule->SetFileInfos(FileInfo);
		pModule->SetPackageVerification((CModuleInfo::EVerifyResult)Package.Verify, Package.Maintainer);

		//
		// And the icon, which is the one thing here that is bytes rather than
		// text. Read once per image and cached; empty for the many programs
		// that have no desktop entry, which is better than a wrong icon.
		//
		const QByteArray Icon = LinuxExecutableIcon(m_FileName);
		if (!Icon.isEmpty())
			pModule->SetFileIcon(Icon);


		m_pModuleInfo = pModule;
	}

	//
	// Naming. There are two names here and Windows has the same two.
	//
	//   The image name - what file is running. That is the exe basename, and it
	//   is never truncated.
	//
	//   comm, which is what the process calls itself and what ps and top show.
	//   The kernel truncates it to 15 characters ("systemd-journald" arrives as
	//   "systemd-journal"), and a program may set it to something that is not a
	//   file name at all: every one of Firefox's children runs the firefox
	//   binary and calls itself "Web Content", "Utility Process", "WebExtensions".
	//
	// So the name column gets the image, as it does on Windows, and comm goes
	// where Windows puts the image's description - see GetDescription. Which of
	// the two is shown, and in which order, is then the same setting on both
	// platforms rather than a separate rule here.
	//
	// Two exceptions to "the name is the exe basename":
	//
	//   A Wine process's exe link points at the Wine loader, so the image is
	//   taken from the Windows image the prefix reports - otherwise every
	//   program in a prefix is called "wine-preloader", and the one thing that
	//   tells them apart is the comm the loader never sets.
	//
	//   A version-directory layout puts the version where the name should be:
	//   Claude Code installs as ".../claude/versions/2.1.220", so the basename
	//   is "2.1.220" and says nothing. A basename of digits and dots is not a
	//   program name, and comm is used instead.
	//
	// Kernel threads get the conventional [brackets]. Note that an unreadable
	// exe link does NOT mean kernel thread - see ProcFs::SStat::IsKernelThread.
	//
	m_Comm = Stat.Comm;

	if (Stat.IsKernelThread)
	{
		m_ProcessName = "[" + Stat.Comm + "]";
	}
	else
	{
		QString ImageName;
		if (Wine.Valid)
		{
			//
			// A Windows path, so it is split on the separator that path uses.
			//
			// And nothing at all rather than the exe link when the prefix has
			// not said: a Wine process's exe is the loader, so that link names
			// "wine-preloader" for every program in the prefix. comm is the
			// better answer there - Wine sets it to the image name, which is
			// how "TaskHelper.exe" is known before the prefix has been asked
			// anything.
			//
			if (!Wine.ImagePath.isEmpty())
				ImageName = Wine.ImagePath.mid(Wine.ImagePath.lastIndexOf(QLatin1Char('\\')) + 1);
		}
		else if (!ExePath.isEmpty())
		{
			//
			// A running program whose file has been replaced or removed - an
			// upgrade while it runs - has " (deleted)" appended to the exe link
			// by the kernel. That belongs on the path, where it says something,
			// and not in the name column.
			//
			QString Path = ExePath;
			if (Path.endsWith(QLatin1String(" (deleted)")))
				Path.chop(10);
			ImageName = QFileInfo(Path).fileName();
		}

		static const QRegularExpression VersionOnly("^[0-9][0-9.]*$");
		if (ImageName.isEmpty() || VersionOnly.match(ImageName).hasMatch())
			m_ProcessName = Stat.Comm;
		else
			m_ProcessName = ImageName;
	}

	// argv is NUL separated; joining with spaces matches what the Windows
	// backend shows, at the cost of being ambiguous for arguments containing
	// spaces. The raw list is still available from /proc if ever needed.
	m_CommandLine = CmdLine.join(' ');

	m_Uid = Uid;
	m_Gid = Gid;
	m_UserName = ProcFs::UserNameFromUid(Uid);

	//
	// The numeric uid, which is what the account actually is here - the name is
	// whatever /etc/passwd maps it to today and differs between machines.
	//
	m_UserKey = QString::number(Uid);

	// Read once: a process does not normally migrate between cgroups, and this
	// would otherwise be an extra file read per process per refresh.
	m_ServiceName = ServiceUnit.Name;
	m_bSystemService = ServiceUnit.bSystemSlice;

	m_CGroupPath = CGroupPath;
	m_Namespaces = Namespaces;
	m_Confinement = Confinement;
	m_Container = Container;

	//
	// The confinement column carries the LSM profile; a snap or a flatpak adds
	// the mode it was started in, which is the part that says how much of that
	// profile is actually enforced - a classic snap is confined by nothing.
	//
	if (Sandbox.Valid)
	{
		m_SandboxPermissions = Sandbox.Permissions;
		if (!Sandbox.Confinement.isEmpty())
			m_Confinement = m_Confinement.isEmpty() ? Sandbox.Confinement
													: (m_Confinement + ", " + Sandbox.Confinement);
	}

	m_StartTimeTicks = Stat.StartTime;
	// Real start time rather than "when we first saw it", so the new-process
	// highlight is correct for processes that predate our startup.
	m_CreateTimeStamp = StartTimeMs;

	m_LastStat = Stat;

	return true;
}

void CLinuxProcess::SetKernelThreadTasks(const QList<quint64>& Pids, quint64 UTime, quint64 STime)
{
	QWriteLocker Locker(&m_Mutex);
	m_bKernelThreadHost = true;
	m_KernelTaskPids = Pids;
	m_KernelTaskUTime = UTime;
	m_KernelTaskSTime = STime;
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

	quint32 NumThreads = (quint32)Stat.NumThreads;
	// There is no cheap fd count; the kernel exposes it only by listing the
	// directory, which is a syscall per process per refresh.
	const quint32 NumHandles = (quint32)ProcFs::EnumFds(Pid).count();

	bool bChanged = false;

	QWriteLocker Locker(&m_Mutex);

	bChanged |= (m_State != Stat.State);
	m_State = Stat.State;
	m_Nice = (qint32)Stat.Nice;
	m_SchedPolicy = (qint32)Stat.Policy;

	//
	// The host answers for all of them. Its own /proc entry says one thread and
	// a few milliseconds of cpu, which is true of kthreadd the task and useless
	// as a description of what the kernel's threads are doing.
	//
	quint64 UTime = Stat.UTime;
	quint64 STime = Stat.STime;
	if (m_bKernelThreadHost)
	{
		NumThreads += (quint32)m_KernelTaskPids.count();
		UTime += m_KernelTaskUTime;
		STime += m_KernelTaskSTime;
	}

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

	m_KernelTime = STime;
	m_UserTime = UTime;

	//
	// What the kernel is enforcing on this process, from two lines of the status
	// file this refresh read anyway. Per refresh rather than once, because a
	// program can install a seccomp filter after it has started - which is
	// exactly what a sandbox launcher does to the program it is about to exec.
	//
	m_MitigationFlags = LinuxMitigationFlags(Status);

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

	//
	// A sum over a set that changes: kworkers come and go, so when one exits the
	// total drops and that refresh shows no cpu rather than a negative amount.
	// One quiet sample rather than a wrong one, and the alternative - carrying a
	// per-task history for threads that live for milliseconds - costs more than
	// the blip is worth.
	//
	m_CpuStats.CpuKernelDelta.Update(STime);
	m_CpuStats.CpuUserDelta.Update(UTime);
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

//
// e_machine, from the ELF header. Reported in the same numbering CModuleInfo
// uses for PE images so that one column reads for either kind of target; the
// values do not overlap, so nothing has to be said about which is which.
//
quint16 CLinuxProcess::GetArchitecture() const
{
	QString FileName = GetFileName();
	if (FileName.isEmpty())
		return 0; // kernel thread

	QFile File(FileName);
	if (!File.open(QIODevice::ReadOnly))
		return 0;

	const QByteArray Header = File.read(20);
	if (Header.size() < 20 || !Header.startsWith("\x7f" "ELF"))
		return 0;

	//
	// EI_DATA, the sixth byte, says which end the rest of the header is written
	// from. Only little-endian machines are read here; the big-endian ones this
	// could run on are not among the four architectures named below.
	//
	if (Header[5] != 1)
		return 0;

	const quint16 Machine = (quint8)Header[18] | ((quint16)(quint8)Header[19] << 8);
	switch (Machine)
	{
		case 3:		return CModuleInfo::eMachineI386;	// EM_386
		case 40:	return CModuleInfo::eMachineArmNt;	// EM_ARM
		case 62:	return CModuleInfo::eMachineAmd64;	// EM_X86_64
		case 183:	return CModuleInfo::eMachineArm64;	// EM_AARCH64
	}
	return 0;
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

//
// /proc reports the run state as a letter; it crosses as one of EProcessState
// so a viewer on any platform can read it without knowing the letters.
//
int CLinuxProcess::GetRunState() const
{
	QReadLocker Locker(&m_Mutex);
	switch (m_State)
	{
	case 'R':	return eStateRunning;
	case 'S':	return eStateSleeping;
	case 'D':	return eStateDiskSleep;
	case 'Z':	return eStateZombie;
	case 'T':	return eStateStopped;
	case 't':	return eStateTracingStop;
	case 'I':	return eStateIdle;
	case 'X':
	case 'x':	return eStateDead;
	case 'W':	return eStateWaking;
	case 'P':	return eStateParked;
	}
	return eStateUnknown;
}

quint32 CLinuxProcess::GetStatusFlags() const
{
	QReadLocker Locker(&m_Mutex);

	quint32 Flags = 0;

	//
	// A kernel thread has no user space at all; root owns the machine. Both are
	// "not somebody's program", which is what this flag means on Windows too.
	//
	if (m_IsKernelThread || m_Uid == 0)
		Flags |= eStatusSystemProcess;

	if (m_bSystemService)
		Flags |= eStatusService;

	//
	// Root, or holding capabilities an ordinary user does not - the same test
	// IsElevated makes, inline because that one takes the lock this holds.
	//
	if (m_Uid == 0 || m_CapEff != 0)
		Flags |= eStatusElevated;

	//
	// A container is the nearest thing here to what Windows calls a sandbox: a
	// process that cannot see the same machine the rest of them can.
	//
	// Only a named one. LinuxDescribeContainer falls back to "namespaced (mnt)"
	// for anything whose namespaces merely differ from pid 1's, and that is true
	// of every systemd unit with PrivateTmp=yes - polkit, rsyslog and logind
	// among them. Those are hardened daemons, not sandboxes, and calling them
	// sandboxed took the daemon colour away from half the machine.
	//
	if (!m_Container.isEmpty() && !m_Container.startsWith("namespaced"))
		Flags |= eStatusSandboxed;

	//
	// A zombie has exited and is being kept only until its parent reads its
	// status. "Terminated" is exactly what Windows means by the same flag.
	//
	// 'Z' is what /proc/<pid>/stat reports for it; GetRunState translates the
	// same letter, but that one takes the lock this already holds.
	if (m_State == 'Z')
		Flags |= eStatusTerminated;

	//
	// And one this backend knows about that /proc cannot say: a process that has
	// gone but is still on the list, because something still refers to it - a
	// panel showing it, or a child that would otherwise lose its place in the
	// tree. The Windows backend reports the same thing from the same mark.
	//
	// It matters most for what somebody else sees. A viewer watching this
	// machine over the wire has no list of its own to compare against; all it
	// knows about a process is what arrives, and without this a parent that
	// exited an hour ago arrives looking exactly like one that is running.
	//
	if (m_RemoveTimeStamp != 0)
		Flags |= eStatusTerminated;

	if (m_Wine.Valid)
		Flags |= eStatusWine;

	return Flags;
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
		return ERR(TE_CannotDebugSelf);

	if (HasDebugger())
		return ERR(TE_ProcAlreadyTraced);

	//
	// Windows looks up the system's postmortem debugger in the AeDebug registry
	// key. Linux has no such registry, so the debugger is whichever of the two
	// usual ones is installed - both take "-p <pid>" to attach.
	//
	QString Debugger = QStandardPaths::findExecutable("gdb");
	if (Debugger.isEmpty())
		Debugger = QStandardPaths::findExecutable("lldb");
	if (Debugger.isEmpty())
		return ERR(TE_NoDebuggerInstalled);

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
		return ERR(TE_ProcTraced);

	const ProcFs::SStat Tracer = ProcFs::ReadStat(TracerPid);
	const QString Name = Tracer.Valid ? Tracer.Comm : MakePlaceholder(TE_NAME_UNKNOWN_TRACER);

	return ERR(TE_DebuggerItselfDetach, QVariantList() << Name << TracerPid);
}

bool CLinuxProcess::HasPriorityBoost() const
{
	// Linux has no per-process priority boost toggle.
	return false;
}

STATUS CLinuxProcess::SetPriorityBoost(bool Value)
{
	return ERR(TE_PriorityBoostUnsupported);
}

STATUS CLinuxProcess::SetPriority(qint32 Value)
{
	// Lowering the nice value (raising priority) requires CAP_SYS_NICE, so this
	// is expected to fail for unprivileged callers going below 0.
	errno = 0;
	if (setpriority(PRIO_PROCESS, (id_t)GetProcessId(), Value) != 0 && errno != 0)
		return ErrnoToStatus(TE_SetProcessPriorityFailed);

	QWriteLocker Locker(&m_Mutex);
	m_Nice = Value;
	m_Priority = Value;
	return OK;
}

STATUS CLinuxProcess::SetBasePriority(qint32 Value)
{
	//
	// The scheduling policy would be sched_setscheduler, but nothing in the
	// shared GUI ever calls this - the Windows base priority is read only, so
	// no menu is wired to it. Left as an explicit refusal rather than an
	// unreachable implementation.
	//
	return ERR(TE_SettingSchedulingPolicy);
}

STATUS CLinuxProcess::SetPagePriority(qint32 Value)
{
	return ERR(TE_PagePriorityUnsupported);
}

STATUS CLinuxProcess::SetIOPriority(qint32 Value)
{
	//
	// The GUI hands over a raw ioprio value as produced by LinuxMakeIoPrio.
	// Raising into the realtime class needs CAP_SYS_ADMIN, so that is expected
	// to fail unprivileged.
	//
	if (LinuxSetIoPrio(GetProcessId(), Value) != 0)
		return ErrnoToStatus(TE_SetIoPriorityFailed);

	QWriteLocker Locker(&m_Mutex);
	m_IoPrio = Value;
	m_IOPriority = Value;
	return OK;
}

STATUS CLinuxProcess::SetOomScoreAdj(int Value)
{
	if (Value < -1000 || Value > 1000)
		return ERR(TE_OomAdjustmentRange);

	if (!ProcFs::WriteOomScoreAdj(GetProcessId(), Value))
	{
		//
		// Raising the value is unprivileged; lowering it below what the process
		// already has needs CAP_SYS_RESOURCE, because that would protect a
		// process from the OOM killer at everyone else's expense.
		//
		if (errno == EACCES || errno == EPERM)
			return ERR(TE_LoweringOomAdjustment, errno);
		return ErrnoToStatus(TE_SetOomAdjustFailed);
	}

	QWriteLocker Locker(&m_Mutex);
	m_OomScoreAdj = Value;
	return OK;
}

STATUS CLinuxProcess::SetAffinityMask(quint64 Value)
{
	if (Value == 0)
		return ERR(TE_AffinityMaskEmpty);

	if (!LinuxSetAffinity(GetProcessId(), Value))
		return ErrnoToStatus(TE_SetAffinityFailed);

	QWriteLocker Locker(&m_Mutex);
	m_AffinityMask = Value;
	return OK;
}

STATUS CLinuxProcess::Terminate(bool bForce)
{
	// SIGTERM asks politely and can be caught or ignored; SIGKILL cannot be,
	// which matches what "force" means on the Windows side.
	if (kill((pid_t)GetProcessId(), bForce ? SIGKILL : SIGTERM) != 0)
		return ErrnoToStatus(TE_TerminateProcessFailed);
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
		return ErrnoToStatus(TE_SuspendProcessFailed);
	return OK;
}

STATUS CLinuxProcess::Resume()
{
	if (kill((pid_t)GetProcessId(), SIGCONT) != 0)
		return ErrnoToStatus(TE_ResumeProcessFailed);
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
	return ERR(TE_EditingEnvironmentRunning);
}

STATUS CLinuxProcess::EditEnvVariable(const QString& Name, const QString& Value)
{
	return ERR(TE_EditingEnvironmentRunning);
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
		pMemory->SetSystem(GetSystem());
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
	return ERR(TE_FlushingHeapsUnsupported);
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
	return ERR(TE_LoadingModuleRunning);
}

bool CLinuxProcess::UpdateThreads()
{
	const quint64 Pid = GetProcessId();

	QList<quint64> Tids = ProcFs::EnumThreads(Pid);
	if (Tids.isEmpty())
		return false; // process is gone

	//
	// And every kernel thread, as a thread of this one.
	//
	// Each of them is a single task by construction - kthread_create makes one -
	// so the task id is the pid, and there is no directory to read to find that
	// out. The pair (pid, tid) is what CLinuxThread reads from, and for these it
	// is (K, K): /proc/K/task/K, which is where a kernel thread's stat, name and
	// scheduling live.
	//
	QMap<quint64, quint64> ForeignTasks;	// tid -> the pid it belongs to
	if (m_bKernelThreadHost)
	{
		QReadLocker Locker(&m_Mutex);
		foreach(quint64 KPid, m_KernelTaskPids)
		{
			Tids.append(KPid);
			ForeignTasks.insert(KPid, KPid);
		}
	}

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
			// Destroyed through deleteLater; see CWindowsAPI::UpdateProcessList.
			pThread = QSharedPointer<CLinuxThread>(new CLinuxThread(), &QObject::deleteLater);
			pThread->SetSystem(GetSystem());
			if (!pThread->InitStaticData(ForeignTasks.value(Tid, Pid), Tid))
				continue; // exited between the readdir and the stat read

			GetSystem()->AddThread(pThread);

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
			GetSystem()->ClearThread(Tid);
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
				pHandle->SetSystem(GetSystem());
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
			pHandle->SetSystem(GetSystem());
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

	//
	// And the other half, for a process inside a Wine prefix.
	//
	// The descriptors above are what the kernel gave it; these are the events,
	// mutexes, keys and sections it opened through Wine, which live in
	// wineserver and which /proc cannot see at all. One process, two tables, one
	// list - kept apart by the key here and by the type numbering, so a filter
	// set to one kind can never match the other. See CWineHandle.
	//
	const SWineInfo Wine = GetWineInfo();
	if (Wine.Valid && Wine.WinPid && CWineHelpers::IsAvailable())
	{
		const int Bits = CWineHelpers::PrefixBits(Wine.Prefix);
		if (Bits && CWineHelpers::IsAvailable(Bits))
		{
			CWineHelper* pHelper = CWineHelpers::Instance()->Get(Wine.Prefix, Bits, GetUid());

			CVariant Handles;
			if (pHelper->ListHandles(Wine.WinPid, Handles, 5000))
			{
				try {
					Handles.ReadRawList([&](const CVariant& Entry)
					{
						CVariant Value;
						if (!Entry.Find("Handle", Value))
							return;

						//
						// Above every descriptor number there can be. A Windows
						// handle value and a small fd would otherwise collide in
						// this map, and the two would take turns overwriting
						// each other every round.
						//
						const quint64 Key = c_WineHandleKey | Value.To<quint64>();

						QSharedPointer<CWineHandle> pHandle = OldHandles.take(Key).staticCast<CWineHandle>();
						if (pHandle.isNull())
						{
							pHandle = QSharedPointer<CWineHandle>(new CWineHandle());
							pHandle->SetSystem(GetSystem());
							if (!pHandle->Apply(Pid, Entry))
								return;

							QWriteLocker Locker(&m_HandleMutex);
							m_HandleList.insert(Key, pHandle);
							Locker.unlock();

							Added.insert(Key);
							return;
						}

						//
						// A handle value is reused the moment it is closed, so
						// what is at one is not necessarily what was there last
						// round. Compared rather than assumed unchanged.
						//
						const quint32 WasType = pHandle->GetTypeIndex();
						const quint32 WasAccess = pHandle->GetGrantedAccess();
						const quint64 WasObject = pHandle->GetObjectAddress();
						const QString WasName = pHandle->GetFileName();

						if (!pHandle->Apply(Pid, Entry))
							return;

						if (WasType != pHandle->GetTypeIndex() || WasAccess != pHandle->GetGrantedAccess()
						 || WasObject != pHandle->GetObjectAddress() || WasName != pHandle->GetFileName())
							Changed.insert(Key);
					});
				} catch (...) {
					//
					// A malformed reply costs this round's wineserver objects
					// and nothing else; the descriptors above are already in.
					//
				}
			}
		}
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

	//
	// Directly where the file can be read, through the elevated helper where it
	// cannot - which for an unprivileged viewer is every process but its own.
	//
	// The same fallback the working directory, the environment and the file
	// descriptors already use. Without it the Modules tab of another user's
	// process was simply never filled, and - because a failed update emits
	// nothing - the panel went on showing the modules of whatever was selected
	// before it.
	//
	QList<ProcFs::SMapEntry> Maps = ProcFs::ReadMaps(Pid);
	if (Maps.isEmpty() && LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
		Maps = ProcFs::ParseMaps(LinuxHelperReadProcFile(Pid, "maps"));

	if (Maps.isEmpty())
		return false; // gone, or not readable even with help

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
			pModule->SetSystem(GetSystem());
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

QString CLinuxProcess::GetDescription() const
{
	QReadLocker Locker(&m_Mutex);

	//
	// Only when it is a name of its own. A comm equal to the image name, or a
	// prefix of it - which is what the kernel's truncation looks like - repeats
	// the name column and crowds out the image's description.
	//
	if (!m_Comm.isEmpty() && !m_ProcessName.startsWith(m_Comm))
		return m_Comm;

	Locker.unlock();
	return CProcessInfo::GetDescription();
}

bool CLinuxProcess::UpdateWindows()
{
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	if (X11Helper::IsAvailable())
	{
		//
		// The list is refreshed for every process by CLinuxAPI::UpdateProcessList
		// via SetWindows(), so an explicit refresh only has to re-read the ones we
		// already know about. Enumerating the whole display again here would repeat
		// work that was just done.
		//
		QReadLocker ReadLocker(&m_WindowMutex);
		const QList<quint64> Keys = m_WindowList.keys();
		const QList<CWndPtr> Windows = m_WindowList.values();
		ReadLocker.unlock();

		for (int i = 0; i < Windows.count(); i++)
		{
			//
			// The Wine ones are not X11 windows and have no such data to re-read;
			// they are refreshed below, from the prefix.
			//
			if (Keys[i] & c_WineWindowKey)
				continue;

			if (Windows[i].staticCast<CLinuxWnd>()->UpdateDynamicData())
				Changed.insert(Windows[i]->GetHWnd());
		}
	}

	//
	// And the same process's windows as user32 sees them.
	//
	// A Wine top-level window is an X11 window too, so the loop above may well
	// have one for it already - but what X11 has is the X11 window. The HWND,
	// the class the program registered, the styles it was created with are
	// user32's, and only something running inside the prefix can ask. Both are
	// listed; see CWineWnd for why neither replaces the other.
	//
	const SWineInfo Wine = GetWineInfo();
	if (Wine.Valid && Wine.WinPid && CWineHelpers::IsAvailable())
	{
		const int Bits = CWineHelpers::PrefixBits(Wine.Prefix);
		if (Bits && CWineHelpers::IsAvailable(Bits))
		{
			CWineHelper* pHelper = CWineHelpers::Instance()->Get(Wine.Prefix, Bits, GetUid());

			CVariant Windows;
			if (pHelper->ListWindows(Wine.WinPid, Windows, 5000))
			{
				QMap<quint64, CWndPtr> OldWine;
				{
					QReadLocker Locker(&m_WindowMutex);
					for (QMap<quint64, CWndPtr>::const_iterator I = m_WindowList.begin(); I != m_WindowList.end(); ++I)
						if (I.key() & c_WineWindowKey)
							OldWine.insert(I.key(), I.value());
				}

				try {
					Windows.ReadRawList([&](const CVariant& Entry)
					{
						CVariant Value;
						if (!Entry.Find("Wnd", Value))
							return;

						const quint64 Key = c_WineWindowKey | Value.To<quint64>();

						QSharedPointer<CWineWnd> pWnd = OldWine.take(Key).staticCast<CWineWnd>();
						if (pWnd.isNull())
						{
							pWnd = QSharedPointer<CWineWnd>(new CWineWnd());
							pWnd->SetSystem(GetSystem());
							//
							// Which prefix to go back to when something is done
							// to this window, so that acting on it does not have
							// to find its way to a process first.
							//
							pWnd->SetTarget(Wine.Prefix, Bits, GetUid());
							if (!pWnd->Apply(GetProcessId(), Entry))
								return;

							QWriteLocker Locker(&m_WindowMutex);
							m_WindowList.insert(Key, pWnd);
							Locker.unlock();

							Added.insert(Key);
							return;
						}

						//
						// Compared rather than assumed unchanged: a title changes
						// while a program runs, and so does whether a window is
						// visible or iconic.
						//
						const QString WasTitle = pWnd->GetWindowTitle();
						const bool bWasVisible = pWnd->IsVisible();
						const bool bWasMinimized = pWnd->IsMinimized();
						const bool bWasMaximized = pWnd->IsMaximized();

						if (!pWnd->Apply(GetProcessId(), Entry))
							return;

						if (WasTitle != pWnd->GetWindowTitle() || bWasVisible != pWnd->IsVisible()
						 || bWasMinimized != pWnd->IsMinimized() || bWasMaximized != pWnd->IsMaximized())
							Changed.insert(Key);
					});

					//
					// Whatever the prefix no longer reports has been destroyed.
					// Only the Wine ones are considered here - the X11 side has
					// its own accounting in SetWindows.
					//
					QWriteLocker Locker(&m_WindowMutex);
					foreach(quint64 Key, OldWine.keys())
					{
						m_WindowList.remove(Key);
						Removed.insert(Key);
					}
				} catch (...) {
					//
					// A malformed reply costs this round's Wine windows and
					// nothing else; the X11 ones are already in.
					//
				}
			}
		}
	}

	//
	// Emitted unconditionally, even when nothing changed.
	//
	// CWindowsView calls this as a "send me the current state" request and only
	// populates itself from the resulting signal - so staying silent because
	// there was no change leaves the view permanently empty, since the window
	// list was already filled in by SetWindows() before the view ever asked.
	//
	emit WindowsUpdated(Added, Changed, Removed);

	return true;
}

//
// A window by the handle it reports, which is not always its key here.
//
// An X11 window is filed under its window id and reports the same number, so
// the map answers directly. A Wine window is filed above every X11 id, because
// the two sets would otherwise collide, but reports the HWND that the program
// inside the prefix knows it by - and that is the number that goes over the
// wire and comes back in an action.
//
CWndPtr CLinuxProcess::GetWindow(quint64 hWnd) const
{
	QReadLocker Locker(&m_WindowMutex);

	CWndPtr pWnd = m_WindowList.value(hWnd);
	if (!pWnd.isNull())
		return pWnd;

	return m_WindowList.value(c_WineWindowKey | hWnd);
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
			pWnd->SetSystem(GetSystem());
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
		//
		// Except the ones that were never X11's to report. A Wine window is
		// accounted for against what the prefix says in UpdateWindows, and
		// dropping it here would delete it a moment after it was added.
		//
		if (Window & c_WineWindowKey)
			continue;

		// CWndInfo derives from CAbstractInfo, which has no removal-persistence
		// machinery, so closed windows are dropped immediately.
		m_WindowList.remove(Window);
		Removed.insert(Window);
	}
	Locker.unlock();

	emit WindowsUpdated(Added, Changed, Removed);

	return true;
}

//
// The collector reads the files, not the viewer.
//
// These were called straight out of CCGroupView, which meant the window read
// its own machine's /sys/fs/cgroup whatever process was selected. Here they
// answer for the machine the process is actually on, and a remote process can
// override them with what came over the wire.
//
SCGroupStats CLinuxProcess::GetCGroupStats() const
{
	const QString Path = GetCGroupPath();
	if (Path.isEmpty())
		return SCGroupStats();
	return ProcFs::ReadCGroupStats(Path);
}

SResourcePressure CLinuxProcess::GetCGroupPressure(const QString& Resource) const
{
	const QString Path = GetCGroupPath();
	if (Path.isEmpty())
		return SResourcePressure();
	return ProcFs::ReadCGroupPressure(Path, Resource);
}

SProcessSecurity CLinuxProcess::GetProcessSecurity() const
{
	return ProcFs::ReadProcSecurity(GetProcessId());
}
