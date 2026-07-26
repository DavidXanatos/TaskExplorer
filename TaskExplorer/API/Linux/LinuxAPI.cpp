#include "stdafx.h"
#include "LinuxAPI.h"
#include "LinuxHelper.h"
#include "ProcFs.h"
#include "SockDiag.h"
#include "X11Helper.h"

#include "LinuxHandle.h"
#include "UdevMonitor.h"

#include "Monitors/LinuxDiskMonitor.h"
#include "Monitors/LinuxGpuMonitor.h"
#include "Monitors/LinuxNetMonitor.h"

#include "../../GUI/TaskExplorer.h"
#include "../../../MiscHelpers/Common/Settings.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusReply>

#include <sys/utsname.h>
#include <unistd.h>

CLinuxAPI::CLinuxAPI(QObject *parent)
	: CSystemAPI(parent)
{
	m_bFullAccess = false;
	m_pUdevMonitor = nullptr;
	m_bDnsAuthAttempted = false;
}

CLinuxAPI::~CLinuxAPI()
{
}

bool CLinuxAPI::Init()
{
	InitSystemInfo();
	InitCpuInfo();

	m_bFullAccess = RootAvaiable();

	m_pGpuMonitor = new CLinuxGpuMonitor();
	m_pGpuMonitor->Init();

	m_pNetMonitor = new CLinuxNetMonitor();
	m_pNetMonitor->Init();

	m_pDiskMonitor = new CLinuxDiskMonitor();
	m_pDiskMonitor->Init();

	//
	// Hotplug watch. Created here rather than in the constructor because this
	// runs on the API worker thread, which is where its socket notifier has to
	// live to be serviced by an event loop.
	//
	// If it cannot start, hotplugged devices are simply picked up on the next
	// manual refresh instead of immediately - not worth reporting as an error.
	//
	m_pUdevMonitor = new CUdevMonitor(this);
	if (m_pUdevMonitor->Init())
	{
		//
		// Function-pointer connect, not the SIGNAL/SLOT macros.
		//
		// NotifyHardwareChanged() is an ordinary virtual on CSystemAPI rather
		// than a declared slot, so the string form compiled fine and then
		// failed at runtime with "No such slot" - meaning hotplug events were
		// received and silently dropped. This form is checked by the compiler,
		// so the same mistake cannot come back.
		//
		connect(m_pUdevMonitor, &CUdevMonitor::HardwareChanged, this, &CLinuxAPI::NotifyHardwareChanged);
	}

	return true;
}

//
// Locates a distribution logo.
//
// The primary source is a DistroLogos folder next to the executable, holding
// plain PNGs named after the os-release ID. Keeping them as loose files rather
// than compiled-in resources means a logo can be added or corrected for a new
// distribution without rebuilding - and avoids shipping third-party trademarks
// inside the binary.
//
// Lookup order, most specific first:
//
//   DistroLogos/<ID>-<VERSION_ID>.png    ubuntu-26.04.png
//   DistroLogos/<ID>.png                 ubuntu.png
//   DistroLogos/<ID_LIKE entry>.png      debian.png       (for derivatives)
//   DistroLogos/linux.png                generic fallback
//
// If none of those exist the distribution's own installed logo is used: the
// os-release LOGO key names an icon-theme entry, and most distributions also
// drop a PNG in /usr/share/pixmaps.
//
static QPixmap FindDistroLogo(const QMap<QString, QString>& OsRelease)
{
	const QString LogoDir = QCoreApplication::applicationDirPath() + "/DistroLogos/";

	const QString Id = OsRelease.value("ID").toLower();
	const QString VersionId = OsRelease.value("VERSION_ID");

	QStringList Candidates;
	if (!Id.isEmpty())
	{
		if (!VersionId.isEmpty())
			Candidates.append(LogoDir + Id + "-" + VersionId + ".png");
		Candidates.append(LogoDir + Id + ".png");
	}

	// ID_LIKE is a space separated list of parent distributions, so a
	// derivative with no logo of its own falls back to its base.
	for (const QString& Like : OsRelease.value("ID_LIKE").toLower().split(' ', Qt::SkipEmptyParts))
		Candidates.append(LogoDir + Like + ".png");

	Candidates.append(LogoDir + "linux.png");

	for (const QString& Path : Candidates)
	{
		QPixmap Logo(Path);
		if (!Logo.isNull())
			return Logo;
	}

	// Fall back to whatever the distribution itself installed.
	const QString LogoName = OsRelease.value("LOGO");
	if (!LogoName.isEmpty())
	{
		const QIcon ThemeIcon = QIcon::fromTheme(LogoName);
		if (!ThemeIcon.isNull())
		{
			const QPixmap Logo = ThemeIcon.pixmap(128, 128);
			if (!Logo.isNull())
				return Logo;
		}

		QPixmap Logo("/usr/share/pixmaps/" + LogoName + ".png");
		if (!Logo.isNull())
			return Logo;
	}

	if (!Id.isEmpty())
	{
		QPixmap Logo("/usr/share/pixmaps/" + Id + "-logo.png");
		if (!Logo.isNull())
			return Logo;
	}

	// Nothing found; the system view simply shows no icon.
	return QPixmap();
}

bool CLinuxAPI::InitSystemInfo()
{
	const QMap<QString, QString> OsRelease = ProcFs::ReadOsRelease();
	const QPixmap Logo = FindDistroLogo(OsRelease);

	QWriteLocker Locker(&m_Mutex);

	struct utsname Uts;
	if (uname(&Uts) == 0)
	{
		m_SystemType = QString::fromUtf8(Uts.machine);
		m_SystemVersion = QString::fromUtf8(Uts.release);
		m_SystemBuild = QString::fromUtf8(Uts.version);
		m_HostName = QString::fromUtf8(Uts.nodename);
	}

	// PRETTY_NAME is the distribution's own display name.
	m_SystemName = OsRelease.value("PRETTY_NAME");
	if (m_SystemName.isEmpty())
		m_SystemName = OsRelease.value("NAME");
	if (m_SystemName.isEmpty())
		m_SystemName = "Linux";

	m_SystemIcon = Logo;

	m_UserName = ProcFs::UserNameFromUid(getuid());
	m_SystemDir = "/";

	return true;
}

bool CLinuxAPI::InitCpuInfo()
{
	const ProcFs::SCpuInfo CpuInfo = ProcFs::ReadCpuInfo();
	const QMap<QString, quint64> MemInfo = ProcFs::ReadMemInfo();

	// NUMA nodes are counted from /sys, which cpuinfo does not describe.
	int NumaCount = QDir("/sys/devices/system/node").entryList(QStringList("node*"), QDir::Dirs).count();

	{
		QWriteLocker Locker(&m_Mutex);
		m_CPU_String = CpuInfo.ModelName;
	}

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_CpuCount = CpuInfo.Logical;
	m_CoreCount = CpuInfo.Cores;
	m_PackageCount = CpuInfo.Packages;
	m_NumaCount = NumaCount > 0 ? NumaCount : 1;

	m_CpuBaseClock = CpuInfo.MHz;
	m_CpuCurrentClock = CpuInfo.MHz;

	m_CpusStats.resize(m_CpuCount);

	m_InstalledMemory = MemInfo.value("MemTotal");

	return true;
}

quint64 CLinuxAPI::UpdateCpuStats()
{
	const ProcFs::SSysStat SysStat = ProcFs::ReadSysStat();
	if (!SysStat.Valid)
		return 0;

	QWriteLocker StatsLocker(&m_StatsMutex);

	//
	// /proc/stat counters are cumulative clock ticks since boot, so everything
	// here is delta based. The first sample after startup has no predecessor
	// and yields a zero delta, which shows as 0% for one refresh.
	//
	const ProcFs::SCpuTime& Cur = SysStat.Total;
	const ProcFs::SCpuTime& Prev = m_LastSysStat.Total;

	quint64 TotalDelta = 0;
	if (m_LastSysStat.Valid)
	{
		const quint64 CurTotal = Cur.Total();
		const quint64 PrevTotal = Prev.Total();
		// Guard against a counter going backwards, which happens across
		// suspend/resume and when a cpu is hot-unplugged.
		TotalDelta = (CurTotal > PrevTotal) ? (CurTotal - PrevTotal) : 0;
	}

	// Kernel time is system + irq + softirq; user time is user + nice. Idle and
	// iowait are deliberately excluded from both.
	m_CpuStats.KernelDelta.Update(Cur.System + Cur.Irq + Cur.SoftIrq);
	m_CpuStats.UserDelta.Update(Cur.User + Cur.Nice);
	m_CpuStats.IdleDelta.Update(Cur.Idle + Cur.IoWait);

	if (TotalDelta > 0)
	{
		m_CpuStats.KernelUsage = (float)m_CpuStats.KernelDelta.Delta / TotalDelta;
		m_CpuStats.UserUsage = (float)m_CpuStats.UserDelta.Delta / TotalDelta;
	}

	m_CpuStats.ContextSwitchesDelta.Update64(SysStat.ContextSwitches);
	m_CpuStats.InterruptsDelta.Update64(SysStat.Interrupts);

	// Per-cpu usage for the CPU graph grid.
	const int Count = qMin(SysStat.PerCpu.size(), m_CpusStats.size());
	for (int i = 0; i < Count; i++)
	{
		const ProcFs::SCpuTime& CpuCur = SysStat.PerCpu[i];
		SCpuStats& Stats = m_CpusStats[i];

		quint64 CpuTotalDelta = 0;
		if (m_LastSysStat.Valid && i < m_LastSysStat.PerCpu.size())
		{
			const quint64 CurTotal = CpuCur.Total();
			const quint64 PrevTotal = m_LastSysStat.PerCpu[i].Total();
			CpuTotalDelta = (CurTotal > PrevTotal) ? (CurTotal - PrevTotal) : 0;
		}

		Stats.KernelDelta.Update(CpuCur.System + CpuCur.Irq + CpuCur.SoftIrq);
		Stats.UserDelta.Update(CpuCur.User + CpuCur.Nice);
		Stats.IdleDelta.Update(CpuCur.Idle + CpuCur.IoWait);

		if (CpuTotalDelta > 0)
		{
			Stats.KernelUsage = (float)Stats.KernelDelta.Delta / CpuTotalDelta;
			Stats.UserUsage = (float)Stats.UserDelta.Delta / CpuTotalDelta;
		}
	}

	m_LastSysStat = SysStat;

	return TotalDelta;
}

void CLinuxAPI::UpdateMemStats()
{
	const QMap<QString, quint64> MemInfo = ProcFs::ReadMemInfo();
	const QList<ProcFs::SSwapArea> Swaps = ProcFs::ReadSwaps();

	quint64 TotalSwap = 0;
	quint64 UsedSwap = 0;
	QList<SPageFile> PageFiles;
	for (const ProcFs::SSwapArea& Area : Swaps)
	{
		TotalSwap += Area.Size;
		UsedSwap += Area.Used;

		SPageFile PageFile;
		PageFile.Path = Area.Path;
		PageFile.TotalSize = Area.Size;
		PageFile.TotalInUse = Area.Used;
		// The kernel does not record a high-water mark for swap usage.
		PageFile.PeakUsage = Area.Used;
		PageFiles.append(PageFile);
	}

	const quint64 MemTotal = MemInfo.value("MemTotal");
	const quint64 MemAvailable = MemInfo.value("MemAvailable");

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_InstalledMemory = MemTotal;
	//
	// MemAvailable is the kernel's own estimate of what can be allocated
	// without swapping - a better answer than MemFree, which excludes
	// reclaimable page cache and makes every Linux box look nearly full.
	//
	m_AvailableMemory = MemAvailable ? MemAvailable : MemInfo.value("MemFree");
	m_PhysicalUsed = (MemTotal > m_AvailableMemory) ? (MemTotal - m_AvailableMemory) : 0;

	m_CacheMemory = MemInfo.value("Cached") + MemInfo.value("Buffers");

	// Committed_AS is the counterpart of Windows' commit charge, and
	// CommitLimit of the commit limit.
	m_CommitedMemory = MemInfo.value("Committed_AS");
	m_MemoryLimit = MemInfo.value("CommitLimit");
	if (m_CommitedMemory > m_CommitedMemoryPeak)
		m_CommitedMemoryPeak = m_CommitedMemory;

	m_TotalSwapMemory = TotalSwap;
	m_SwapedOutMemory = UsedSwap;
	m_PageFiles = PageFiles;

	//
	// Linux has no paged/non-paged pool split. Slab is the nearest equivalent
	// to kernel allocations, so it is reported split by reclaimability rather
	// than left at zero.
	//
	m_PagedPool = MemInfo.value("SReclaimable");
	m_NonPagedPool = MemInfo.value("SUnreclaim");
	m_PersistentPagedPool = 0;

	m_KernelMemory = MemInfo.value("Slab") + MemInfo.value("KernelStack") + MemInfo.value("PageTables");
	m_DriverMemory = 0; // not separately accounted for
	m_ReservedMemory = 0;
}

bool CLinuxAPI::RootAvaiable()
{
	if (geteuid() == 0)
		return true;

	//
	// Not root, but a binary granted CAP_SYS_PTRACE (via setcap, or a systemd
	// unit's AmbientCapabilities) can still read other users' process details,
	// which is what this flag actually gates. CapEff is the effective
	// capability set as a hex bitmask.
	//
	// CAP_SYS_PTRACE is bit 19; CAP_SYS_ADMIN is bit 21 and implies it.
	//
	const QString CapEff = ProcFs::ReadStatus(getpid()).value("CapEff");
	if (CapEff.isEmpty())
		return false;

	bool bOk = false;
	const quint64 Caps = CapEff.toULongLong(&bOk, 16);
	if (!bOk)
		return false;

	const quint64 CAP_SYS_PTRACE_BIT = 1ULL << 19;
	const quint64 CAP_SYS_ADMIN_BIT  = 1ULL << 21;
	return (Caps & (CAP_SYS_PTRACE_BIT | CAP_SYS_ADMIN_BIT)) != 0;
}

bool CLinuxAPI::UpdateAll()
{
	// Process list first: it refreshes the cpu totals that the other passes and
	// the per-process usage percentages are computed against.
	UpdateProcessList();
	UpdateSocketList();
	UpdateSysStats();
	UpdateServiceList();
	return true;
}

bool CLinuxAPI::UpdateSysStats()
{
	m_pGpuMonitor->UpdateGpuStats();
	m_pNetMonitor->UpdateNetStats();
	m_pDiskMonitor->UpdateDiskStats();

	UpdateMemStats();

	//
	// Pressure Stall Information. Three small files, sampled with the rest of
	// the system counters; absent when the kernel was built without CONFIG_PSI,
	// in which case the graphs simply have nothing to draw.
	//
	{
		const ProcFs::SPressure Cpu = ProcFs::ReadSysPressure("cpu");
		const ProcFs::SPressure Memory = ProcFs::ReadSysPressure("memory");
		const ProcFs::SPressure Io = ProcFs::ReadSysPressure("io");

		QWriteLocker Locker(&m_StatsMutex);
		m_CpuPressure = Cpu;
		m_MemoryPressure = Memory;
		m_IoPressure = Io;
	}

	//
	// Roll the per-device counters up into the system totals that the status
	// bar and the summary graphs read.
	//
	// The Windows backend derives these by summing each process's own I/O
	// counters, which it can do because it gets them for every process. On
	// Linux /proc/<pid>/io needs ptrace access, so per-process totals are only
	// available for our own processes - the device counters are the only
	// system-wide source.
	//
	quint64 DiskRead = 0, DiskReadCount = 0, DiskWrite = 0, DiskWriteCount = 0;
	for (const CDiskMonitor::SDiskInfo& Disk : m_pDiskMonitor->GetDiskList())
	{
		DiskRead += Disk.ReadRaw;
		DiskReadCount += Disk.ReadCount;
		DiskWrite += Disk.WriteRaw;
		DiskWriteCount += Disk.WriteCount;
	}

	quint64 NetRecv = 0, NetRecvCount = 0, NetSend = 0, NetSendCount = 0;
	for (const CNetMonitor::SNicInfo& Nic : m_pNetMonitor->GetNicList(false))
	{
		NetRecv += Nic.ReceiveRaw;
		NetRecvCount += Nic.ReceiveCount;
		NetSend += Nic.SendRaw;
		NetSendCount += Nic.SendCount;
	}

	//
	// Memory-mapped I/O: the traffic between the page cache and the block
	// devices, from /proc/vmstat.
	//
	// This is the counterpart of what the Windows backend reads out of the
	// cache manager and paging counters. pgpgin/pgpgout are in kilobytes -
	// confirmed against /proc/diskstats, whose sector counts come out at
	// exactly twice these numbers.
	//
	// It necessarily overlaps the disk figures above, as it does on Windows:
	// what the cache writes back does reach the disk. The two differ where I/O
	// bypasses the cache (O_DIRECT) or never reaches it (a cache hit).
	//
	const QMap<QString, quint64> VmStat = ProcFs::ReadVmStat();
	const quint64 PageIn = VmStat.value("pgpgin") * 1024;
	const quint64 PageOut = VmStat.value("pgpgout") * 1024;

	//
	// File I/O: the logical read/write traffic of every process, i.e. what the
	// programs themselves asked for, cache hits included. This is the figure
	// that differs most from the disk counters and the reason the graph is
	// worth having.
	//
	// Summed from the per-process counters, which UpdateProcessList has already
	// refreshed this cycle - so this costs no extra reads.
	//
	// Deltas are accumulated into a running total rather than summing the
	// processes' cumulative values directly: a process exiting would otherwise
	// make the total jump backwards and produce a nonsense rate.
	//
	// Note /proc/<pid>/io is readable only for one's own processes without
	// privileges, so unprivileged this covers the current user's processes and
	// running elevated it covers everything.
	//
	quint64 IoReadDelta = 0, IoReadOps = 0, IoWriteDelta = 0, IoWriteOps = 0;
	foreach(const CProcessPtr& pProcess, GetProcessList())
	{
		const SProcStats Stats = pProcess->GetStats();
		IoReadDelta += Stats.Io.ReadRawDelta.Delta;
		IoReadOps += Stats.Io.ReadDelta.Delta;
		IoWriteDelta += Stats.Io.WriteRawDelta.Delta;
		IoWriteOps += Stats.Io.WriteDelta.Delta;
	}

	QWriteLocker StatsLocker(&m_StatsMutex);

	m_Stats.Disk.SetRead(DiskRead, DiskReadCount);
	m_Stats.Disk.SetWrite(DiskWrite, DiskWriteCount);
	m_Stats.Net.SetReceive(NetRecv, NetRecvCount);
	m_Stats.Net.SetSend(NetSend, NetSendCount);

	m_Stats.MMapIo.SetRead(PageIn, 0);
	m_Stats.MMapIo.SetWrite(PageOut, 0);

	m_TotalIoRead += IoReadDelta;
	m_TotalIoReadOps += IoReadOps;
	m_TotalIoWrite += IoWriteDelta;
	m_TotalIoWriteOps += IoWriteOps;

	m_Stats.Io.SetRead(m_TotalIoRead, m_TotalIoReadOps);
	m_Stats.Io.SetWrite(m_TotalIoWrite, m_TotalIoWriteOps);
	// Linux has no counterpart of the Windows "other" I/O operations
	// (ioctl and friends are not counted separately), so this stays at zero.
	m_Stats.Io.SetOther(0, 0);

	m_Stats.UpdateStats();

	return true;
}

bool CLinuxAPI::UpdateProcessList()
{
	//
	// Total cpu ticks elapsed across all cpus since the last refresh. Per
	// process usage is (process delta / this), so a process pinning one core on
	// an 8 core box reads 12.5%.
	//
	// "Linux style" instead divides by the per-cpu total, so the same process
	// reads 100%. The setting is shared with the Windows backend.
	//
	const quint64 SysTotalTime = UpdateCpuStats();
	const int iLinuxStyleCPU = theConf->GetInt("Options/LinuxStyleCPU", 2);
	const quint64 SysTotalTimePerCPU = m_CpuCount ? (SysTotalTime / m_CpuCount) : SysTotalTime;
	const quint64 CpuDivisor = (iLinuxStyleCPU == 1) ? SysTotalTimePerCPU : SysTotalTime;

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	const QList<quint64> Pids = ProcFs::EnumProcesses();
	if (Pids.isEmpty())
		return false;

	quint32 NewTotalProcesses = 0;
	quint32 NewTotalThreads = 0;
	quint32 NewTotalHandles = 0;

	QList<CProcessPtr> NewProcessList;

	// Everything left in here at the end has exited.
	QMap<SProcessUID, CProcessPtr> OldProcesses = GetProcessMap();

	for (quint64 Pid : Pids)
	{
		//
		// The UID pairs the pid with its start time, so a recycled pid produces
		// a different key and is correctly treated as a new process rather than
		// silently inheriting the old one's history.
		//
		const ProcFs::SStat Stat = ProcFs::ReadStat(Pid);
		if (!Stat.Valid)
			continue; // exited between the readdir and this read

		const SProcessUID UID(Pid, ProcFs::StartTimeToEpochMs(Stat.StartTime));

		QSharedPointer<CLinuxProcess> pProcess = OldProcesses.take(UID).staticCast<CLinuxProcess>();
		bool bAdd = false;
		if (pProcess.isNull())
		{
			QWriteLocker Locker(&m_ProcessMutex);
			CProcessPtr& pProcessRef = m_ProcessMap[UID];
			if (pProcessRef.isNull())
			{
				pProcessRef = QSharedPointer<CLinuxProcess>(new CLinuxProcess());
				m_ProcessByPID[Pid] = pProcessRef;
			}
			pProcess = pProcessRef.staticCast<CLinuxProcess>();
			Locker.unlock();

			if (!pProcess->InitStaticData(Pid))
			{
				// Vanished mid-init; drop it again rather than leave a half
				// initialised entry in the map.
				QWriteLocker RemoveLocker(&m_ProcessMutex);
				m_ProcessMap.remove(UID);
				m_ProcessByPID.remove(Pid);
				continue;
			}
			bAdd = true;
		}

		if (pProcess->GetParentUId().Get() == 0)
			NewProcessList.append(pProcess);

		const bool bChanged = pProcess->UpdateDynamicData(false, CpuDivisor, SysTotalTimePerCPU);

		if (bAdd)
			Added.insert(Pid);
		else if (bChanged)
			Changed.insert(Pid);

		NewTotalProcesses++;
		NewTotalThreads += pProcess->GetNumberOfThreads();
		NewTotalHandles += pProcess->GetNumberOfHandles();
	}

	//
	// Resolve parent links. This is a second pass because a child can be
	// enumerated before its parent, and the link is by UID rather than pid so
	// that it survives pid recycling.
	//
	foreach(auto& pProcess, NewProcessList)
	{
		auto pParent = GetProcessByID(pProcess->GetParentId()).objectCast<CLinuxProcess>();
		if (pParent && pProcess->ValidateParent(pParent.data()))
			pProcess->SetParentUId(pParent->GetProcessUId());
		else
		{
			//
			// No usable parent: pid 1 and pid 2 report ppid 0, and a reparented
			// orphan can name a pid that has already gone away.
			//
			// The synthetic UID must not be zero. CProcessModel::Sync() skips
			// any process whose parent UID is 0, and CLinuxAPI uses 0 as its
			// own "not yet resolved" sentinel above - so handing out 0 here
			// would make pid 1 and pid 2 invisible in the tree, leaving their
			// children stranded under an empty placeholder row.
			//
			// Pairing the parent pid with this process's own creation time
			// yields a stable, non-zero UID that deliberately matches no real
			// process, so MakeProcPath() treats it as a tree root.
			//
			pProcess->SetParentUId(SProcessUID(pProcess->GetParentId(), pProcess->GetCreateTimeStamp()));
		}
	}

	QMap<quint64, CProcessPtr> Processes = GetProcessList();

	//
	// Windows are enumerated once for the whole display and then distributed,
	// rather than each process enumerating for itself: EnumWindows() returns
	// every managed window regardless, so per-process enumeration would be
	// O(processes x windows) X round trips for the same answer.
	//
	// Doing it here also means every process has its window list populated, so
	// the process tree's window menu works without the Windows tab having been
	// opened first.
	//
	if (X11Helper::IsAvailable())
	{
		QMap<quint64, QList<X11Helper::SWindow>> WindowsByPid;
		for (const X11Helper::SWindow& Window : X11Helper::EnumWindows())
		{
			if (Window.ProcessId)
				WindowsByPid[Window.ProcessId].append(Window);
		}

		for (auto I = Processes.begin(); I != Processes.end(); ++I)
		{
			auto pProcess = I.value().objectCast<CLinuxProcess>();
			if (pProcess.isNull())
				continue;

			//
			// Processes with no windows still need the call, so that windows
			// which have just closed are removed from their list.
			//
			const QList<X11Helper::SWindow> Windows = WindowsByPid.value(I.key());
			if (Windows.isEmpty() && pProcess->GetWindowList().isEmpty())
				continue; // nothing to do, and by far the common case

			pProcess->SetWindows(Windows);
		}
	}

	// Parent retention: keep an exited process listed while it still has live
	// children, so the tree does not reshuffle underneath them.
	QMap<quint64, int> ChildCount;
	if (theConf->GetBool("Options/EnableParentRetention", true))
	{
		foreach(const CProcessPtr& pProcess, Processes) {
			CProcessPtr pParent = Processes.value(pProcess->GetParentId());
			if (!pParent.isNull() && pProcess->ValidateParent(pParent.data()))
				ChildCount[pProcess->GetParentId()]++;
		}
	}

	// Purge whatever is left over - those pids are no longer running.
	QWriteLocker Locker(&m_ProcessMutex);
	foreach(const SProcessUID& UID, OldProcesses.keys())
	{
		CProcessPtr pProcess = m_ProcessMap.value(UID);
		if (pProcess.isNull())
			continue;

		if (pProcess->CanBeRemoved() && !ChildCount.contains(pProcess->GetProcessId()))
		{
			auto F = m_ProcessByPID.find(pProcess->GetProcessId());
			if (F != m_ProcessByPID.end()) {
				if (F.value() == pProcess)
					m_ProcessByPID.erase(F);
			}
			m_ProcessMap.remove(UID);
			Removed.insert(pProcess->GetProcessId());
		}
		else if (!pProcess->IsMarkedForRemoval())
		{
			// Held for the configured persistence interval so the row stays
			// visible (greyed) briefly after exit.
			pProcess->MarkForRemoval();
			Changed.insert(pProcess->GetProcessId());
		}
	}
	Locker.unlock();

	//
	// Fill in the I/O counters that could not be read directly.
	//
	// /proc/<pid>/io needs ptrace-level access, so unprivileged this is refused
	// for every process belonging to another user. Rather than show those rows
	// blank, the pids are collected and handed to an elevated TaskHelper in a
	// single request - one round trip per refresh, not one per process.
	//
	// Opt-in, because the first request raises an authentication prompt; a task
	// manager should not do that merely because it was started.
	//
	if (LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
		UpdateHelperProcIo();

	emit ProcessListUpdated(Added, Changed, Removed);

	QWriteLocker StatsLocker(&m_StatsMutex);
	m_TotalProcesses = NewTotalProcesses;
	m_TotalThreads = NewTotalThreads;
	m_TotalHandles = NewTotalHandles;

	return true;
}

//
// Finds an existing socket entry matching this endpoint tuple. The socket list
// is a multimap keyed by a hash of the tuple, so several entries can share a
// key and each candidate has to be compared properly.
//
static QMultiMap<quint64, CSocketPtr>::iterator FindSocketEntry(QMultiMap<quint64, CSocketPtr>& Sockets,
	quint64 ProcessId, quint32 ProtocolType, const QHostAddress& LocalAddress, quint16 LocalPort,
	const QHostAddress& RemoteAddress, quint16 RemotePort, CSocketInfo::EMatchMode Mode)
{
	const quint64 HashID = CSocketInfo::MkHash(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort);

	for (auto I = Sockets.find(HashID); I != Sockets.end() && I.key() == HashID; ++I)
	{
		if (I.value()->Match(ProcessId, ProtocolType, LocalAddress, LocalPort, RemoteAddress, RemotePort, Mode))
			return I;
	}

	return Sockets.end();
}

//
// Asks an elevated TaskHelper for the I/O counters of every process whose own
// /proc/<pid>/io could not be read.
//
void CLinuxAPI::UpdateHelperProcIo()
{
	QList<quint64> Pids;

	QReadLocker Locker(&m_ProcessMutex);
	foreach(const CProcessPtr& pProcess, m_ProcessMap)
	{
		auto pLinuxProcess = pProcess.objectCast<CLinuxProcess>();
		if (!pLinuxProcess.isNull() && pLinuxProcess->NeedsHelperProcIo())
			Pids.append(pLinuxProcess->GetProcessId());
	}
	Locker.unlock();

	if (Pids.isEmpty())
		return;

	const QMap<quint64, QMap<QString, QByteArray>> Files =
		LinuxHelperReadProcFiles(Pids, QStringList() << "io");

	if (Files.isEmpty())
		return;	// helper unavailable, or the user declined - leave the rows blank

	QReadLocker ReadLocker(&m_ProcessMutex);
	for (auto I = Files.begin(); I != Files.end(); ++I)
	{
		auto pProcess = m_ProcessByPID.value(I.key()).objectCast<CLinuxProcess>();
		if (!pProcess.isNull())
			pProcess->SetHelperProcIo(I.value().value("io"));
	}
}

bool CLinuxAPI::UpdateSocketList()
{
	//
	// sock_diag is preferred because it is the only source of per-socket byte
	// counters; /proc/net exposes queue depths only, which would leave the
	// transfer rate columns permanently at zero.
	//
	// It is unavailable on kernels built without CONFIG_INET_DIAG and in some
	// restricted containers, hence the fallback.
	//
	QList<ProcFs::SNetConnection> Connections = SockDiag::Enumerate();
	if (Connections.isEmpty())
		Connections = ProcFs::ReadNetConnections();

	//
	// /proc/net/* names the owning socket only by inode, so ownership has to be
	// resolved by scanning every process's fd table. That is the expensive part
	// of this refresh, so it is done once here rather than per connection.
	//
	const QMap<quint64, quint64> InodeToPid = ProcFs::BuildSocketInodeMap();

	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMultiMap<quint64, CSocketPtr> OldSockets = GetSocketList();

	for (const ProcFs::SNetConnection& Conn : Connections)
	{
		const quint64 ProcessId = InodeToPid.value(Conn.Inode, 0);

		auto I = FindSocketEntry(OldSockets, ProcessId, Conn.ProtocolType,
			Conn.LocalAddress, Conn.LocalPort, Conn.RemoteAddress, Conn.RemotePort, CSocketInfo::eStrict);

		QSharedPointer<CLinuxSocket> pSocket;
		bool bAdd = false;
		if (I == OldSockets.end())
		{
			pSocket = QSharedPointer<CLinuxSocket>(new CLinuxSocket());
			pSocket->InitStaticData(ProcessId, Conn);

			if (ProcessId)
			{
				// Note: bAddIfNew, so a socket belonging to a process we have
				// not enumerated yet still gets attributed.
				CProcessPtr pProcess = GetProcessByID(ProcessId, true);
				if (pProcess)
				{
					pSocket->LinkProcess(pProcess);
					pProcess->AddSocket(pSocket);
				}
			}

			QWriteLocker Locker(&m_SocketMutex);
			m_SocketList.insert(pSocket->GetHashID(), pSocket);
			Locker.unlock();

			bAdd = true;
		}
		else
		{
			pSocket = I.value().staticCast<CLinuxSocket>();
			OldSockets.erase(I);
		}

		const bool bChanged = pSocket->UpdateDynamicData(Conn);

		if (bAdd)
			Added.insert(pSocket->GetHashID());
		else if (bChanged)
			Changed.insert(pSocket->GetHashID());
	}

	// Anything still in OldSockets was not in this sample, i.e. it closed.
	QWriteLocker Locker(&m_SocketMutex);
	for (auto I = OldSockets.begin(); I != OldSockets.end(); ++I)
	{
		CSocketPtr pSocket = I.value();

		if (pSocket->CanBeRemoved())
		{
			if (CProcessPtr pProcess = pSocket->GetProcess().toStrongRef().staticCast<CProcessInfo>())
				pProcess->RemoveSocket(pSocket);
			m_SocketList.remove(I.key(), I.value());
			Removed.insert(I.key());
		}
		else
		{
			// Keep it listed briefly, greyed, the way the Windows backend does.
			if (pSocket->GetState() != (quint32)-1)
				pSocket->SetClosed();
			if (!pSocket->IsMarkedForRemoval())
				pSocket->MarkForRemoval();
			Changed.insert(I.key());
		}
	}
	Locker.unlock();

	emit SocketListUpdated(Added, Changed, Removed);

	return true;
}

bool CLinuxAPI::UpdateOpenFileList()
{
	//
	// The system-wide open file list, which is what the Files view shows.
	//
	// This walks every process's fd table, so it is the most expensive refresh
	// in the backend. CSystemAPI::UpdateOpenFileListAsync() runs it on a worker
	// so the UI does not stall; the Windows backend is structured the same way.
	//
	// Only real files are listed - sockets, pipes and anonymous inodes have
	// their own views and would swamp this one.
	//
	QSet<quint64> Added;
	QSet<quint64> Changed;
	QSet<quint64> Removed;

	QMap<quint64, CHandlePtr> OldHandles = GetOpenFilesList();

	for (quint64 Pid : ProcFs::EnumProcesses())
	{
		for (quint64 Fd : ProcFs::EnumFds(Pid))
		{
			//
			// Pre-filter on the raw link target so a CLinuxHandle is only built
			// for entries that will actually be listed. The full InitStaticData
			// does several more reads per fd.
			//
			const QString Target = ProcFs::ReadLink(ProcFs::ProcPath(Pid, QString("fd/%1").arg(Fd)));
			if (Target.isEmpty() || !Target.startsWith('/'))
				continue; // closed, not permitted, or a socket/pipe/anon_inode

			//
			// The key must be unique across processes; an fd number alone is
			// not, since every process has an fd 0.
			//
			const quint64 HandleId = (Pid << 32) | (Fd & 0xFFFFFFFF);

			QSharedPointer<CLinuxHandle> pHandle = OldHandles.take(HandleId).staticCast<CLinuxHandle>();
			bool bAdd = false;
			if (pHandle.isNull())
			{
				pHandle = QSharedPointer<CLinuxHandle>(new CLinuxHandle());
				if (!pHandle->InitStaticData(Pid, Fd))
					continue;

				pHandle->SetProcess(GetProcessByID(Pid));

				QWriteLocker Locker(&m_OpenFilesMutex);
				m_OpenFilesList.insert(HandleId, pHandle);
				Locker.unlock();

				bAdd = true;
			}

			if (bAdd)
				Added.insert(HandleId);
		}
	}

	QWriteLocker Locker(&m_OpenFilesMutex);
	foreach(quint64 HandleId, OldHandles.keys())
	{
		CHandlePtr pHandle = m_OpenFilesList.value(HandleId);
		if (pHandle.isNull())
			continue;

		if (pHandle->CanBeRemoved())
		{
			m_OpenFilesList.remove(HandleId);
			Removed.insert(HandleId);
		}
		else if (!pHandle->IsMarkedForRemoval())
		{
			pHandle->MarkForRemoval();
			Changed.insert(HandleId);
		}
	}
	Locker.unlock();

	emit OpenFileListUpdated(Added, Changed, Removed);

	return true;
}

//
// One element of what systemd's ListUnits returns: (ssssssouso).
//
struct SSystemdUnit
{
	QString	Name;
	QString	Description;
	QString	LoadState;
	QString	ActiveState;
	QString	SubState;
	QString	Following;
	QDBusObjectPath	Path;
	quint32	JobId = 0;
	QString	JobType;
	QDBusObjectPath	JobPath;
};

Q_DECLARE_METATYPE(SSystemdUnit)

static const QDBusArgument& operator>>(const QDBusArgument& Argument, SSystemdUnit& Unit)
{
	Argument.beginStructure();
	Argument >> Unit.Name >> Unit.Description >> Unit.LoadState >> Unit.ActiveState
	         >> Unit.SubState >> Unit.Following >> Unit.Path >> Unit.JobId
	         >> Unit.JobType >> Unit.JobPath;
	Argument.endStructure();
	return Argument;
}

static QDBusArgument& operator<<(QDBusArgument& Argument, const SSystemdUnit& Unit)
{
	Argument.beginStructure();
	Argument << Unit.Name << Unit.Description << Unit.LoadState << Unit.ActiveState
	         << Unit.SubState << Unit.Following << Unit.Path << Unit.JobId
	         << Unit.JobType << Unit.JobPath;
	Argument.endStructure();
	return Argument;
}

bool CLinuxAPI::UpdateServiceList(bool bRefresh)
{
	//
	// systemd is the service manager on every mainstream distribution now, and
	// it is reachable over the system bus without privileges for reads.
	// Start/stop go through the same interface and are authorised by polkit,
	// which is why this does not need the UI to run as root.
	//
	// Systems without systemd (or without a running system bus) simply report
	// no services rather than failing.
	//
	static bool bTypeRegistered = false;
	if (!bTypeRegistered)
	{
		qDBusRegisterMetaType<SSystemdUnit>();
		qDBusRegisterMetaType<QList<SSystemdUnit>>();
		bTypeRegistered = true;
	}

	QDBusConnection Bus = QDBusConnection::systemBus();
	if (!Bus.isConnected())
		return false;

	QDBusInterface Manager("org.freedesktop.systemd1", "/org/freedesktop/systemd1",
	                       "org.freedesktop.systemd1.Manager", Bus);
	if (!Manager.isValid())
		return false;

	QDBusReply<QList<SSystemdUnit>> Reply = Manager.call("ListUnits");
	if (!Reply.isValid())
		return false;

	QSet<QString> Added;
	QSet<QString> Changed;
	QSet<QString> Removed;

	QMap<QString, CServicePtr> OldServices = GetServiceList();

	const QList<SSystemdUnit> Units = Reply.value();
	for (const SSystemdUnit& Unit : Units)
	{
		//
		// ListUnits returns every unit type. A Windows service list has only
		// one kind of entry, but systemd manages several that are worth seeing:
		// a .timer is what replaced cron, a .socket is what starts a daemon on
		// demand, a .mount is a filesystem, a .slice is a resource group.
		//
		// Two types are deliberately excluded. .device units are one-to-one
		// mirrors of udev devices - over a hundred of them here, none
		// controllable, all noise next to real services. .target units are
		// synchronisation points with no process and no state worth watching;
		// they would add another sixty rows that never do anything.
		//
		// The type is shown in its own column, so the list can be sorted or
		// filtered back down to services alone.
		//
		static const QStringList InterestingTypes = {
			".service", ".socket", ".timer", ".mount", ".automount",
			".path", ".swap", ".scope", ".slice",
		};

		bool bInteresting = false;
		for (const QString& Type : InterestingTypes)
		{
			if (Unit.Name.endsWith(Type))
			{
				bInteresting = true;
				break;
			}
		}

		if (!bInteresting)
			continue;

		QSharedPointer<CLinuxService> pService = OldServices.take(Unit.Name).staticCast<CLinuxService>();
		bool bAdd = false;
		if (pService.isNull())
		{
			pService = QSharedPointer<CLinuxService>(new CLinuxService());
			pService->InitStaticData(Unit.Name);
			pService->SetObjectPath(Unit.Path.path());

			QWriteLocker Locker(&m_ServiceMutex);
			m_ServiceList.insert(Unit.Name, pService);
			Locker.unlock();

			bAdd = true;
		}

		bool bChanged = pService->UpdateDynamicData(Unit.LoadState, Unit.ActiveState,
		                                            Unit.SubState, Unit.Description);

		//
		// MainPID and FragmentPath each cost a D-Bus round trip per unit, and a
		// typical system has a few hundred units - fetching them for everything
		// on every refresh would make this pass dominate the update cycle.
		//
		// A running unit is the only case where a pid is meaningful, so the
		// full fetch is limited to those. Everything else gets the unit file
		// path only, and only once (FetchProperties caches it).
		//
		const bool bRunning = (Unit.ActiveState == "active" || Unit.ActiveState == "activating");
		if (bRunning)
			bChanged |= pService->FetchProperties(false);
		else if (bAdd)
			bChanged |= pService->FetchProperties(true);

		if (bAdd)
			Added.insert(Unit.Name);
		else if (bChanged)
			Changed.insert(Unit.Name);
	}

	QWriteLocker Locker(&m_ServiceMutex);
	foreach(const QString& Name, OldServices.keys())
	{
		CServicePtr pService = m_ServiceList.value(Name);
		if (pService.isNull())
			continue;

		if (pService->CanBeRemoved())
		{
			m_ServiceList.remove(Name);
			Removed.insert(Name);
		}
		else if (!pService->IsMarkedForRemoval())
		{
			pService->MarkForRemoval();
			Changed.insert(Name);
		}
	}
	Locker.unlock();

	emit ServiceListUpdated(Added, Changed, Removed);

	return true;
}

bool CLinuxAPI::UpdateDriverList()
{
	//
	// /proc/modules, one loaded module per line:
	//   name size refcount deps state offset
	//   nvidia_drm 122880 8 nvidia,drm_kms_helper Live 0xffffffffc0e00000
	// "deps" is "-" when nothing depends on the module.
	//
	const QByteArray Data = ProcFs::ReadFile("/proc/modules");
	if (Data.isEmpty())
		return false; // no loadable module support, or /proc not mounted

	QSet<QString> Added;
	QSet<QString> Changed;
	QSet<QString> Removed;

	QMap<QString, CDriverPtr> OldDrivers = GetDriverList();

	for (const QByteArray& Line : Data.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		const QList<QByteArray> F = Line.simplified().split(' ');
		if (F.size() < 5)
			continue;

		const QString Name = QString::fromUtf8(F[0]);
		const quint64 Size = F[1].toULongLong();
		const quint32 RefCount = F[2].toUInt();
		const QString UsedBy = (F[3] == "-") ? QString() : QString::fromUtf8(F[3]);
		const QString State = QString::fromUtf8(F[4]);

		QSharedPointer<CLinuxDriver> pDriver = OldDrivers.take(Name).staticCast<CLinuxDriver>();
		bool bAdd = false;
		if (pDriver.isNull())
		{
			pDriver = QSharedPointer<CLinuxDriver>(new CLinuxDriver());
			pDriver->InitStaticData(Name);

			QWriteLocker Locker(&m_DriverMutex);
			m_DriverList.insert(Name, pDriver);
			Locker.unlock();

			bAdd = true;
		}

		const bool bChanged = pDriver->UpdateDynamicData(Size, RefCount, UsedBy, State);

		if (bAdd)
			Added.insert(Name);
		else if (bChanged)
			Changed.insert(Name);
	}

	QWriteLocker Locker(&m_DriverMutex);
	foreach(const QString& Name, OldDrivers.keys())
	{
		CDriverPtr pDriver = m_DriverList.value(Name);
		if (pDriver.isNull())
			continue;

		if (pDriver->CanBeRemoved())
		{
			m_DriverList.remove(Name);
			Removed.insert(Name);
		}
		else if (!pDriver->IsMarkedForRemoval())
		{
			pDriver->MarkForRemoval();
			Changed.insert(Name);
		}
	}
	Locker.unlock();

	emit DriverListUpdated(Added, Changed, Removed);

	return true;
}

void CLinuxAPI::ClearPersistence()
{
	//
	// Resets the removal timestamps, so entries that are being held on screen
	// after they exited become eligible for purging on the next refresh.
	//
	foreach(const CProcessPtr& pProcess, GetProcessList())
		pProcess->ClearPersistence();

	foreach(const CSocketPtr& pSocket, GetSocketList())
		pSocket->ClearPersistence();

	foreach(const CHandlePtr& pHandle, GetOpenFilesList())
		pHandle->ClearPersistence();

	foreach(const CServicePtr& pService, GetServiceList())
		pService->ClearPersistence();

	foreach(const CDriverPtr& pDriver, GetDriverList())
		pDriver->ClearPersistence();
}

quint64 CLinuxAPI::GetUpTime() const
{
	return (quint64)ProcFs::UpTime();
}

QList<CSystemAPI::SUser> CLinuxAPI::GetUsers() const
{
	QList<SUser> Users;

	//
	// logind's ListSessions returns (susso): session id, uid, user name, seat,
	// object path. It is the authoritative source on any systemd system, and
	// unlike utmp it does not go stale when something exits uncleanly.
	//
	QDBusConnection Bus = QDBusConnection::systemBus();
	if (!Bus.isConnected())
		return Users;

	QDBusInterface Manager("org.freedesktop.login1", "/org/freedesktop/login1",
	                       "org.freedesktop.login1.Manager", Bus);
	if (!Manager.isValid())
		return Users;

	QDBusMessage Reply = Manager.call("ListSessions");
	if (Reply.type() != QDBusMessage::ReplyMessage || Reply.arguments().isEmpty())
		return Users;

	const QDBusArgument Argument = Reply.arguments().at(0).value<QDBusArgument>();
	Argument.beginArray();
	while (!Argument.atEnd())
	{
		QString SessionId;
		quint32 Uid = 0;
		QString UserName;
		QString Seat;
		QDBusObjectPath Path;

		Argument.beginStructure();
		Argument >> SessionId >> Uid >> UserName >> Seat >> Path;
		Argument.endStructure();

		SUser User;
		User.UserName = UserName;

		//
		// The GUI's "session id" column is numeric. logind session ids are
		// strings ("2", "c1" for a greeter), so the numeric prefix is used
		// where there is one.
		//
		User.SessionId = SessionId.toUInt();

		//
		// State is one of online / active / closing. Reading it costs a
		// property fetch per session, but there are only ever a handful.
		//
		QDBusInterface Session("org.freedesktop.login1", Path.path(),
		                       "org.freedesktop.DBus.Properties", Bus);
		if (Session.isValid())
		{
			QDBusReply<QVariant> StateReply = Session.call("Get", "org.freedesktop.login1.Session", "State");
			if (StateReply.isValid())
				User.Status = StateReply.value().toString();
		}
		if (User.Status.isEmpty())
			User.Status = Seat.isEmpty() ? tr("remote") : Seat;

		Users.append(User);
	}
	Argument.endArray();

	return Users;
}

QMultiMap<QString, CDnsCacheEntryPtr> CLinuxAPI::GetDnsEntryList() const
{
	QReadLocker Locker(&m_DnsMutex);
	return m_DnsCache;
}

//
// Talks varlink to systemd-resolved and returns the parsed reply.
//
// The cache contents are deliberately not on resolved's D-Bus interface, which
// offers only FlushCaches and CacheStatistics. DumpCache lives on the varlink
// socket instead, and it is what "resolvectl show-cache" itself calls.
//
// Varlink is a trivial protocol: one JSON object per message, terminated by a
// NUL byte. That is little enough to speak directly, which avoids a dependency
// on libsystemd for one call.
//
static QJsonObject CallResolvedVarlink(const QString& Method, const QJsonObject& Parameters, bool bAllowInteractive)
{
	//
	// The socket is world accessible, but the methods on it are polkit
	// protected. org.freedesktop.resolve1.dump-cache is auth_admin_keep, so
	// reading the cache costs an administrator authentication, remembered for a
	// while afterwards.
	//
	// Interactive authentication therefore has to be requested sparingly - see
	// UpdateDnsCache. Without the flag the call simply fails when the caller is
	// not already authorised, which is the right behaviour for a view that
	// refreshes on a timer.
	//
	QJsonObject Call;
	Call["method"] = Method;
	QJsonObject Params = Parameters;
	if (bAllowInteractive)
		Params["allowInteractiveAuthentication"] = true;
	Call["parameters"] = Params;

	QLocalSocket Socket;
	Socket.connectToServer("/run/systemd/resolve/io.systemd.Resolve.Monitor");
	if (!Socket.waitForConnected(1000))
		return QJsonObject();	// systemd-resolved is not running

	QByteArray Request = QJsonDocument(Call).toJson(QJsonDocument::Compact);
	Request.append('\0');
	Socket.write(Request);
	if (!Socket.waitForBytesWritten(1000))
		return QJsonObject();

	//
	// The reply can be large - a busy cache runs to hundreds of kilobytes - and
	// arrives in several chunks, so read until the terminating NUL.
	//
	// Whatever is already buffered has to be taken before waiting for more.
	// waitForBytesWritten above runs the socket's event handling, so a fast
	// reply can land while it is still executing; waitForReadyRead would then
	// sit waiting for data that had already arrived and time out with the
	// complete answer sitting unread in the buffer.
	//
	QByteArray Reply;
	forever
	{
		Reply.append(Socket.readAll());
		if (Reply.contains('\0'))
			break;

		if (!Socket.waitForReadyRead(3000))
			return QJsonObject();
	}
	Reply.truncate(Reply.indexOf('\0'));

	const QJsonObject Response = QJsonDocument::fromJson(Reply).object();

	// A varlink error reply carries "error" instead of "parameters".
	if (Response.contains("error"))
		return QJsonObject();

	return Response.value("parameters").toObject();
}

//
// Turns one resource record from resolved's JSON into the string the cache view
// shows in its "Resolved data" column.
//
// Which fields a record carries depends on its type; anything not recognised
// falls back to empty, and such records are skipped rather than listed blank.
//
static QString FormatResourceRecord(const QJsonObject& Record, quint16 Type, QHostAddress& Address)
{
	switch (Type)
	{
		case 1:		// A
		case 28:	// AAAA
		{
			//
			// The address arrives as an array of bytes, 4 for A and 16 for
			// AAAA, rather than as a formatted string.
			//
			const QJsonArray Bytes = Record.value("address").toArray();
			if (Bytes.count() == 4)
			{
				quint32 Raw = 0;
				for (int i = 0; i < 4; i++)
					Raw = (Raw << 8) | (quint8)Bytes[i].toInt();
				Address = QHostAddress(Raw);
			}
			else if (Bytes.count() == 16)
			{
				quint8 Raw[16];
				for (int i = 0; i < 16; i++)
					Raw[i] = (quint8)Bytes[i].toInt();
				Address = QHostAddress(Raw);
			}
			else
				return QString();

			return Address.toString();
		}

		case 5:		// CNAME
		case 12:	// PTR
		case 2:		// NS
			return Record.value("name").toString();

		case 15:	// MX
			return QString("%1 (priority %2)")
				.arg(Record.value("exchange").toString())
				.arg(Record.value("priority").toInt());

		case 16:	// TXT
		{
			QStringList Items;
			foreach(const QJsonValue& Item, Record.value("items").toArray())
				Items.append(Item.toString());
			return Items.join(' ');
		}

		case 33:	// SRV
			return QString("%1:%2")
				.arg(Record.value("name").toString())
				.arg(Record.value("port").toInt());

		default:
			return QString();
	}
}

bool CLinuxAPI::UpdateDnsCache()
{
	//
	// systemd-resolved is the only component on a stock Linux system that keeps
	// a DNS cache. Where it is not running there is nothing to report: glibc's
	// resolver does not cache, so an empty list is the correct answer rather
	// than a failure.
	//
	// Reading it needs administrator authentication (auth_admin_keep). This
	// view refreshes on a timer, so the prompt is offered exactly once per run:
	// after that the call is made non-interactively, which keeps working for as
	// long as polkit remembers the authentication and costs nothing when it
	// does not. Running elevated, no prompt appears at all.
	//
	// The alternative - passing the interactive flag every time - would put a
	// password dialog on screen every few seconds once the grace period lapsed.
	//
	QJsonObject Result = CallResolvedVarlink("io.systemd.Resolve.Monitor.DumpCache", QJsonObject(), false);
	if (Result.isEmpty() && !m_bDnsAuthAttempted)
	{
		m_bDnsAuthAttempted = true;
		Result = CallResolvedVarlink("io.systemd.Resolve.Monitor.DumpCache", QJsonObject(), true);
	}

	if (Result.isEmpty())
		return false;

	//
	// Record expiry is given as an absolute CLOCK_BOOTTIME deadline in
	// microseconds, so the remaining lifetime has to be measured against that
	// same clock - not against the wall clock, which moves independently.
	//
	struct timespec Now;
	clock_gettime(CLOCK_BOOTTIME, &Now);
	const qint64 NowUs = (qint64)Now.tv_sec * 1000000 + Now.tv_nsec / 1000;

	QMultiMap<QString, CDnsCacheEntryPtr> OldEntries;
	{
		QReadLocker Locker(&m_DnsMutex);
		OldEntries = m_DnsCache;
	}

	QMultiMap<QString, CDnsCacheEntryPtr> NewCache;
	bool bChanged = false;

	// One scope per protocol and interface; their caches are listed separately.
	foreach(const QJsonValue& ScopeValue, Result.value("dump").toArray())
	{
		const QJsonObject Scope = ScopeValue.toObject();

		foreach(const QJsonValue& EntryValue, Scope.value("cache").toArray())
		{
			const QJsonObject Entry = EntryValue.toObject();

			//
			// An entry with no "rrs" is a cached negative answer - proof that a
			// name does not resolve. There is no resolved data to show, and the
			// Windows side has no equivalent concept, so these are skipped.
			//
			if (!Entry.contains("rrs"))
				continue;

			const qint64 Until = (qint64)Entry.value("until").toDouble();
			// Negative once the record is past its deadline. resolved keeps such
			// records listed until it gets around to evicting them.
			const qint64 RemainingMs = (Until - NowUs) / 1000;

			foreach(const QJsonValue& RecordValue, Entry.value("rrs").toArray())
			{
				const QJsonObject Record = RecordValue.toObject().value("rr").toObject();
				const QJsonObject Key = Record.value("key").toObject();

				const QString HostName = Key.value("name").toString();
				const quint16 Type = (quint16)Key.value("type").toInt();
				if (HostName.isEmpty())
					continue;

				QHostAddress Address;
				const QString Resolved = FormatResourceRecord(Record, Type, Address);
				if (Resolved.isEmpty())
					continue;	// a record type this view has no way to render

				//
				// Reuse the existing object when the same record is still
				// cached, so that its creation timestamp and the view's
				// selection survive a refresh.
				//
				CDnsCacheEntryPtr pEntry;
				for (auto I = OldEntries.find(HostName); I != OldEntries.end() && I.key() == HostName; ++I)
				{
					if (I.value()->GetType() == Type && I.value()->GetResolvedString() == Resolved)
					{
						pEntry = I.value();
						OldEntries.erase(I);
						break;
					}
				}

				if (pEntry.isNull())
				{
					pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry(HostName, Type, Address, Resolved));
					bChanged = true;
				}

				//
				// A live record just gets its remaining lifetime. An expired one
				// is recorded as dead instead, which is what makes the view grey
				// it out - and the transition is done only once, because
				// SetTTL(0) would count a fresh query on every refresh and
				// SubtractTTL accumulates rather than assigns.
				//
				if (RemainingMs > 0)
					pEntry->SetTTL(RemainingMs);
				else if (pEntry->GetDeadTime() == 0)
					pEntry->SubtractTTL((quint64)-RemainingMs);
				NewCache.insert(HostName, pEntry);
			}
		}
	}

	// Anything left in OldEntries has expired out of resolved's cache.
	if (!OldEntries.isEmpty())
		bChanged = true;

	{
		QWriteLocker Locker(&m_DnsMutex);
		m_DnsCache = NewCache;
	}

	emit DnsCacheUpdated();

	return bChanged;
}

void CLinuxAPI::FlushDnsCache()
{
	//
	// systemd-resolved is the only component on a stock Linux system that keeps
	// a DNS cache worth flushing. Where it is not running there is nothing to
	// do - glibc's resolver does not cache.
	//
	QDBusConnection Bus = QDBusConnection::systemBus();
	if (!Bus.isConnected())
		return;

	QDBusInterface Manager("org.freedesktop.resolve1", "/org/freedesktop/resolve1",
	                       "org.freedesktop.resolve1.Manager", Bus);
	if (!Manager.isValid())
		return;

	// FlushCaches is polkit-protected, so allow the prompt the same way service
	// control does.
	QDBusMessage Call = QDBusMessage::createMethodCall("org.freedesktop.resolve1",
	                                                   "/org/freedesktop/resolve1",
	                                                   "org.freedesktop.resolve1.Manager",
	                                                   "FlushCaches");
	Call.setInteractiveAuthorizationAllowed(true);
	Bus.call(Call, QDBus::Block, 120 * 1000);
}

void CLinuxAPI::OnHardwareChanged()
{
	m_HardwareChangePending = false;

	//
	// Re-enumerate whatever may have changed. Each of these rebuilds its device
	// list from /sys, so a disk or interface that has just appeared or gone
	// away is picked up.
	//
	m_pGpuMonitor->UpdateAdapters();
	m_pNetMonitor->UpdateAdapters();
	m_pDiskMonitor->UpdateDisks();
}
