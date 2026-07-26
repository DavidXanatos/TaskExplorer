#pragma once

#include <qobject.h>
#include <QByteArray>
#include <QHostAddress>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

//
// Thin, dependency-free helpers for reading the /proc and /sys pseudo
// filesystems. Everything in API/Linux is built on top of these so that the
// parsing rules live in exactly one place.
//
// All functions are safe to call for a pid that disappears mid-read; they
// simply return empty/false rather than raising.
//

namespace ProcFs
{
	// ---- raw access ----

	// Reads a pseudo file whole. /proc files report st_size == 0, so this
	// cannot use QFile::size() and must read until EOF.
	QByteArray		ReadFile(const QString& Path);
	QString			ReadFileStr(const QString& Path);

	// Splits a NUL-separated pseudo file (cmdline, environ) into its parts,
	// dropping the trailing empty element these files usually carry.
	QStringList		ReadNulList(const QString& Path);

	QString			ReadLink(const QString& Path);

	bool			FileExists(const QString& Path);

	// ---- path builders ----

	QString			ProcPath(quint64 Pid, const QString& Leaf = QString());
	QString			TaskPath(quint64 Pid, quint64 Tid, const QString& Leaf = QString());

	// ---- enumeration ----

	// All numeric entries under /proc, i.e. the live pids.
	QList<quint64>	EnumProcesses();
	// All numeric entries under /proc/<pid>/task, i.e. the live tids.
	QList<quint64>	EnumThreads(quint64 Pid);
	// All numeric entries under /proc/<pid>/fd.
	QList<quint64>	EnumFds(quint64 Pid);

	// ---- structured readers ----

	//
	// /proc/<pid>/stat. The comm field is parenthesised and may itself contain
	// spaces and parentheses, so the fields cannot simply be split on
	// whitespace - Split() finds the LAST ')' and parses from there.
	//
	struct SStat
	{
		quint64	Pid = 0;
		QString	Comm;
		char	State = '\0';
		quint64	PPid = 0;
		quint64	PGrp = 0;
		quint64	Session = 0;
		quint64	TtyNr = 0;
		quint64	TPGid = 0;
		quint64	Flags = 0;
		quint64	MinFlt = 0;
		quint64	CMinFlt = 0;
		quint64	MajFlt = 0;
		quint64	CMajFlt = 0;
		quint64	UTime = 0;			// clock ticks
		quint64	STime = 0;			// clock ticks
		qint64	CUTime = 0;
		qint64	CSTime = 0;
		qint64	Priority = 0;
		qint64	Nice = 0;
		qint64	NumThreads = 0;
		quint64	StartTime = 0;		// clock ticks since boot
		quint64	VSize = 0;			// bytes
		qint64	Rss = 0;			// pages
		quint64	RssLim = 0;
		quint64	StartCode = 0;
		quint64	EndCode = 0;
		quint64	StartStack = 0;
		quint64	KstkEsp = 0;
		quint64	KstkEip = 0;
		quint64	Processor = 0;
		quint64	RtPriority = 0;
		quint64	Policy = 0;
		quint64	DelayAcctBlkIOTicks = 0;
		quint64	GuestTime = 0;

		//
		// PF_KTHREAD from the Flags field.
		//
		// This is the only reliable way to identify a kernel thread. The
		// tempting alternative - "readlink /proc/<pid>/exe returned nothing" -
		// is wrong, because that link is also unreadable for any process owned
		// by another user when running unprivileged, which would misreport a
		// large number of ordinary root-owned daemons as kernel threads.
		//
		// /proc/<pid>/stat is world readable, so this works regardless of
		// privileges.
		//
		bool	IsKernelThread = false;

		bool	Valid = false;
	};

	SStat			ReadStat(quint64 Pid);
	SStat			ReadThreadStat(quint64 Pid, quint64 Tid);
	SStat			ParseStat(const QByteArray& Data);

	//
	// /proc/<pid>/status - "Key:\tValue" lines. Returned verbatim; callers pick
	// the fields they need (Uid, Gid, VmRSS, Threads, ...).
	//
	QMap<QString, QString>	ReadStatus(quint64 Pid);

	//
	// /proc/<pid>/statm, in pages.
	//
	struct SStatM
	{
		quint64	Size = 0;
		quint64	Resident = 0;
		quint64	Shared = 0;
		quint64	Text = 0;
		quint64	Lib = 0;
		quint64	Data = 0;
		quint64	Dirty = 0;
		bool	Valid = false;
	};

	SStatM			ReadStatM(quint64 Pid);

	//
	// /proc/<pid>/io. Requires ptrace-level access to other users' processes,
	// so Valid stays false when unreadable.
	//
	struct SProcIo
	{
		quint64	RChar = 0;
		quint64	WChar = 0;
		quint64	SysCr = 0;
		quint64	SysCw = 0;
		quint64	ReadBytes = 0;
		quint64	WriteBytes = 0;
		quint64	CancelledWriteBytes = 0;
		bool	Valid = false;
	};

	SProcIo			ReadProcIo(quint64 Pid);

	// The same parse, for content obtained some other way - e.g. handed over by an
	// elevated TaskHelper for a process this user cannot read directly.
	SProcIo			ParseProcIo(const QByteArray& Data);

	//
	// One line of /proc/<pid>/maps.
	//
	struct SMapEntry
	{
		quint64	Start = 0;
		quint64	End = 0;
		bool	Read = false;
		bool	Write = false;
		bool	Exec = false;
		bool	Shared = false;	// 's' rather than 'p'
		quint64	Offset = 0;
		quint32	DevMajor = 0;
		quint32	DevMinor = 0;
		quint64	Inode = 0;
		QString	Path;			// may be empty, or "[heap]", "[stack]", ...
	};

	QList<SMapEntry>	ReadMaps(quint64 Pid);

	// The same, for maps text obtained some other way - an elevated TaskHelper
	// reads it on behalf of the core dumper, which cannot open it directly.
	QList<SMapEntry>	ParseMaps(const QByteArray& Data);

	//
	// Per-region residency detail, keyed by region start address.
	//
	// This comes from /proc/<pid>/smaps, which is considerably more expensive
	// than maps: the kernel walks the page tables of every region to produce
	// it. Callers should read it only when the numbers are actually going to be
	// displayed, not on every refresh.
	//
	struct SMapDetail
	{
		quint64	Rss = 0;		// resident bytes
		quint64	Pss = 0;		// resident, divided by the number of sharers
		quint64	SharedClean = 0;
		quint64	SharedDirty = 0;
		quint64	PrivateClean = 0;
		quint64	PrivateDirty = 0;
		quint64	Swap = 0;
		quint64	Locked = 0;
	};

	QMap<quint64, SMapDetail>	ReadMapDetails(quint64 Pid);

	// Whole-process totals from /proc/<pid>/smaps_rollup, which the kernel
	// aggregates itself and is therefore far cheaper than summing smaps.
	SMapDetail		ReadMapRollup(quint64 Pid);

	//
	// /proc/stat aggregate CPU line(s), in clock ticks. Index -1 is the "cpu"
	// total; 0..n-1 are the per-cpu lines.
	//
	struct SCpuTime
	{
		quint64	User = 0;
		quint64	Nice = 0;
		quint64	System = 0;
		quint64	Idle = 0;
		quint64	IoWait = 0;
		quint64	Irq = 0;
		quint64	SoftIrq = 0;
		quint64	Steal = 0;
		quint64	Guest = 0;
		quint64	GuestNice = 0;

		quint64	Total() const { return User + Nice + System + Idle + IoWait + Irq + SoftIrq + Steal; }
		quint64	Busy() const  { return Total() - Idle - IoWait; }
	};

	struct SSysStat
	{
		SCpuTime			Total;
		QVector<SCpuTime>	PerCpu;
		quint64				ContextSwitches = 0;
		quint64				BootTime = 0;
		quint64				Processes = 0;
		quint64				ProcsRunning = 0;
		quint64				ProcsBlocked = 0;
		quint64				Interrupts = 0;
		bool				Valid = false;
	};

	SSysStat		ReadSysStat();

	// /proc/meminfo, normalised to bytes (the file reports kB).
	QMap<QString, quint64>	ReadMemInfo();

	// One entry of /proc/swaps, sizes normalised to bytes.
	struct SSwapArea
	{
		QString	Path;
		QString	Type;		// "partition" or "file"
		quint64	Size = 0;
		quint64	Used = 0;
	};

	QList<SSwapArea>	ReadSwaps();

	//
	// /proc/cpuinfo. Only the fields the system-info panel needs.
	//
	// Topology is counted by distinct "physical id" values (sockets) and
	// distinct (physical id, core id) pairs (physical cores), which is how
	// hyperthreaded logical cpus collapse onto their real cores. Architectures
	// whose cpuinfo omits those fields (arm64) fall back to logical count.
	//
	struct SCpuInfo
	{
		QString	ModelName;
		double	MHz = 0;
		int	Logical = 0;	// logical cpus, i.e. "processor" lines
		int	Cores = 0;	// distinct physical cores
		int	Packages = 0;	// distinct sockets
	};

	SCpuInfo		ReadCpuInfo();

	//
	// One row of /proc/net/{tcp,tcp6,udp,udp6}.
	//
	// State is already translated to the MIB_TCP_STATE numbering the shared
	// CSocketInfo::GetStateString() expects, rather than the kernel's own
	// TCP_* values - the two do not agree (Linux TCP_ESTABLISHED is 1, the MIB
	// value is 5).
	//
	struct SNetConnection
	{
		quint32		ProtocolType = 0;	// NET_TYPE_* from SocketInfo.h
		QHostAddress	LocalAddress;
		quint16		LocalPort = 0;
		QHostAddress	RemoteAddress;
		quint16		RemotePort = 0;
		quint32		State = 0;
		quint64		Inode = 0;
		quint32		Uid = 0;
		quint64		TxQueue = 0;
		quint64		RxQueue = 0;

		//
		// Cumulative per-socket byte and segment counters.
		//
		// Only the sock_diag path can fill these in - /proc/net exposes queue
		// depths but no totals. bHaveCounters distinguishes "no traffic yet"
		// from "this source cannot tell us", so the rate columns can show blank
		// rather than a misleading zero.
		//
		quint64		BytesSent = 0;
		quint64		BytesReceived = 0;
		quint64		SegmentsSent = 0;
		quint64		SegmentsReceived = 0;
		bool		bHaveCounters = false;
	};

	QList<SNetConnection>	ReadNetConnections();

	//
	// Maps socket inode -> owning pid, by walking every /proc/<pid>/fd and
	// picking out the "socket:[n]" links. This is what ss(8) and netstat(8) do;
	// there is no reverse index in the kernel.
	//
	// Entries for processes whose fd directory is not readable are simply
	// absent, so sockets owned by other users show up unattributed when running
	// unprivileged.
	//
	QMap<quint64, quint64>	BuildSocketInodeMap();

	//
	// One row of /proc/diskstats.
	//
	// The "sectors" fields are always in 512 byte units regardless of the
	// device's real block size, so they are converted to bytes here.
	//
	struct SDiskStat
	{
		quint32	Major = 0;
		quint32	Minor = 0;
		QString	Name;

		quint64	ReadsCompleted = 0;
		quint64	BytesRead = 0;
		quint64	ReadTimeMs = 0;
		quint64	WritesCompleted = 0;
		quint64	BytesWritten = 0;
		quint64	WriteTimeMs = 0;
		quint64	IosInProgress = 0;
		quint64	IoTicksMs = 0;		// time the queue was non-empty
	};

	// Whole block devices only - partitions, loop, ram and zram devices are
	// filtered out, since the disk view wants physical drives.
	QList<SDiskStat>	ReadDiskStats();

	// Capacity of a whole block device in bytes, from /sys/block/<dev>/size.
	quint64			ReadDiskSize(const QString& Device);
	// Human readable model, from /sys/block/<dev>/device/model.
	QString			ReadDiskModel(const QString& Device);
	// Mount points backed by this device, from /proc/self/mountinfo.
	QStringList		ReadDiskMountPoints(const QString& Device);

	//
	// A network interface as described by /sys/class/net/<name>.
	//
	struct SNetDevice
	{
		QString	Name;
		QString	MacAddress;
		quint32	Index = 0;
		qint64	SpeedMbit = -1;		// -1 when the driver does not report it
		bool	Carrier = false;
		QString	OperState;		// up / down / dormant / unknown
		bool	IsLoopback = false;

		quint64	RxBytes = 0;
		quint64	RxPackets = 0;
		quint64	TxBytes = 0;
		quint64	TxPackets = 0;
	};

	QList<SNetDevice>	ReadNetDevices();

	//
	// Default gateways per interface, from /proc/net/route and
	// /proc/net/ipv6_route. Netlink (RTM_GETROUTE) would give the same answer;
	// these files are simpler and the route table is small.
	//
	QMap<QString, QList<QHostAddress> >	ReadDefaultGateways();

	//
	// Configured DNS servers and search domains.
	//
	// /etc/resolv.conf is preferred, EXCEPT when it points at the
	// systemd-resolved stub listener on 127.0.0.53 - reporting that as "the DNS
	// server" is technically true but tells the user nothing. In that case the
	// real upstream servers are read from /run/systemd/resolve/resolv.conf,
	// which resolved maintains for exactly this purpose.
	//
	struct SDnsConfig
	{
		QList<QHostAddress>	Servers;
		QSet<QString>		Domains;
	};

	SDnsConfig		ReadDnsConfig();

	//
	// /etc/os-release, the freedesktop-standard distribution description.
	//
	// Values are unquoted and unescaped. The keys of interest are PRETTY_NAME
	// (display name), ID ("ubuntu"), ID_LIKE (space separated parents, e.g.
	// "debian"), VERSION_ID ("26.04") and LOGO (an icon-theme name).
	//
	// /usr/lib/os-release is the fallback location mandated by the spec for
	// systems where /etc is not writable.
	//
	QMap<QString, QString>	ReadOsRelease();

	//
	// The systemd unit a process belongs to, from /proc/<pid>/cgroup, or an
	// empty string when it is not part of a service.
	//
	// The cgroup path is scanned from the end backwards for the first
	// ".service" component, stopping at any ".scope". Both halves matter:
	//
	//   /system.slice/systemd-udevd.service/udev
	//       the unit is not the last component, so the walk is needed
	//
	//   /user.slice/user-1000.slice/user@1000.service/app.slice/
	//       app-org.kde.konsole-1938.scope/main.scope
	//       a desktop app - walking past the .scope would wrongly attribute it
	//       to the user manager's user@1000.service and paint every GUI
	//       application as a service
	//
	struct SServiceUnit
	{
		QString	Name;			// e.g. "NetworkManager.service"

		//
		// Whether the unit lives under system.slice, i.e. is a system daemon
		// rather than something in the user's session.
		//
		// This distinction matters because systemd units are not the same
		// category as Windows services. A desktop session launches ordinary
		// applications as transient units too - KDE runs Dolphin as
		// "app-org.kde.dolphin@<id>.service" - so on a typical desktop the
		// large majority of processes belong to *some* .service. Treating all
		// of them as services would make the process tree's service colouring
		// meaningless.
		//
		bool	bSystemSlice = false;
	};

	SServiceUnit		ReadServiceUnit(quint64 Pid);

	//
	// /proc/vmstat as raw key/value pairs.
	//
	// The paging counters are the interesting ones here: pgpgin and pgpgout are
	// the traffic between the page cache and the block devices, in kilobytes -
	// verified against /proc/diskstats, whose sector counts are exactly twice
	// them. pswpin and pswpout are swap activity, in pages.
	//
	QMap<QString, quint64>	ReadVmStat();

	// ---- cgroups ----

	//
	// The unified (v2) cgroup path of a process, as it appears under
	// /sys/fs/cgroup. Empty when the process is not in a cgroup, or when only
	// the v1 hierarchies exist.
	//
	QString			ReadCGroupPath(quint64 Pid);

	//
	// Pressure Stall Information: what share of a window some or all runnable
	// tasks spent stalled waiting for a resource.
	//
	// This has no Windows counterpart. "some" is the share of time at least one
	// task was stalled - the useful latency signal; "full" is the share where
	// *every* task was stalled, i.e. outright lost throughput. The averages are
	// percentages over the trailing 10, 60 and 300 seconds.
	//
	struct SPressure
	{
		float	SomeAvg10 = 0, SomeAvg60 = 0, SomeAvg300 = 0;
		float	FullAvg10 = 0, FullAvg60 = 0, FullAvg300 = 0;
		quint64	SomeTotal = 0, FullTotal = 0;	// cumulative stall, microseconds
		bool	Valid = false;
	};

	SPressure		ParsePressure(const QByteArray& Data);

	// Resource is "cpu", "memory" or "io". System wide, from /proc/pressure.
	SPressure		ReadSysPressure(const QString& Resource);

	// The same, for one cgroup. Requires the kernel to have PSI enabled per
	// cgroup, which is the default on modern kernels.
	SPressure		ReadCGroupPressure(const QString& CGroupPath, const QString& Resource);

	//
	// Accounting and limits for one cgroup.
	//
	// This is the Linux equivalent of a Windows job object: a set of processes
	// with shared resource limits and shared accounting. Limits read as 0 when
	// the file says "max", i.e. unlimited.
	//
	struct SCGroupStats
	{
		bool		Valid = false;

		quint64		MemoryCurrent = 0;
		quint64		MemoryPeak = 0;
		quint64		MemoryMax = 0;			// 0 = unlimited
		quint64		MemoryHigh = 0;			// throttling threshold, 0 = unset
		quint64		MemorySwapCurrent = 0;
		quint64		MemorySwapMax = 0;

		// cpu.stat, microseconds
		quint64		CpuUsageUs = 0;
		quint64		CpuUserUs = 0;
		quint64		CpuSystemUs = 0;

		// Throttling, present only when a cpu.max limit is set.
		quint64		NrPeriods = 0;
		quint64		NrThrottled = 0;
		quint64		ThrottledUs = 0;

		quint64		PidsCurrent = 0;
		quint64		PidsMax = 0;			// 0 = unlimited

		// io.stat, summed over all block devices.
		quint64		IoReadBytes = 0;
		quint64		IoWriteBytes = 0;

		QStringList	Controllers;			// cgroup.controllers
	};

	SCGroupStats	ReadCGroupStats(const QString& CGroupPath);

	// ---- namespaces ----

	//
	// The inode number of each of a process's namespaces, or 0 where the link
	// could not be read (which needs ptrace access for another user's process).
	//
	// Comparing these against pid 1's is how a container or sandbox is
	// detected: anything differing means the process is isolated from the host
	// in that dimension.
	//
	struct SNamespaces
	{
		quint64	Pid = 0, Net = 0, Mnt = 0, User = 0;
		quint64	Uts = 0, Ipc = 0, CGroup = 0, Time = 0;
	};

	SNamespaces		ReadNamespaces(quint64 Pid);

	// ---- security ----

	//
	// What a process is permitted to do: capabilities, LSM confinement and
	// seccomp state. Together these are the Linux answer to the Windows token.
	//
	struct SProcSecurity
	{
		bool	Valid = false;

		// Capability sets, as bit masks. Inheritable, permitted, effective,
		// bounding and ambient respectively.
		quint64	CapInh = 0, CapPrm = 0, CapEff = 0, CapBnd = 0, CapAmb = 0;

		// The LSM label from /proc/<pid>/attr/current: an AppArmor profile
		// ("snap.firefox.firefox (enforce)") or an SELinux context. Empty when
		// no LSM is active; "unconfined" when one is but this process is not
		// confined by it.
		QString	Confinement;

		int		Seccomp = 0;			// 0 disabled, 1 strict, 2 filter
		quint64	SeccompFilters = 0;
		bool	NoNewPrivs = false;
	};

	SProcSecurity	ReadProcSecurity(quint64 Pid);

	// Capability bit mask -> the CAP_* names it contains.
	QStringList		DecodeCapabilities(quint64 Mask);
	QString			SeccompModeToString(int Mode);

	// ---- out of memory killer ----

	struct SOomInfo
	{
		// The badness the kernel currently assigns, 0..1000. Higher dies first.
		int		Score = 0;
		// The user's adjustment, -1000..1000. -1000 exempts the process.
		int		ScoreAdj = 0;
		bool	Valid = false;
	};

	SOomInfo		ReadOomInfo(quint64 Pid);

	//
	// Adjusts a process's OOM badness. Raising it is unprivileged; lowering it
	// below the current value needs CAP_SYS_RESOURCE, so this fails for
	// ordinary users trying to protect a process.
	//
	bool			WriteOomScoreAdj(quint64 Pid, int Value);

	// ---- inotify ----

	//
	// How many inotify watches a process holds, summed over its inotify file
	// descriptors.
	//
	// Worth surfacing because the per-user limit (fs.inotify.max_user_watches)
	// is a well known thing to exhaust, and when it happens the failure appears
	// somewhere else entirely - an editor that stops noticing file changes, a
	// sync client that silently stalls.
	//
	// This reads every fdinfo file of the process, so it is markedly more
	// expensive than the other per-process readers and should not be called on
	// every refresh for every process.
	//
	quint64			CountInotifyWatches(quint64 Pid);

	// ---- units ----

	// sysconf(_SC_CLK_TCK) - the divisor for the clock-tick fields above.
	quint64			ClockTicksPerSec();
	// sysconf(_SC_PAGESIZE)
	quint64			PageSize();
	// Seconds since boot, from /proc/uptime.
	double			UpTime();
	// CLOCK_BOOTTIME-based conversion of a stat StartTime into a unix epoch
	// timestamp in milliseconds.
	quint64			StartTimeToEpochMs(quint64 StartTimeTicks);

	// ---- identity ----

	QString			UserNameFromUid(quint32 Uid);
	QString			GroupNameFromGid(quint32 Gid);
}
