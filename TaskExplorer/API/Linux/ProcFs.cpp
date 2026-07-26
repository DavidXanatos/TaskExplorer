#include "stdafx.h"
#include "ProcFs.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDirIterator>
#include <QHostAddress>
#include <QSet>
#include <QSysInfo>
#include <QtEndian>

#include <fcntl.h>
#include <errno.h>
#include <grp.h>
#include <pwd.h>
#include <unistd.h>

namespace ProcFs
{

QByteArray ReadFile(const QString& Path)
{
	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly))
		return QByteArray();

	//
	// Files under /proc report a size of 0, so readAll() is the only correct
	// way to drain them - QFile::size() would yield an empty result.
	//
	return File.readAll();
}

QString ReadFileStr(const QString& Path)
{
	return QString::fromUtf8(ReadFile(Path));
}

QStringList ReadNulList(const QString& Path)
{
	QByteArray Data = ReadFile(Path);
	if (Data.isEmpty())
		return QStringList();

	// These files are NUL terminated, which would otherwise yield a trailing
	// empty entry on every read.
	while (Data.endsWith('\0'))
		Data.chop(1);
	if (Data.isEmpty())
		return QStringList();

	QStringList List;
	for (const QByteArray& Part : Data.split('\0'))
		List.append(QString::fromUtf8(Part));
	return List;
}

QString ReadLink(const QString& Path)
{
	//
	// Must be readlink(2) rather than QFile::symLinkTarget().
	//
	// The /proc/<pid>/fd links do not always point at a path: sockets, pipes
	// and anonymous inodes resolve to "socket:[12345]", "pipe:[12345]" and
	// "anon_inode:[eventfd]". Those are not absolute, so symLinkTarget()
	// helpfully rebases them against the link's own directory and hands back
	// "/proc/<pid>/fd/socket:[12345]" - which hides both the object kind and
	// the inode number that socket ownership is later resolved by.
	//
	const QByteArray Local = Path.toLocal8Bit();

	// /proc links report st_size 0, so the buffer has to be grown blindly.
	QByteArray Buffer(1024, '\0');
	for (;;)
	{
		const ssize_t Length = readlink(Local.constData(), Buffer.data(), Buffer.size());
		if (Length < 0)
			return QString(); // gone, or not permitted

		if (Length < Buffer.size())
			return QString::fromLocal8Bit(Buffer.constData(), (int)Length);

		// Truncated - readlink does not NUL terminate or report the full size.
		if (Buffer.size() > 64 * 1024)
			return QString::fromLocal8Bit(Buffer.constData(), (int)Length);
		Buffer.resize(Buffer.size() * 2);
	}
}

bool FileExists(const QString& Path)
{
	return QFileInfo::exists(Path);
}

QString ProcPath(quint64 Pid, const QString& Leaf)
{
	QString Path = QString("/proc/%1").arg(Pid);
	if (!Leaf.isEmpty())
		Path += "/" + Leaf;
	return Path;
}

QString TaskPath(quint64 Pid, quint64 Tid, const QString& Leaf)
{
	QString Path = QString("/proc/%1/task/%2").arg(Pid).arg(Tid);
	if (!Leaf.isEmpty())
		Path += "/" + Leaf;
	return Path;
}

static QList<quint64> EnumNumericEntries(const QString& DirPath)
{
	QList<quint64> Entries;

	QDir Dir(DirPath);
	if (!Dir.exists())
		return Entries;

	const QStringList Names = Dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
	for (const QString& Name : Names)
	{
		bool bOk = false;
		quint64 Value = Name.toULongLong(&bOk);
		if (bOk)
			Entries.append(Value);
	}
	return Entries;
}

QList<quint64> EnumProcesses()
{
	return EnumNumericEntries("/proc");
}

QList<quint64> EnumThreads(quint64 Pid)
{
	return EnumNumericEntries(ProcPath(Pid, "task"));
}

QList<quint64> EnumFds(quint64 Pid)
{
	QList<quint64> Fds;

	QDir Dir(ProcPath(Pid, "fd"));
	// The fd entries are symlinks, not directories, so this needs a different
	// filter than EnumNumericEntries.
	const QStringList Names = Dir.entryList(QDir::Files | QDir::System | QDir::NoDotAndDotDot);
	for (const QString& Name : Names)
	{
		bool bOk = false;
		quint64 Value = Name.toULongLong(&bOk);
		if (bOk)
			Fds.append(Value);
	}
	return Fds;
}

SStat ParseStat(const QByteArray& Data)
{
	SStat Stat;
	if (Data.isEmpty())
		return Stat;

	//
	// Field 2 (comm) is wrapped in parentheses and may contain both spaces and
	// parentheses of its own - e.g. a process named ") evil (". Anchoring on
	// the LAST ')' is the only reliable split.
	//
	int Open = Data.indexOf('(');
	int Close = Data.lastIndexOf(')');
	if (Open < 0 || Close < 0 || Close < Open)
		return Stat;

	bool bOk = false;
	Stat.Pid = Data.left(Open).trimmed().toULongLong(&bOk);
	if (!bOk)
		return Stat;

	Stat.Comm = QString::fromUtf8(Data.mid(Open + 1, Close - Open - 1));

	const QList<QByteArray> F = Data.mid(Close + 1).simplified().split(' ');
	// F[0] is state, i.e. the original field 3; index i here == field (i + 3).
	auto U64 = [&F](int i) -> quint64 { return i < F.size() ? F[i].toULongLong() : 0; };
	auto I64 = [&F](int i) -> qint64  { return i < F.size() ? F[i].toLongLong()  : 0; };

	if (F.isEmpty() || F[0].isEmpty())
		return Stat;

	Stat.State		= F[0].at(0);
	Stat.PPid		= U64(1);
	Stat.PGrp		= U64(2);
	Stat.Session		= U64(3);
	Stat.TtyNr		= U64(4);
	Stat.TPGid		= U64(5);
	Stat.Flags		= U64(6);
	Stat.MinFlt		= U64(7);
	Stat.CMinFlt		= U64(8);
	Stat.MajFlt		= U64(9);
	Stat.CMajFlt		= U64(10);
	Stat.UTime		= U64(11);
	Stat.STime		= U64(12);
	Stat.CUTime		= I64(13);
	Stat.CSTime		= I64(14);
	Stat.Priority		= I64(15);
	Stat.Nice		= I64(16);
	Stat.NumThreads		= I64(17);
	// F[18] is itrealvalue, obsolete and always 0.
	Stat.StartTime		= U64(19);
	Stat.VSize		= U64(20);
	Stat.Rss		= I64(21);
	Stat.RssLim		= U64(22);
	Stat.StartCode		= U64(23);
	Stat.EndCode		= U64(24);
	Stat.StartStack		= U64(25);
	Stat.KstkEsp		= U64(26);
	Stat.KstkEip		= U64(27);
	// F[28..31] are the obsolete signal bitmaps.
	// F[32] is wchan, F[33..34] are obsolete.
	Stat.Processor		= U64(36);
	Stat.RtPriority		= U64(37);
	Stat.Policy		= U64(38);
	Stat.DelayAcctBlkIOTicks = U64(39);
	Stat.GuestTime		= U64(40);

	// PF_KTHREAD, from include/linux/sched.h. Defined locally because it is a
	// kernel-internal constant with no uapi header.
	static const quint64 PF_KTHREAD = 0x00200000;
	Stat.IsKernelThread = (Stat.Flags & PF_KTHREAD) != 0;

	Stat.Valid = true;
	return Stat;
}

SStat ReadStat(quint64 Pid)
{
	return ParseStat(ReadFile(ProcPath(Pid, "stat")));
}

SStat ReadThreadStat(quint64 Pid, quint64 Tid)
{
	return ParseStat(ReadFile(TaskPath(Pid, Tid, "stat")));
}

QMap<QString, QString> ReadStatus(quint64 Pid)
{
	QMap<QString, QString> Status;

	const QByteArray Data = ReadFile(ProcPath(Pid, "status"));
	for (const QByteArray& Line : Data.split('\n'))
	{
		int Sep = Line.indexOf(':');
		if (Sep < 0)
			continue;
		Status.insert(QString::fromUtf8(Line.left(Sep)),
		              QString::fromUtf8(Line.mid(Sep + 1).trimmed()));
	}
	return Status;
}

SStatM ReadStatM(quint64 Pid)
{
	SStatM StatM;

	const QList<QByteArray> F = ReadFile(ProcPath(Pid, "statm")).simplified().split(' ');
	if (F.size() < 7)
		return StatM;

	StatM.Size	= F[0].toULongLong();
	StatM.Resident	= F[1].toULongLong();
	StatM.Shared	= F[2].toULongLong();
	StatM.Text	= F[3].toULongLong();
	StatM.Lib	= F[4].toULongLong();
	StatM.Data	= F[5].toULongLong();
	StatM.Dirty	= F[6].toULongLong();
	StatM.Valid	= true;
	return StatM;
}

SProcIo ParseProcIo(const QByteArray& Data)
{
	SProcIo Io;
	if (Data.isEmpty())
		return Io;

	for (const QByteArray& Line : Data.split('\n'))
	{
		int Sep = Line.indexOf(':');
		if (Sep < 0)
			continue;
		const QByteArray Key = Line.left(Sep);
		const quint64 Value = Line.mid(Sep + 1).trimmed().toULongLong();

		if      (Key == "rchar")                 Io.RChar = Value;
		else if (Key == "wchar")                 Io.WChar = Value;
		else if (Key == "syscr")                 Io.SysCr = Value;
		else if (Key == "syscw")                 Io.SysCw = Value;
		else if (Key == "read_bytes")            Io.ReadBytes = Value;
		else if (Key == "write_bytes")           Io.WriteBytes = Value;
		else if (Key == "cancelled_write_bytes") Io.CancelledWriteBytes = Value;
	}

	Io.Valid = true;
	return Io;
}

SProcIo ReadProcIo(quint64 Pid)
{
	//
	// Reading another user's /proc/<pid>/io needs PTRACE_MODE_READ, so this is
	// expected to fail for most processes when running unprivileged. Leaving
	// Valid false lets callers show "n/a" rather than a bogus 0 - and lets
	// CLinuxAPI collect the pid for a batched request to TaskHelper.
	//
	return ParseProcIo(ReadFile(ProcPath(Pid, "io")));
}

QList<SMapEntry> ReadMaps(quint64 Pid)
{
	return ParseMaps(ReadFile(ProcPath(Pid, "maps")));
}

QList<SMapEntry> ParseMaps(const QByteArray& Data)
{
	QList<SMapEntry> Maps;

	for (const QByteArray& Line : Data.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		// address           perms offset   dev   inode   pathname
		// 7f1c2a000000-7f1c2a021000 r-xp 00000000 08:01 1234  /usr/lib/libc.so
		const QList<QByteArray> F = Line.simplified().split(' ');
		if (F.size() < 5)
			continue;

		const int Dash = F[0].indexOf('-');
		if (Dash < 0)
			continue;

		SMapEntry Entry;
		Entry.Start	= F[0].left(Dash).toULongLong(nullptr, 16);
		Entry.End	= F[0].mid(Dash + 1).toULongLong(nullptr, 16);

		if (F[1].size() >= 4)
		{
			Entry.Read	= F[1][0] == 'r';
			Entry.Write	= F[1][1] == 'w';
			Entry.Exec	= F[1][2] == 'x';
			Entry.Shared	= F[1][3] == 's';
		}

		Entry.Offset = F[2].toULongLong(nullptr, 16);

		const int Colon = F[3].indexOf(':');
		if (Colon >= 0)
		{
			Entry.DevMajor = F[3].left(Colon).toUInt(nullptr, 16);
			Entry.DevMinor = F[3].mid(Colon + 1).toUInt(nullptr, 16);
		}

		Entry.Inode = F[4].toULongLong();

		// The path may legitimately contain spaces, so rejoin the remainder
		// instead of taking F[5] alone.
		if (F.size() > 5)
		{
			QByteArray Path;
			for (int i = 5; i < F.size(); i++)
			{
				if (i > 5)
					Path += ' ';
				Path += F[i];
			}
			Entry.Path = QString::fromUtf8(Path);
		}

		Maps.append(Entry);
	}

	return Maps;
}

//
// Applies one "Key:  <n> kB" line from smaps to a detail record. Returns false
// if the line is not one of the fields we care about.
//
static bool ApplyMapDetailLine(const QByteArray& Key, quint64 ValueBytes, SMapDetail& Detail)
{
	if      (Key == "Rss")            Detail.Rss = ValueBytes;
	else if (Key == "Pss")            Detail.Pss = ValueBytes;
	else if (Key == "Shared_Clean")   Detail.SharedClean = ValueBytes;
	else if (Key == "Shared_Dirty")   Detail.SharedDirty = ValueBytes;
	else if (Key == "Private_Clean")  Detail.PrivateClean = ValueBytes;
	else if (Key == "Private_Dirty")  Detail.PrivateDirty = ValueBytes;
	else if (Key == "Swap")           Detail.Swap = ValueBytes;
	else if (Key == "Locked")         Detail.Locked = ValueBytes;
	else return false;
	return true;
}

QMap<quint64, SMapDetail> ReadMapDetails(quint64 Pid)
{
	QMap<quint64, SMapDetail> Details;

	//
	// smaps repeats the maps header line for each region, followed by that
	// region's counters. A header is recognised by starting with a hex address
	// range rather than "Key:".
	//
	const QByteArray Data = ReadFile(ProcPath(Pid, "smaps"));

	quint64 CurrentStart = 0;
	bool bHaveRegion = false;
	SMapDetail Current;

	for (const QByteArray& Line : Data.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		const int Colon = Line.indexOf(':');
		const int Dash = Line.indexOf('-');

		// Header lines have their '-' before any ':' (the address range).
		const bool bHeader = (Dash > 0) && (Colon < 0 || Dash < Colon);

		if (bHeader)
		{
			if (bHaveRegion)
				Details.insert(CurrentStart, Current);

			Current = SMapDetail();
			CurrentStart = Line.left(Dash).toULongLong(nullptr, 16);
			bHaveRegion = true;
			continue;
		}

		if (!bHaveRegion || Colon < 0)
			continue;

		const QByteArray Key = Line.left(Colon).trimmed();
		// Values are "<n> kB"; a few (VmFlags, THPeligible) are not numeric and
		// are simply not matched by ApplyMapDetailLine.
		const QList<QByteArray> F = Line.mid(Colon + 1).simplified().split(' ');
		if (F.isEmpty())
			continue;

		quint64 Value = F[0].toULongLong();
		if (F.size() > 1 && F[1] == "kB")
			Value *= 1024;

		ApplyMapDetailLine(Key, Value, Current);
	}

	if (bHaveRegion)
		Details.insert(CurrentStart, Current);

	return Details;
}

SMapDetail ReadMapRollup(quint64 Pid)
{
	SMapDetail Detail;

	// smaps_rollup has the same key/value lines as smaps but only one set,
	// already summed by the kernel. Present since Linux 4.14.
	const QByteArray Data = ReadFile(ProcPath(Pid, "smaps_rollup"));
	for (const QByteArray& Line : Data.split('\n'))
	{
		const int Colon = Line.indexOf(':');
		if (Colon < 0)
			continue;

		const QByteArray Key = Line.left(Colon).trimmed();
		const QList<QByteArray> F = Line.mid(Colon + 1).simplified().split(' ');
		if (F.isEmpty())
			continue;

		quint64 Value = F[0].toULongLong();
		if (F.size() > 1 && F[1] == "kB")
			Value *= 1024;

		ApplyMapDetailLine(Key, Value, Detail);
	}

	return Detail;
}

static SCpuTime ParseCpuLine(const QList<QByteArray>& F)
{
	SCpuTime Time;
	auto U64 = [&F](int i) -> quint64 { return i < F.size() ? F[i].toULongLong() : 0; };

	Time.User	= U64(1);
	Time.Nice	= U64(2);
	Time.System	= U64(3);
	Time.Idle	= U64(4);
	Time.IoWait	= U64(5);
	Time.Irq	= U64(6);
	Time.SoftIrq	= U64(7);
	Time.Steal	= U64(8);
	Time.Guest	= U64(9);
	Time.GuestNice	= U64(10);
	return Time;
}

SSysStat ReadSysStat()
{
	SSysStat Stat;

	const QByteArray Data = ReadFile("/proc/stat");
	if (Data.isEmpty())
		return Stat;

	for (const QByteArray& Line : Data.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		const QList<QByteArray> F = Line.simplified().split(' ');
		const QByteArray& Key = F[0];

		if (Key == "cpu")
		{
			Stat.Total = ParseCpuLine(F);
		}
		else if (Key.startsWith("cpu"))
		{
			bool bOk = false;
			const int Index = Key.mid(3).toInt(&bOk);
			if (bOk)
			{
				if (Stat.PerCpu.size() <= Index)
					Stat.PerCpu.resize(Index + 1);
				Stat.PerCpu[Index] = ParseCpuLine(F);
			}
		}
		else if (Key == "ctxt" && F.size() > 1)		Stat.ContextSwitches = F[1].toULongLong();
		else if (Key == "btime" && F.size() > 1)	Stat.BootTime = F[1].toULongLong();
		else if (Key == "processes" && F.size() > 1)	Stat.Processes = F[1].toULongLong();
		else if (Key == "procs_running" && F.size() > 1) Stat.ProcsRunning = F[1].toULongLong();
		else if (Key == "procs_blocked" && F.size() > 1) Stat.ProcsBlocked = F[1].toULongLong();
		else if (Key == "intr" && F.size() > 1)		Stat.Interrupts = F[1].toULongLong();
	}

	Stat.Valid = true;
	return Stat;
}

QMap<QString, quint64> ReadMemInfo()
{
	QMap<QString, quint64> Info;

	const QByteArray Data = ReadFile("/proc/meminfo");
	for (const QByteArray& Line : Data.split('\n'))
	{
		int Sep = Line.indexOf(':');
		if (Sep < 0)
			continue;

		const QString Key = QString::fromUtf8(Line.left(Sep));
		const QByteArray Rest = Line.mid(Sep + 1).trimmed();

		// Almost every entry is "<number> kB"; a few (HugePages_*) are a bare
		// count. Normalise the former to bytes and leave the latter alone.
		const QList<QByteArray> F = Rest.simplified().split(' ');
		if (F.isEmpty())
			continue;

		quint64 Value = F[0].toULongLong();
		if (F.size() > 1 && F[1] == "kB")
			Value *= 1024;

		Info.insert(Key, Value);
	}
	return Info;
}

QList<SSwapArea> ReadSwaps()
{
	QList<SSwapArea> Areas;

	const QByteArray Data = ReadFile("/proc/swaps");
	const QList<QByteArray> Lines = Data.split('\n');

	// First line is the header: "Filename Type Size Used Priority"
	for (int i = 1; i < Lines.size(); i++)
	{
		if (Lines[i].isEmpty())
			continue;

		const QList<QByteArray> F = Lines[i].simplified().split(' ');
		if (F.size() < 4)
			continue;

		SSwapArea Area;
		Area.Path = QString::fromUtf8(F[0]);
		Area.Type = QString::fromUtf8(F[1]);
		// Size and Used are in 1 KiB units regardless of page size.
		Area.Size = F[2].toULongLong() * 1024;
		Area.Used = F[3].toULongLong() * 1024;
		Areas.append(Area);
	}

	return Areas;
}

SCpuInfo ReadCpuInfo()
{
	SCpuInfo Info;

	QSet<QString> Packages;
	QSet<QString> Cores;
	QString CurPhysicalId;

	const QByteArray Data = ReadFile("/proc/cpuinfo");
	for (const QByteArray& Line : Data.split('\n'))
	{
		const int Sep = Line.indexOf(':');
		if (Sep < 0)
			continue;

		const QByteArray Key = Line.left(Sep).trimmed();
		const QByteArray Value = Line.mid(Sep + 1).trimmed();

		if (Key == "processor")
		{
			Info.Logical++;
			// A new block starts here; forget the previous block's socket.
			CurPhysicalId.clear();
		}
		else if (Key == "model name" && Info.ModelName.isEmpty())
		{
			Info.ModelName = QString::fromUtf8(Value);
		}
		else if (Key == "cpu MHz" && Info.MHz == 0)
		{
			Info.MHz = Value.toDouble();
		}
		else if (Key == "physical id")
		{
			CurPhysicalId = QString::fromUtf8(Value);
			Packages.insert(CurPhysicalId);
		}
		else if (Key == "core id")
		{
			// Core ids repeat across sockets, so they only identify a physical
			// core when paired with the socket they belong to.
			Cores.insert(CurPhysicalId + ":" + QString::fromUtf8(Value));
		}
	}

	if (Info.Logical == 0)
	{
		long Online = sysconf(_SC_NPROCESSORS_ONLN);
		Info.Logical = (Online > 0) ? (int)Online : 1;
	}

	// arm64 and some virtualised x86 cpuinfo layouts omit physical/core id.
	Info.Packages = Packages.isEmpty() ? 1 : Packages.size();
	Info.Cores = Cores.isEmpty() ? Info.Logical : Cores.size();

	if (Info.ModelName.isEmpty())
	{
		// arm64 reports "CPU implementer"/"CPU part" instead of a model name.
		Info.ModelName = QString::fromUtf8(QSysInfo::currentCpuArchitecture().toUtf8());
	}

	return Info;
}

//
// The kernel prints IPv4 addresses as one little-endian 32 bit hex word, and
// IPv6 as four of them. Both need byte swapping on a little-endian host, which
// is what qFromLittleEndian on the parsed words achieves.
//
static QHostAddress ParseNetAddress(const QByteArray& Hex, bool bIPv6)
{
	if (!bIPv6)
	{
		if (Hex.size() < 8)
			return QHostAddress();
		bool bOk = false;
		const quint32 Raw = Hex.left(8).toUInt(&bOk, 16);
		if (!bOk)
			return QHostAddress();
		// Raw is host-order of a little-endian word: swap to get the address.
		return QHostAddress(qFromBigEndian<quint32>(qToLittleEndian<quint32>(Raw)));
	}

	if (Hex.size() < 32)
		return QHostAddress();

	quint8 Bytes[16];
	for (int Word = 0; Word < 4; Word++)
	{
		bool bOk = false;
		const quint32 Raw = Hex.mid(Word * 8, 8).toUInt(&bOk, 16);
		if (!bOk)
			return QHostAddress();
		const quint32 Swapped = qFromBigEndian<quint32>(qToLittleEndian<quint32>(Raw));
		Bytes[Word * 4 + 0] = (Swapped >> 24) & 0xFF;
		Bytes[Word * 4 + 1] = (Swapped >> 16) & 0xFF;
		Bytes[Word * 4 + 2] = (Swapped >> 8) & 0xFF;
		Bytes[Word * 4 + 3] = (Swapped) & 0xFF;
	}
	return QHostAddress(Bytes);
}

//
// Kernel TCP_* state -> MIB_TCP_STATE, which is what the shared socket layer
// renders. The numbering differs, so this cannot be a pass-through.
//
static quint32 TcpStateToMib(quint32 LinuxState)
{
	switch (LinuxState)
	{
		case 1:  return 5;	// TCP_ESTABLISHED -> MIB_TCP_STATE_ESTAB
		case 2:  return 3;	// TCP_SYN_SENT    -> MIB_TCP_STATE_SYN_SENT
		case 3:  return 4;	// TCP_SYN_RECV    -> MIB_TCP_STATE_SYN_RCVD
		case 4:  return 6;	// TCP_FIN_WAIT1   -> MIB_TCP_STATE_FIN_WAIT1
		case 5:  return 7;	// TCP_FIN_WAIT2   -> MIB_TCP_STATE_FIN_WAIT2
		case 6:  return 11;	// TCP_TIME_WAIT   -> MIB_TCP_STATE_TIME_WAIT
		case 7:  return 1;	// TCP_CLOSE       -> MIB_TCP_STATE_CLOSED
		case 8:  return 8;	// TCP_CLOSE_WAIT  -> MIB_TCP_STATE_CLOSE_WAIT
		case 9:  return 10;	// TCP_LAST_ACK    -> MIB_TCP_STATE_LAST_ACK
		case 10: return 2;	// TCP_LISTEN      -> MIB_TCP_STATE_LISTEN
		case 11: return 9;	// TCP_CLOSING     -> MIB_TCP_STATE_CLOSING
	}
	return 0;
}

static void ReadNetTable(const QString& Path, quint32 ProtocolType, bool bIPv6, bool bTcp,
                         QList<SNetConnection>& Connections)
{
	const QByteArray Data = ReadFile(Path);
	const QList<QByteArray> Lines = Data.split('\n');

	// Line 0 is the column header.
	for (int i = 1; i < Lines.size(); i++)
	{
		if (Lines[i].isEmpty())
			continue;

		//  sl local_address rem_address st tx_queue:rx_queue tr tm->when retrnsmt uid timeout inode
		const QList<QByteArray> F = Lines[i].simplified().split(' ');
		if (F.size() < 10)
			continue;

		const QList<QByteArray> Local = F[1].split(':');
		const QList<QByteArray> Remote = F[2].split(':');
		if (Local.size() < 2 || Remote.size() < 2)
			continue;

		SNetConnection Conn;
		Conn.ProtocolType = ProtocolType;
		Conn.LocalAddress = ParseNetAddress(Local[0], bIPv6);
		Conn.LocalPort = Local[1].toUShort(nullptr, 16);
		Conn.RemoteAddress = ParseNetAddress(Remote[0], bIPv6);
		Conn.RemotePort = Remote[1].toUShort(nullptr, 16);

		const quint32 RawState = F[3].toUInt(nullptr, 16);
		//
		// The state column only means anything for TCP. UDP reuses it for its
		// own pseudo states, where 7 (TCP_CLOSE) is an ordinary unconnected
		// socket - reporting that as "Closed" would be misleading, so UDP is
		// pinned to ESTAB, which the shared code renders as "Open".
		//
		Conn.State = bTcp ? TcpStateToMib(RawState) : 5;

		const QList<QByteArray> Queues = F[4].split(':');
		if (Queues.size() >= 2)
		{
			Conn.TxQueue = Queues[0].toULongLong(nullptr, 16);
			Conn.RxQueue = Queues[1].toULongLong(nullptr, 16);
		}

		Conn.Uid = F[7].toUInt();
		Conn.Inode = F[9].toULongLong();

		Connections.append(Conn);
	}
}

QList<SNetConnection> ReadNetConnections()
{
	QList<SNetConnection> Connections;

	// NET_TYPE_* values, mirrored from SocketInfo.h to keep ProcFs free of
	// dependencies on the API layer.
	const quint32 NET_IPV4 = 0x1, NET_IPV6 = 0x2, NET_TCP = 0x10, NET_UDP = 0x20;

	ReadNetTable("/proc/net/tcp",  NET_IPV4 | NET_TCP, false, true,  Connections);
	ReadNetTable("/proc/net/tcp6", NET_IPV6 | NET_TCP, true,  true,  Connections);
	ReadNetTable("/proc/net/udp",  NET_IPV4 | NET_UDP, false, false, Connections);
	ReadNetTable("/proc/net/udp6", NET_IPV6 | NET_UDP, true,  false, Connections);

	return Connections;
}

QMap<quint64, quint64> BuildSocketInodeMap()
{
	QMap<quint64, quint64> InodeToPid;

	for (quint64 Pid : EnumProcesses())
	{
		const QString FdDir = ProcPath(Pid, "fd");

		//
		// Iterating with QDirIterator rather than EnumFds + ReadLink avoids
		// building an intermediate list per process; this loop runs over every
		// process on every socket refresh, so it is the hot path here.
		//
		QDirIterator It(FdDir, QDir::Files | QDir::System | QDir::NoDotAndDotDot);
		while (It.hasNext())
		{
			const QString Target = ReadLink(It.next());
			if (!Target.startsWith("socket:["))
				continue;

			const int Close = Target.lastIndexOf(']');
			if (Close < 8)
				continue;

			const quint64 Inode = Target.mid(8, Close - 8).toULongLong();
			if (!Inode)
				continue;

			// First writer wins. A socket shared across a fork appears under
			// several pids; the lowest enumerated one is as good a choice as
			// any and matches what ss reports first.
			if (!InodeToPid.contains(Inode))
				InodeToPid.insert(Inode, Pid);
		}
	}

	return InodeToPid;
}

QList<SDiskStat> ReadDiskStats()
{
	QList<SDiskStat> Stats;

	//
	// /proc/diskstats lists partitions alongside whole devices. Only names that
	// exist as a directory under /sys/block are whole devices, which is the
	// cheapest reliable way to tell them apart (sda is there, sda1 is not).
	//
	const QSet<QString> WholeDevices = [] {
		QSet<QString> Set;
		for (const QString& Name : QDir("/sys/block").entryList(QDir::Dirs | QDir::NoDotAndDotDot))
			Set.insert(Name);
		return Set;
	}();

	const QByteArray Data = ReadFile("/proc/diskstats");
	for (const QByteArray& Line : Data.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		const QList<QByteArray> F = Line.simplified().split(' ');
		if (F.size() < 14)
			continue;

		const QString Name = QString::fromUtf8(F[2]);
		if (!WholeDevices.contains(Name))
			continue;

		// Virtual devices that would only clutter the disk view.
		if (Name.startsWith("loop") || Name.startsWith("ram") || Name.startsWith("zram"))
			continue;

		SDiskStat Stat;
		Stat.Major		= F[0].toUInt();
		Stat.Minor		= F[1].toUInt();
		Stat.Name		= Name;
		Stat.ReadsCompleted	= F[3].toULongLong();
		Stat.BytesRead		= F[5].toULongLong() * 512;
		Stat.ReadTimeMs		= F[6].toULongLong();
		Stat.WritesCompleted	= F[7].toULongLong();
		Stat.BytesWritten	= F[9].toULongLong() * 512;
		Stat.WriteTimeMs	= F[10].toULongLong();
		Stat.IosInProgress	= F[11].toULongLong();
		Stat.IoTicksMs		= F[12].toULongLong();

		Stats.append(Stat);
	}

	return Stats;
}

quint64 ReadDiskSize(const QString& Device)
{
	// /sys/block/<dev>/size is in 512 byte sectors, always, regardless of the
	// device's logical block size.
	const QByteArray Data = ReadFile(QString("/sys/block/%1/size").arg(Device));
	return Data.trimmed().toULongLong() * 512;
}

QString ReadDiskModel(const QString& Device)
{
	QString Model = ReadFileStr(QString("/sys/block/%1/device/model").arg(Device)).trimmed();
	if (Model.isEmpty())
	{
		// Virtual and NVMe devices often expose a model elsewhere or not at all.
		Model = ReadFileStr(QString("/sys/block/%1/device/name").arg(Device)).trimmed();
	}
	return Model;
}

QStringList ReadDiskMountPoints(const QString& Device)
{
	QStringList Mounts;

	//
	// mountinfo field 3 is "major:minor" of the source device and field 5 is
	// the mount point. Matching on the major of the whole device catches all of
	// its partitions, which is what "mount points of this disk" should mean.
	//
	const quint32 Major = ReadFileStr(QString("/sys/block/%1/dev").arg(Device)).section(':', 0, 0).toUInt();
	if (!Major)
		return Mounts;

	const QByteArray Data = ReadFile("/proc/self/mountinfo");
	for (const QByteArray& Line : Data.split('\n'))
	{
		const QList<QByteArray> F = Line.simplified().split(' ');
		if (F.size() < 5)
			continue;

		if (F[2].split(':').value(0).toUInt() != Major)
			continue;

		const QString Mount = QString::fromUtf8(F[4]);
		if (!Mounts.contains(Mount))
			Mounts.append(Mount);
	}

	return Mounts;
}

QList<SNetDevice> ReadNetDevices()
{
	QList<SNetDevice> Devices;

	const QStringList Names = QDir("/sys/class/net").entryList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::System);
	for (const QString& Name : Names)
	{
		const QString Base = "/sys/class/net/" + Name;

		SNetDevice Device;
		Device.Name = Name;
		Device.MacAddress = ReadFileStr(Base + "/address").trimmed();
		Device.Index = ReadFileStr(Base + "/ifindex").trimmed().toUInt();
		Device.OperState = ReadFileStr(Base + "/operstate").trimmed();

		// ARPHRD_LOOPBACK is 772.
		Device.IsLoopback = (ReadFileStr(Base + "/type").trimmed().toUInt() == 772);

		//
		// speed and carrier return EINVAL for a down interface, and speed is
		// simply absent for virtual devices. An empty read leaves the -1
		// sentinel meaning "unknown" rather than reporting a link speed of 0.
		//
		const QString Speed = ReadFileStr(Base + "/speed").trimmed();
		if (!Speed.isEmpty())
		{
			bool bOk = false;
			const qint64 Value = Speed.toLongLong(&bOk);
			if (bOk && Value > 0)
				Device.SpeedMbit = Value;
		}
		Device.Carrier = (ReadFileStr(Base + "/carrier").trimmed() == "1");

		const QString Stats = Base + "/statistics/";
		Device.RxBytes	= ReadFileStr(Stats + "rx_bytes").trimmed().toULongLong();
		Device.RxPackets = ReadFileStr(Stats + "rx_packets").trimmed().toULongLong();
		Device.TxBytes	= ReadFileStr(Stats + "tx_bytes").trimmed().toULongLong();
		Device.TxPackets = ReadFileStr(Stats + "tx_packets").trimmed().toULongLong();

		Devices.append(Device);
	}

	return Devices;
}

QMap<QString, QList<QHostAddress> > ReadDefaultGateways()
{
	QMap<QString, QList<QHostAddress> > Gateways;

	//
	// IPv4: /proc/net/route. A destination of 0.0.0.0 with the gateway flag set
	// is a default route. Both fields are little-endian hex, like the socket
	// tables.
	//
	const QByteArray V4 = ReadFile("/proc/net/route");
	const QList<QByteArray> V4Lines = V4.split('\n');
	for (int i = 1; i < V4Lines.size(); i++)	// line 0 is the header
	{
		const QList<QByteArray> F = V4Lines[i].simplified().split(' ');
		if (F.size() < 4)
			continue;

		if (F[1] != "00000000")
			continue; // not a default route

		// RTF_GATEWAY is 0x0002.
		const quint32 Flags = F[3].toUInt(nullptr, 16);
		if (!(Flags & 0x0002))
			continue;

		bool bOk = false;
		const quint32 Raw = F[2].toUInt(&bOk, 16);
		if (!bOk || !Raw)
			continue;

		const QHostAddress Address(qFromBigEndian<quint32>(qToLittleEndian<quint32>(Raw)));
		Gateways[QString::fromUtf8(F[0])].append(Address);
	}

	//
	// IPv6: /proc/net/ipv6_route, whose columns are
	//   dest plen src plen nexthop metric refcnt use flags iface
	// all as plain hex with no separators. A destination prefix length of 0 is
	// the default route.
	//
	const QByteArray V6 = ReadFile("/proc/net/ipv6_route");
	for (const QByteArray& Line : V6.split('\n'))
	{
		if (Line.isEmpty())
			continue;

		const QList<QByteArray> F = Line.simplified().split(' ');
		if (F.size() < 10)
			continue;

		if (F[1] != "00")
			continue; // prefix length != 0, so not a default route

		const QByteArray NextHop = F[4];
		if (NextHop.size() < 32 || NextHop == QByteArray(32, '0'))
			continue; // no gateway (an on-link route)

		quint8 Bytes[16];
		bool bOk = true;
		for (int b = 0; b < 16 && bOk; b++)
			Bytes[b] = (quint8)NextHop.mid(b * 2, 2).toUShort(&bOk, 16);
		if (!bOk)
			continue;

		Gateways[QString::fromUtf8(F[9])].append(QHostAddress(Bytes));
	}

	return Gateways;
}

SDnsConfig ReadDnsConfig()
{
	SDnsConfig Config;

	auto Parse = [](const QString& Path, SDnsConfig& Out) {
		const QByteArray Data = ReadFile(Path);
		for (const QByteArray& Line : Data.split('\n'))
		{
			const QList<QByteArray> F = Line.simplified().split(' ');
			if (F.size() < 2)
				continue;

			if (F[0] == "nameserver")
			{
				const QHostAddress Address(QString::fromUtf8(F[1]));
				if (!Address.isNull())
					Out.Servers.append(Address);
			}
			else if (F[0] == "search" || F[0] == "domain")
			{
				for (int i = 1; i < F.size(); i++)
				{
					const QString Domain = QString::fromUtf8(F[i]);
					// resolved writes "search ." when there is no real domain.
					if (Domain != ".")
						Out.Domains.insert(Domain);
				}
			}
		}
	};

	Parse("/etc/resolv.conf", Config);

	//
	// If the only server is the systemd-resolved stub listener, the useful
	// answer is upstream of it. resolved publishes the real servers in its own
	// resolv.conf.
	//
	bool bStubOnly = !Config.Servers.isEmpty();
	for (const QHostAddress& Server : Config.Servers)
	{
		if (Server != QHostAddress("127.0.0.53"))
		{
			bStubOnly = false;
			break;
		}
	}

	if (bStubOnly)
	{
		SDnsConfig Upstream;
		Parse("/run/systemd/resolve/resolv.conf", Upstream);
		if (!Upstream.Servers.isEmpty())
		{
			Config.Servers = Upstream.Servers;
			Config.Domains.unite(Upstream.Domains);
		}
	}

	return Config;
}

QMap<QString, QString> ReadOsRelease()
{
	QMap<QString, QString> Values;

	QByteArray Data = ReadFile("/etc/os-release");
	if (Data.isEmpty())
		Data = ReadFile("/usr/lib/os-release");

	for (const QByteArray& Line : Data.split('\n'))
	{
		const int Equals = Line.indexOf('=');
		if (Equals <= 0)
			continue;

		const QByteArray Key = Line.left(Equals).trimmed();
		if (Key.startsWith('#'))
			continue;

		QByteArray Value = Line.mid(Equals + 1).trimmed();

		// Values may be quoted with either single or double quotes.
		if (Value.size() >= 2 &&
		    ((Value.startsWith('"') && Value.endsWith('"')) ||
		     (Value.startsWith('\'') && Value.endsWith('\''))))
		{
			Value = Value.mid(1, Value.size() - 2);
		}

		Values.insert(QString::fromUtf8(Key), QString::fromUtf8(Value));
	}

	return Values;
}

QString ReadCGroupPath(quint64 Pid)
{
	//
	// cgroup v2 has a single "0::<path>" line. v1 has one line per controller,
	// "<id>:<controllers>:<path>"; the systemd hierarchy is the one whose
	// controller field is "name=systemd" (or empty, on the unified hierarchy).
	//
	const QByteArray Data = ReadFile(ProcPath(Pid, "cgroup"));
	if (Data.isEmpty())
		return QString();

	QString CgroupPath;
	for (const QByteArray& Line : Data.split('\n'))
	{
		const QList<QByteArray> F = Line.split(':');
		if (F.size() < 3)
			continue;

		// Rejoin in case the path itself contains a colon.
		QByteArray Path = F.mid(2).join(':');

		if (F[0] == "0" && F[1].isEmpty())	// cgroup v2 unified
			return QString::fromUtf8(Path).trimmed();

		if (F[1] == "name=systemd")		// cgroup v1 systemd hierarchy
			CgroupPath = QString::fromUtf8(Path).trimmed();
	}

	return CgroupPath;
}

SServiceUnit ReadServiceUnit(quint64 Pid)
{
	SServiceUnit Unit;

	const QString CgroupPath = ReadCGroupPath(Pid);
	if (CgroupPath.isEmpty())
		return Unit;

	const QStringList Parts = CgroupPath.split('/', Qt::SkipEmptyParts);
	for (int i = Parts.size() - 1; i >= 0; i--)
	{
		// A scope is a separately tracked unit; anything enclosing it does not
		// own this process for the purpose of naming a service.
		if (Parts[i].endsWith(".scope"))
			return Unit;

		if (Parts[i].endsWith(".service"))
		{
			Unit.Name = Parts[i];
			Unit.bSystemSlice = !Parts.isEmpty() && Parts.first() == "system.slice";
			return Unit;
		}
	}

	return Unit;
}

quint64 ClockTicksPerSec()
{
	static const quint64 Ticks = []() -> quint64 {
		long Value = sysconf(_SC_CLK_TCK);
		return Value > 0 ? (quint64)Value : 100;
	}();
	return Ticks;
}

quint64 PageSize()
{
	static const quint64 Size = []() -> quint64 {
		long Value = sysconf(_SC_PAGESIZE);
		return Value > 0 ? (quint64)Value : 4096;
	}();
	return Size;
}

double UpTime()
{
	const QList<QByteArray> F = ReadFile("/proc/uptime").simplified().split(' ');
	if (F.isEmpty())
		return 0.0;
	return F[0].toDouble();
}

quint64 StartTimeToEpochMs(quint64 StartTimeTicks)
{
	//
	// stat.StartTime is measured in clock ticks since boot, so it needs the
	// boot wall-clock time from /proc/stat's btime to become an absolute
	// timestamp.
	//
	static const quint64 BootTimeSec = []() -> quint64 {
		return ReadSysStat().BootTime;
	}();

	if (!BootTimeSec)
		return 0;

	return (BootTimeSec * 1000ULL) + (StartTimeTicks * 1000ULL / ClockTicksPerSec());
}

QString UserNameFromUid(quint32 Uid)
{
	//
	// getpwuid_r needs a caller-supplied buffer; _SC_GETPW_R_SIZE_MAX is only a
	// hint, so fall back to a sane size and grow once on ERANGE.
	//
	long Hint = sysconf(_SC_GETPW_R_SIZE_MAX);
	size_t BufSize = (Hint > 0) ? (size_t)Hint : 4096;

	for (int Attempt = 0; Attempt < 2; Attempt++)
	{
		QByteArray Buffer(BufSize, '\0');
		struct passwd Pwd;
		struct passwd* pResult = nullptr;

		int Ret = getpwuid_r(Uid, &Pwd, Buffer.data(), Buffer.size(), &pResult);
		if (Ret == ERANGE)
		{
			BufSize *= 4;
			continue;
		}
		if (Ret != 0 || !pResult)
			break;

		return QString::fromUtf8(pResult->pw_name);
	}

	return QString::number(Uid);
}

QString GroupNameFromGid(quint32 Gid)
{
	long Hint = sysconf(_SC_GETGR_R_SIZE_MAX);
	size_t BufSize = (Hint > 0) ? (size_t)Hint : 4096;

	for (int Attempt = 0; Attempt < 2; Attempt++)
	{
		QByteArray Buffer(BufSize, '\0');
		struct group Grp;
		struct group* pResult = nullptr;

		int Ret = getgrgid_r(Gid, &Grp, Buffer.data(), Buffer.size(), &pResult);
		if (Ret == ERANGE)
		{
			BufSize *= 4;
			continue;
		}
		if (Ret != 0 || !pResult)
			break;

		return QString::fromUtf8(pResult->gr_name);
	}

	return QString::number(Gid);
}

QMap<QString, quint64> ReadVmStat()
{
	QMap<QString, quint64> Stats;

	// One "name value" pair per line, whitespace separated.
	const QByteArray Data = ReadFile("/proc/vmstat");
	for (const QByteArray& Line : Data.split('\n'))
	{
		const int Sep = Line.indexOf(' ');
		if (Sep < 0)
			continue;
		Stats.insert(QString::fromUtf8(Line.left(Sep)), Line.mid(Sep + 1).trimmed().toULongLong());
	}

	return Stats;
}

// ---- cgroups ----

//
// cgroup files spell "no limit" as the literal "max" rather than as a number,
// which would otherwise parse as 0 and read as "limited to nothing".
//
static quint64 ParseCGroupLimit(const QString& Value)
{
	const QString Trimmed = Value.trimmed();
	if (Trimmed.isEmpty() || Trimmed == "max")
		return 0;
	return Trimmed.toULongLong();
}

static QString CGroupFilePath(const QString& CGroupPath, const QString& Leaf)
{
	// The unified hierarchy is mounted at /sys/fs/cgroup, and the path from
	// /proc/<pid>/cgroup is relative to that mount point.
	return "/sys/fs/cgroup" + CGroupPath + "/" + Leaf;
}

SPressure ParsePressure(const QByteArray& Data)
{
	SPressure Pressure;

	//
	// Two lines, "some" and "full", each:
	//   some avg10=0.00 avg60=0.00 avg300=0.03 total=83030100
	//
	// "full" is absent for the cpu resource on some kernels, which is why each
	// line is matched by its leading keyword rather than by position.
	//
	for (const QByteArray& Line : Data.split('\n'))
	{
		const QList<QByteArray> Fields = Line.simplified().split(' ');
		if (Fields.size() < 2)
			continue;

		const bool bFull = (Fields[0] == "full");
		if (!bFull && Fields[0] != "some")
			continue;

		for (int i = 1; i < Fields.size(); i++)
		{
			const int Eq = Fields[i].indexOf('=');
			if (Eq < 0)
				continue;

			const QByteArray Key = Fields[i].left(Eq);
			const QByteArray Value = Fields[i].mid(Eq + 1);

			if (Key == "avg10")			(bFull ? Pressure.FullAvg10 : Pressure.SomeAvg10) = Value.toFloat();
			else if (Key == "avg60")	(bFull ? Pressure.FullAvg60 : Pressure.SomeAvg60) = Value.toFloat();
			else if (Key == "avg300")	(bFull ? Pressure.FullAvg300 : Pressure.SomeAvg300) = Value.toFloat();
			else if (Key == "total")	(bFull ? Pressure.FullTotal : Pressure.SomeTotal) = Value.toULongLong();
		}

		Pressure.Valid = true;
	}

	return Pressure;
}

SPressure ReadSysPressure(const QString& Resource)
{
	// Absent when the kernel was built without CONFIG_PSI or booted with
	// psi=0, in which case Valid stays false and the caller shows nothing.
	return ParsePressure(ReadFile("/proc/pressure/" + Resource));
}

SPressure ReadCGroupPressure(const QString& CGroupPath, const QString& Resource)
{
	if (CGroupPath.isEmpty())
		return SPressure();
	return ParsePressure(ReadFile(CGroupFilePath(CGroupPath, Resource + ".pressure")));
}

SCGroupStats ReadCGroupStats(const QString& CGroupPath)
{
	SCGroupStats Stats;
	if (CGroupPath.isEmpty())
		return Stats;

	//
	// Which files exist depends on which controllers are enabled for this
	// cgroup, so every read is optional and an absent file simply leaves its
	// fields at zero.
	//
	auto ReadValue = [&](const QString& Leaf) -> QString {
		return ReadFileStr(CGroupFilePath(CGroupPath, Leaf)).trimmed();
	};

	const QString MemoryCurrent = ReadValue("memory.current");
	if (!MemoryCurrent.isEmpty())
	{
		Stats.Valid = true;
		Stats.MemoryCurrent = MemoryCurrent.toULongLong();
		Stats.MemoryPeak = ReadValue("memory.peak").toULongLong();
		Stats.MemoryMax = ParseCGroupLimit(ReadValue("memory.max"));
		Stats.MemoryHigh = ParseCGroupLimit(ReadValue("memory.high"));
		Stats.MemorySwapCurrent = ReadValue("memory.swap.current").toULongLong();
		Stats.MemorySwapMax = ParseCGroupLimit(ReadValue("memory.swap.max"));
	}

	// cpu.stat is "key value" per line; the throttling keys appear only once a
	// cpu.max limit has been set.
	const QByteArray CpuStat = ReadFile(CGroupFilePath(CGroupPath, "cpu.stat"));
	for (const QByteArray& Line : CpuStat.split('\n'))
	{
		const QList<QByteArray> Fields = Line.simplified().split(' ');
		if (Fields.size() < 2)
			continue;

		const quint64 Value = Fields[1].toULongLong();
		if (Fields[0] == "usage_usec")				Stats.CpuUsageUs = Value;
		else if (Fields[0] == "user_usec")			Stats.CpuUserUs = Value;
		else if (Fields[0] == "system_usec")		Stats.CpuSystemUs = Value;
		else if (Fields[0] == "nr_periods")			Stats.NrPeriods = Value;
		else if (Fields[0] == "nr_throttled")		Stats.NrThrottled = Value;
		else if (Fields[0] == "throttled_usec")		Stats.ThrottledUs = Value;

		Stats.Valid = true;
	}

	const QString PidsCurrent = ReadValue("pids.current");
	if (!PidsCurrent.isEmpty())
	{
		Stats.Valid = true;
		Stats.PidsCurrent = PidsCurrent.toULongLong();
		Stats.PidsMax = ParseCGroupLimit(ReadValue("pids.max"));
	}

	//
	// io.stat has one line per block device:
	//   8:0 rbytes=123 wbytes=456 rios=7 wios=8 dbytes=0 dios=0
	// Summed here, because which device did the I/O is not what this view is
	// about.
	//
	const QByteArray IoStat = ReadFile(CGroupFilePath(CGroupPath, "io.stat"));
	for (const QByteArray& Line : IoStat.split('\n'))
	{
		for (const QByteArray& Field : Line.simplified().split(' '))
		{
			if (Field.startsWith("rbytes="))
				Stats.IoReadBytes += Field.mid(7).toULongLong();
			else if (Field.startsWith("wbytes="))
				Stats.IoWriteBytes += Field.mid(7).toULongLong();
		}
	}

	Stats.Controllers = ReadValue("cgroup.controllers").split(' ', Qt::SkipEmptyParts);

	return Stats;
}

// ---- namespaces ----

SNamespaces ReadNamespaces(quint64 Pid)
{
	SNamespaces Namespaces;

	//
	// Each link reads as "<type>:[<inode>]"; the inode is the namespace's
	// identity. Two processes share a namespace exactly when the inodes match.
	//
	auto ReadOne = [Pid](const char* Name) -> quint64 {
		const QString Link = ReadLink(ProcPath(Pid, QString("ns/%1").arg(Name)));
		const int Open = Link.indexOf('[');
		if (Open < 0 || !Link.endsWith(']'))
			return 0;
		return QStringView(Link).mid(Open + 1, Link.length() - Open - 2).toULongLong();
	};

	Namespaces.Pid = ReadOne("pid");
	Namespaces.Net = ReadOne("net");
	Namespaces.Mnt = ReadOne("mnt");
	Namespaces.User = ReadOne("user");
	Namespaces.Uts = ReadOne("uts");
	Namespaces.Ipc = ReadOne("ipc");
	Namespaces.CGroup = ReadOne("cgroup");
	Namespaces.Time = ReadOne("time");

	return Namespaces;
}

// ---- security ----

SProcSecurity ReadProcSecurity(quint64 Pid)
{
	SProcSecurity Security;

	const QMap<QString, QString> Status = ReadStatus(Pid);
	if (Status.isEmpty())
		return Security;

	Security.Valid = true;

	// The capability sets are hexadecimal masks, without a 0x prefix.
	Security.CapInh = Status.value("CapInh").trimmed().toULongLong(nullptr, 16);
	Security.CapPrm = Status.value("CapPrm").trimmed().toULongLong(nullptr, 16);
	Security.CapEff = Status.value("CapEff").trimmed().toULongLong(nullptr, 16);
	Security.CapBnd = Status.value("CapBnd").trimmed().toULongLong(nullptr, 16);
	Security.CapAmb = Status.value("CapAmb").trimmed().toULongLong(nullptr, 16);

	Security.Seccomp = Status.value("Seccomp").trimmed().toInt();
	Security.SeccompFilters = Status.value("Seccomp_filters").trimmed().toULongLong();
	Security.NoNewPrivs = Status.value("NoNewPrivs").trimmed().toInt() != 0;

	//
	// attr/current is the active LSM label. The file does not exist when no LSM
	// is compiled in, and reading it for another user's process needs the same
	// access as the rest of /proc.
	//
	// The value is NUL terminated rather than newline terminated, which trips
	// up naive readers.
	//
	QString Confinement = QString::fromUtf8(ReadFile(ProcPath(Pid, "attr/current")));
	const int Nul = Confinement.indexOf(QChar('\0'));
	if (Nul >= 0)
		Confinement.truncate(Nul);
	Security.Confinement = Confinement.trimmed();

	return Security;
}

QStringList DecodeCapabilities(quint64 Mask)
{
	//
	// Indexed by capability number, as defined in <linux/capability.h>. Listed
	// literally rather than pulled from the header so that a kernel newer than
	// the build machine's headers still names everything it reports.
	//
	static const char* Names[] = {
		"CAP_CHOWN", "CAP_DAC_OVERRIDE", "CAP_DAC_READ_SEARCH", "CAP_FOWNER",
		"CAP_FSETID", "CAP_KILL", "CAP_SETGID", "CAP_SETUID",
		"CAP_SETPCAP", "CAP_LINUX_IMMUTABLE", "CAP_NET_BIND_SERVICE", "CAP_NET_BROADCAST",
		"CAP_NET_ADMIN", "CAP_NET_RAW", "CAP_IPC_LOCK", "CAP_IPC_OWNER",
		"CAP_SYS_MODULE", "CAP_SYS_RAWIO", "CAP_SYS_CHROOT", "CAP_SYS_PTRACE",
		"CAP_SYS_PACCT", "CAP_SYS_ADMIN", "CAP_SYS_BOOT", "CAP_SYS_NICE",
		"CAP_SYS_RESOURCE", "CAP_SYS_TIME", "CAP_SYS_TTY_CONFIG", "CAP_MKNOD",
		"CAP_LEASE", "CAP_AUDIT_WRITE", "CAP_AUDIT_CONTROL", "CAP_SETFCAP",
		"CAP_MAC_OVERRIDE", "CAP_MAC_ADMIN", "CAP_SYSLOG", "CAP_WAKE_ALARM",
		"CAP_BLOCK_SUSPEND", "CAP_AUDIT_READ", "CAP_PERFMON", "CAP_BPF",
		"CAP_CHECKPOINT_RESTORE",
	};
	static const int Count = (int)(sizeof(Names) / sizeof(Names[0]));

	QStringList Capabilities;
	for (int i = 0; i < 64; i++)
	{
		if (!(Mask & (1ULL << i)))
			continue;

		if (i < Count)
			Capabilities.append(Names[i]);
		else
			Capabilities.append(QString("CAP_%1").arg(i));	// added since this table
	}

	return Capabilities;
}

QString SeccompModeToString(int Mode)
{
	switch (Mode)
	{
		case 0:		return QObject::tr("Disabled");
		case 1:		return QObject::tr("Strict");
		case 2:		return QObject::tr("Filtered");
		default:	return QObject::tr("Unknown (%1)").arg(Mode);
	}
}

// ---- out of memory killer ----

SOomInfo ReadOomInfo(quint64 Pid)
{
	SOomInfo Info;

	const QString Score = ReadFileStr(ProcPath(Pid, "oom_score")).trimmed();
	if (Score.isEmpty())
		return Info;

	Info.Valid = true;
	Info.Score = Score.toInt();
	Info.ScoreAdj = ReadFileStr(ProcPath(Pid, "oom_score_adj")).trimmed().toInt();

	return Info;
}

bool WriteOomScoreAdj(quint64 Pid, int Value)
{
	//
	// Written with the raw syscalls rather than QFile.
	//
	// QFile buffers, so a write the kernel is going to refuse still reports the
	// full byte count; the refusal only surfaces when the buffer is flushed at
	// close, long after the caller has been told it succeeded. Lowering this
	// value needs CAP_SYS_RESOURCE, so that refusal is the normal case for an
	// unprivileged user and must not be swallowed.
	//
	// Going direct also keeps errno intact for the caller's error message.
	//
	const QByteArray Path = ProcPath(Pid, "oom_score_adj").toLocal8Bit();

	const int Fd = ::open(Path.constData(), O_WRONLY | O_CLOEXEC);
	if (Fd < 0)
		return false;

	const QByteArray Data = QByteArray::number(Value);
	const ssize_t Written = ::write(Fd, Data.constData(), Data.size());

	// Preserve the write's errno across close(), which can overwrite it.
	const int Error = errno;
	::close(Fd);
	errno = Error;

	return Written == (ssize_t)Data.size();
}

// ---- inotify ----

quint64 CountInotifyWatches(quint64 Pid)
{
	quint64 Watches = 0;

	//
	// An inotify descriptor's fdinfo lists one "inotify wd:..." line per watch,
	// so the watches are counted by counting those lines.
	//
	// Descriptors are filtered by their link target first. A readlink is a
	// single syscall, where opening and reading fdinfo for every descriptor of
	// every process would mean thousands of file reads per refresh to find the
	// handful of descriptors that are actually inotify ones.
	//
	foreach(quint64 Fd, EnumFds(Pid))
	{
		if (ReadLink(ProcPath(Pid, QString("fd/%1").arg(Fd))) != "anon_inode:inotify")
			continue;

		const QByteArray Data = ReadFile(ProcPath(Pid, QString("fdinfo/%1").arg(Fd)));
		for (const QByteArray& Line : Data.split('\n'))
		{
			if (Line.startsWith("inotify wd:"))
				Watches++;
		}
	}

	return Watches;
}

} // namespace ProcFs
