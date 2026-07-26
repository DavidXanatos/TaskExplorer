#include "stdafx.h"
#include "LinuxHelper.h"
#include "ProcFs.h"
#include "../../SVC/TaskService.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHash>
#include <QMutex>
#include <QProcess>
#include <QStandardPaths>

#include <algorithm>

#include <errno.h>
#include <sched.h>
#include <string.h>
#include <sys/syscall.h>
#include <unistd.h>

STATUS ErrnoToStatus(const QString& Context, int Error)
{
	if (Error == 0)
		return OK;

	// strerror_r has two incompatible signatures; formatting the number
	// alongside the message keeps this portable and still useful.
	char Buffer[256] = { 0 };
	const char* pMessage = strerror_r(Error, Buffer, sizeof(Buffer));
	if (!pMessage)
		pMessage = Buffer;

	return ERR(QString("%1: %2 (errno %3)").arg(Context).arg(QString::fromLocal8Bit(pMessage)).arg(Error), Error);
}

STATUS ErrnoToStatus(const QString& Context)
{
	return ErrnoToStatus(Context, errno);
}

QString LinuxStateToString(char State)
{
	switch (State)
	{
		case 'R': return QObject::tr("Running");
		case 'S': return QObject::tr("Sleeping");
		case 'D': return QObject::tr("Disk Sleep");
		case 'Z': return QObject::tr("Zombie");
		case 'T': return QObject::tr("Stopped");
		case 't': return QObject::tr("Tracing Stop");
		case 'X':
		case 'x': return QObject::tr("Dead");
		case 'K': return QObject::tr("Wakekill");
		case 'W': return QObject::tr("Waking");
		case 'P': return QObject::tr("Parked");
		case 'I': return QObject::tr("Idle");
	}
	return QObject::tr("Unknown");
}

QString LinuxSchedPolicyToString(int Policy)
{
	switch (Policy)
	{
		case SCHED_OTHER:	return QObject::tr("Normal");
		case SCHED_FIFO:	return QObject::tr("FIFO");
		case SCHED_RR:		return QObject::tr("Round Robin");
		case SCHED_BATCH:	return QObject::tr("Batch");
		case SCHED_IDLE:	return QObject::tr("Idle");
#ifdef SCHED_DEADLINE
		case SCHED_DEADLINE:	return QObject::tr("Deadline");
#endif
	}
	return QObject::tr("Unknown");
}

QString LinuxNiceToPriorityString(int Nice)
{
	//
	// The shared GUI expects Windows-style priority class names. Bucketing the
	// nice range onto them keeps the process list readable without inventing a
	// Linux-specific column, at the cost of being approximate by nature.
	//
	if (Nice <= -15)	return QObject::tr("Realtime");
	if (Nice <= -5)		return QObject::tr("High");
	if (Nice < 0)		return QObject::tr("Above Normal");
	if (Nice == 0)		return QObject::tr("Normal");
	if (Nice <= 9)		return QObject::tr("Below Normal");
	return QObject::tr("Idle");
}

//
// ioprio_get/ioprio_set have no glibc wrappers, so they are issued directly.
//
#ifndef IOPRIO_WHO_PROCESS
#define IOPRIO_WHO_PROCESS 1
#endif

#define IOPRIO_CLASS_SHIFT 13
#define IOPRIO_PRIO_MASK   ((1UL << IOPRIO_CLASS_SHIFT) - 1)

int LinuxGetIoPrio(quint64 Pid)
{
	return (int)syscall(SYS_ioprio_get, IOPRIO_WHO_PROCESS, (int)Pid);
}

int LinuxSetIoPrio(quint64 Pid, int Priority)
{
	return (int)syscall(SYS_ioprio_set, IOPRIO_WHO_PROCESS, (int)Pid, Priority);
}

int LinuxMakeIoPrio(int Class, int Level)
{
	return (Class << IOPRIO_CLASS_SHIFT) | (Level & IOPRIO_PRIO_MASK);
}

quint64 LinuxGetAffinity(quint64 Pid, bool* pTruncated)
{
	if (pTruncated)
		*pTruncated = false;

	cpu_set_t Set;
	CPU_ZERO(&Set);

	if (sched_getaffinity((pid_t)Pid, sizeof(Set), &Set) != 0)
		return 0;

	quint64 Mask = 0;
	const int Count = CPU_SETSIZE;
	for (int i = 0; i < Count; i++)
	{
		if (!CPU_ISSET(i, &Set))
			continue;

		if (i >= 64)
		{
			// Beyond what the 64 bit interface can carry.
			if (pTruncated)
				*pTruncated = true;
			break;
		}
		Mask |= (1ULL << i);
	}

	return Mask;
}

bool LinuxSetAffinity(quint64 Pid, quint64 Mask)
{
	cpu_set_t Set;
	CPU_ZERO(&Set);

	for (int i = 0; i < 64; i++)
	{
		if (Mask & (1ULL << i))
			CPU_SET(i, &Set);
	}

	return sched_setaffinity((pid_t)Pid, sizeof(Set), &Set) == 0;
}

// ---- privileged reads, by way of TaskHelper ----

bool LinuxHelperNeeded()
{
	// Already privileged: a direct read is cheaper than an IPC round trip.
	return geteuid() != 0;
}

//
// Talks to an elevated helper, starting one if needed.
//
// The socket name is cached by CTaskService, so the authentication prompt happens
// on the first privileged request of a session rather than on each one. A failure
// here is expected and unremarkable - the user may simply have declined - so it
// returns an invalid variant rather than raising.
//
static QVariant LinuxHelperCall(const QString& Command, const QVariantMap& Parameters, int TimeoutMs = 10000)
{
	//
	// Elevated, unlike the stack tracer: the whole point of these calls is to see
	// what the current user cannot. If we are already root the helper inherits
	// that and no prompt appears.
	//
	const QString Socket = CTaskService::RunWorker(true);
	if (Socket.isEmpty())
		return QVariant();

	QVariantMap Request;
	Request["Command"] = Command;
	Request["Parameters"] = Parameters;

	const QVariant Reply = CTaskService::SendCommand(Socket, Request, TimeoutMs);

	//
	// The helper reports a rejected request as a bare string. Those must not reach
	// the callers, because QVariant will happily coerce one into whatever shape is
	// asked of it - toList() on "Invalid Parameters" yields eighteen elements, one
	// per character, and toMap() an empty map that reads as "no data" rather than
	// as an error. Turn them into an invalid variant, which every caller already
	// treats as "nothing came back".
	//
	if (Reply.typeId() == QMetaType::QString)
	{
		const QString Text = Reply.toString();
		if (Text == "Invalid Parameters" || Text == "Unknown Command")
		{
			qDebug() << "TaskHelper rejected" << Command << ":" << Text;
			return QVariant();
		}
	}

	return Reply;
}

QMap<quint64, QMap<QString, QByteArray>> LinuxHelperReadProcFiles(const QList<quint64>& Pids, const QStringList& Leaves)
{
	QMap<quint64, QMap<QString, QByteArray>> Result;
	if (Pids.isEmpty() || Leaves.isEmpty())
		return Result;

	QVariantList PidList;
	foreach(quint64 Pid, Pids)
		PidList.append(Pid);

	QVariantMap Parameters;
	Parameters["Pids"] = PidList;
	Parameters["Leaves"] = QVariant(Leaves);

	const QVariantMap Reply = LinuxHelperCall("ReadProcFiles", Parameters).toMap();

	for (auto I = Reply.begin(); I != Reply.end(); ++I)
	{
		bool bOk = false;
		const quint64 Pid = I.key().toULongLong(&bOk);
		if (!bOk)
			continue;

		QMap<QString, QByteArray> Files;
		const QVariantMap FileMap = I.value().toMap();
		for (auto J = FileMap.begin(); J != FileMap.end(); ++J)
			Files.insert(J.key(), J.value().toByteArray());

		Result.insert(Pid, Files);
	}

	return Result;
}

QByteArray LinuxHelperReadProcFile(quint64 Pid, const QString& Leaf)
{
	const QMap<quint64, QMap<QString, QByteArray>> Reply =
		LinuxHelperReadProcFiles(QList<quint64>() << Pid, QStringList() << Leaf);

	return Reply.value(Pid).value(Leaf);
}

QString LinuxHelperReadProcLink(quint64 Pid, const QString& Leaf)
{
	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;
	Parameters["Leaf"] = Leaf;

	return LinuxHelperCall("ReadProcLink", Parameters).toString();
}

QList<QMap<QString, QVariant>> LinuxHelperListFds(quint64 Pid)
{
	QList<QMap<QString, QVariant>> Result;

	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;

	foreach(const QVariant& Entry, LinuxHelperCall("ListProcFds", Parameters).toList())
		Result.append(Entry.toMap());

	return Result;
}

QByteArray LinuxHelperReadMemory(quint64 Pid, quint64 Address, quint64 Size)
{
	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;
	Parameters["Address"] = Address;
	Parameters["Size"] = Size;

	return LinuxHelperCall("ReadProcMemory", Parameters).toByteArray();
}

bool LinuxHelperDumpAttach(quint64 Pid, SHelperDumpInfo* pInfo)
{
	if (!pInfo)
		return false;

	*pInfo = SHelperDumpInfo();

	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;

	//
	// A generous timeout: this stops every thread of the target and reads its
	// register sets, which on a process with hundreds of threads takes noticeably
	// longer than an ordinary request.
	//
	const QVariant Reply = LinuxHelperCall("DumpAttach", Parameters, 60000);
	if (Reply.typeId() != QMetaType::QVariantMap)
		return false;

	const QVariantMap Map = Reply.toMap();

	foreach(const QVariant& Entry, Map.value("Threads").toList())
	{
		const QVariantMap ThreadMap = Entry.toMap();

		SHelperDumpThread Thread;
		Thread.Tid = ThreadMap.value("Tid").toULongLong();
		Thread.Attached = ThreadMap.value("Attached").toBool();
		Thread.Regs = ThreadMap.value("Regs").toByteArray();
		Thread.FpRegs = ThreadMap.value("FpRegs").toByteArray();

		if (Thread.Tid)
			pInfo->Threads.append(Thread);
	}

	pInfo->Maps = Map.value("Maps").toByteArray();
	pInfo->Auxv = Map.value("Auxv").toByteArray();
	pInfo->AttachErrno = Map.value("AttachErrno").toInt();
	pInfo->Valid = true;

	return true;
}

void LinuxHelperDumpDetach(quint64 Pid)
{
	QVariantMap Parameters;
	Parameters["ProcessId"] = Pid;

	LinuxHelperCall("DumpDetach", Parameters);
}

QString LinuxDescribeContainer(quint64 Pid, const ProcFs::SNamespaces& Namespaces,
                               const QString& CGroupPath, const QString& Confinement)
{
	//
	// pid 1's namespaces define "the host". Read once: init does not change
	// namespaces, and this would otherwise be eight readlinks per process.
	//
	static const ProcFs::SNamespaces Host = ProcFs::ReadNamespaces(1);

	//
	// Runtime conventions first, because they can name the container rather
	// than merely detect it.
	//
	// The cgroup path is the usual giveaway. Docker uses
	// ".../docker-<64 hex>.scope" under systemd, or "/docker/<64 hex>"
	// otherwise; podman uses "libpod-<id>"; LXC and systemd-nspawn put their
	// machines under machine.slice.
	//
	for (const QString& Part : CGroupPath.split('/', Qt::SkipEmptyParts))
	{
		QString Id = Part;
		if (Id.endsWith(".scope"))
			Id.chop(6);

		// The ids are long hashes; the leading 12 characters are what the
		// runtimes themselves display.
		auto ShortId = [](const QString& Value) {
			return Value.length() > 12 ? Value.left(12) : Value;
		};

		if (Id.startsWith("docker-"))
			return QString("docker: %1").arg(ShortId(Id.mid(7)));
		if (Id.startsWith("libpod-"))
			return QString("podman: %1").arg(ShortId(Id.mid(7)));
		if (Id.startsWith("crio-"))
			return QString("cri-o: %1").arg(ShortId(Id.mid(5)));
		if (Id.startsWith("lxc.payload."))
			return QString("lxc: %1").arg(Id.mid(12));
		if (Id.startsWith("machine-"))
		{
			// systemd-nspawn escapes the machine name, "-" becoming "\x2d".
			QString Name = Id.mid(8);
			Name.replace("\\x2d", "-");
			return QString("machine: %1").arg(Name);
		}
	}

	// Docker without systemd cgroup driver.
	if (CGroupPath.startsWith("/docker/"))
		return QString("docker: %1").arg(CGroupPath.mid(8).left(12));

	//
	// Snap and flatpak are sandboxes rather than containers, but they are what
	// a desktop user actually encounters, and both are identifiable.
	//
	// The snap name comes from the AppArmor profile, which is where snapd puts
	// it; flatpak leaves a .flatpak-info in the sandbox's root.
	//
	if (Confinement.startsWith("snap."))
	{
		// "snap.firefox.firefox (enforce)" -> "firefox"
		const QString Profile = Confinement.section(' ', 0, 0);
		return QString("snap: %1").arg(Profile.section('.', 1, 1));
	}

	if (ProcFs::FileExists(ProcFs::ProcPath(Pid, "root/.flatpak-info")))
	{
		const QByteArray Info = ProcFs::ReadFile(ProcFs::ProcPath(Pid, "root/.flatpak-info"));
		for (const QByteArray& Line : Info.split('\n'))
		{
			if (Line.startsWith("name="))
				return QString("flatpak: %1").arg(QString::fromUtf8(Line.mid(5)).trimmed());
		}
		return "flatpak";
	}

	//
	// Nothing recognised. Fall back to reporting which namespaces differ from
	// the host's, which at least says how the process is isolated even when
	// what isolated it cannot be named.
	//
	// A zero means the link was unreadable rather than shared, so those are
	// skipped instead of being reported as differences.
	//
	QStringList Differing;
	auto Compare = [&Differing](quint64 Value, quint64 HostValue, const char* Name) {
		if (Value && HostValue && Value != HostValue)
			Differing.append(Name);
	};

	Compare(Namespaces.Pid, Host.Pid, "pid");
	Compare(Namespaces.Net, Host.Net, "net");
	Compare(Namespaces.Mnt, Host.Mnt, "mnt");
	Compare(Namespaces.User, Host.User, "user");
	Compare(Namespaces.Uts, Host.Uts, "uts");
	Compare(Namespaces.Ipc, Host.Ipc, "ipc");

	if (Differing.isEmpty())
		return QString();

	return QString("namespaced (%1)").arg(Differing.join(", "));
}

STATUS LinuxRunInTerminal(const QString& Program, const QStringList& Arguments)
{
	//
	// Terminals disagree on how a command is handed to them, so each candidate
	// carries its own separator. An empty separator means the terminal takes
	// the command as trailing arguments with no flag at all.
	//
	// "--" is the modern spelling and the only correct one for
	// gnome-terminal, whose -e was deprecated and re-parses its argument as a
	// shell word list.
	//
	static const struct { const char* Program; const char* Separator; } Terminals[] =
	{
		{ "x-terminal-emulator",	"-e" },		// the Debian alternative: whatever the user chose
		{ "konsole",				"-e" },
		{ "gnome-terminal",			"--" },
		{ "kgx",					"--" },		// GNOME Console
		{ "xfce4-terminal",			"-x" },		// -x takes the rest of the line as the command
		{ "mate-terminal",			"--" },
		{ "tilix",					"-e" },
		{ "alacritty",				"-e" },
		{ "kitty",					""   },
		{ "foot",					""   },
		{ "wezterm",				"start" },
		{ "xterm",					"-e" },
	};

	QStringList Tried;
	for (size_t i = 0; i < sizeof(Terminals) / sizeof(Terminals[0]); i++)
	{
		const QString Terminal = QStandardPaths::findExecutable(Terminals[i].Program);
		if (Terminal.isEmpty())
		{
			Tried.append(Terminals[i].Program);
			continue;
		}

		QStringList Args;
		if (Terminals[i].Separator[0])
			Args << Terminals[i].Separator;
		Args << Program << Arguments;

		if (QProcess::startDetached(Terminal, Args))
			return OK;

		Tried.append(Terminals[i].Program);
	}

	return ERR(QObject::tr("No terminal emulator could be started. Looked for: %1.").arg(Tried.join(", ")));
}

QString LinuxResolveAddress(quint64 Pid, quint64 Address, quint64* pOffset, QString* pModulePath)
{
	if (pOffset)
		*pOffset = 0;
	if (pModulePath)
		pModulePath->clear();

	if (!Address)
		return QString();

	//
	// A one-entry cache, which is all this needs: the callers walk the threads
	// of one process in a row, so the same mapping table is wanted many times
	// in immediate succession and then never again.
	//
	static QMutex Lock;
	static quint64 CachedPid = 0;
	static qint64 CachedAt = 0;
	static QList<ProcFs::SMapEntry> CachedMaps;

	QMutexLocker Locker(&Lock);

	const qint64 Now = QDateTime::currentMSecsSinceEpoch();
	if (CachedPid != Pid || Now - CachedAt > 2000)
	{
		CachedMaps = ProcFs::ReadMaps(Pid);
		CachedPid = Pid;
		CachedAt = Now;
	}

	foreach(const ProcFs::SMapEntry& Map, CachedMaps)
	{
		if (Address < Map.Start || Address >= Map.End)
			continue;

		if (Map.Path.isEmpty() || Map.Path.startsWith('['))
			break;	// anonymous or pseudo mapping; there is no module to name

		//
		// The offset is measured from the start of the *file*, not the start of
		// this mapping, so that it matches what a debugger or addr2line reports
		// for a binary whose segments are mapped separately.
		//
		const quint64 Offset = Map.Offset + (Address - Map.Start);

		if (pOffset)
			*pOffset = Offset;
		if (pModulePath)
			*pModulePath = Map.Path;

		return QString("%1+0x%2").arg(Map.Path.section('/', -1)).arg(Offset, 0, 16);
	}

	return "0x" + QString::number(Address, 16);
}

//
// Builds the executable-basename -> display-name index out of the .desktop
// files in the standard applications directories.
//
// Only the plain "Name" key is used, never a localised "Name[xx]" one: this
// string ends up next to process names and file paths, which are not
// translated, and a half translated tree reads worse than an untranslated one.
//
static QHash<QString, QString> BuildDesktopIndex()
{
	QHash<QString, QString> Index;

	//
	// XDG search order, least significant first, so that a user's own entry in
	// ~/.local/share overrides the packaged one.
	//
	QStringList Dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
	std::reverse(Dirs.begin(), Dirs.end());

	foreach(const QString& Dir, Dirs)
	{
		QDirIterator It(Dir, QStringList() << "*.desktop", QDir::Files, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			QFile File(It.next());
			if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
				continue;

			QString Name;
			QString Exec;
			bool bInEntry = false;

			while (!File.atEnd())
			{
				const QString Line = QString::fromUtf8(File.readLine()).trimmed();

				if (Line.startsWith('['))
				{
					// Only the main group describes the application itself;
					// the "Desktop Action ..." groups describe menu entries.
					bInEntry = (Line == "[Desktop Entry]");
					if (!bInEntry && !Name.isEmpty() && !Exec.isEmpty())
						break;
					continue;
				}

				if (!bInEntry)
					continue;

				if (Name.isEmpty() && Line.startsWith("Name="))
					Name = Line.mid(5).trimmed();
				else if (Exec.isEmpty() && Line.startsWith("Exec="))
					Exec = Line.mid(5).trimmed();
			}

			if (Name.isEmpty() || Exec.isEmpty())
				continue;

			//
			// Exec is a command line: field codes (%u, %f, ...), arguments and
			// wrapper programs all have to come off to get at the program that
			// will actually be running. "env FOO=1 /usr/bin/foo %u" has to
			// yield "foo".
			//
			const QStringList Tokens = Exec.split(' ', Qt::SkipEmptyParts);
			for (int i = 0; i < Tokens.count(); i++)
			{
				QString Token = Tokens[i];

				if (Token.startsWith('%'))		// field code, and everything after it is arguments
					break;
				if (Token.contains('='))		// an environment assignment for "env"
					continue;

				const QString Base = Token.section('/', -1);
				if (Base == "env" || Base == "sh" || Base == "bash" || Base == "flatpak" || Base == "snap")
					continue;					// a wrapper; the real program is further along

				if (!Base.isEmpty())
					Index.insert(Base, Name);
				break;
			}
		}
	}

	return Index;
}

QString LinuxDescribeExecutable(const QString& Path)
{
	if (Path.isEmpty())
		return QString();

	static QMutex Lock;
	static QHash<QString, QString> Index;
	static bool bBuilt = false;

	QMutexLocker Locker(&Lock);
	if (!bBuilt)
	{
		//
		// Built once and kept. A desktop entry appearing while TaskExplorer is
		// running would be missed, which is a fair trade for not walking a few
		// thousand files on every process-list refresh.
		//
		Index = BuildDesktopIndex();
		bBuilt = true;
	}

	return Index.value(Path.section('/', -1));
}

STATUS LinuxRunElevated(const QString& Program, const QStringList& Arguments, qint64* pPid)
{
	if (pPid)
		*pPid = 0;

	const QByteArray SessionType = qgetenv("XDG_SESSION_TYPE");
	const QString Display = qEnvironmentVariable("DISPLAY");

	//
	// A Wayland compositor will not accept a connection from a process running
	// as a different user, so a root GUI instance has nothing to display on.
	// Under XWayland DISPLAY is still set and this can work, hence the check
	// for the display rather than the session type alone.
	//
	if (Display.isEmpty())
	{
		if (SessionType == "wayland")
		{
			return ERR(QObject::tr("Cannot restart elevated under a Wayland session: a compositor does not accept "
			                       "connections from a process running as another user. Run TaskExplorer from a "
			                       "terminal with 'sudo' instead."));
		}
		return ERR(QObject::tr("Cannot restart elevated: no X display is available."));
	}

	//
	// Helper preference:
	//
	//   pkexec     polkit's own, present wherever polkit is - which is a
	//              prerequisite for the service control anyway
	//   kdesu      KDE; handles X authority itself
	//   lxqt-sudo  LXQt equivalent
	//   gksudo     older GTK desktops, largely retired
	//
	struct SHelper { const char* Name; bool bNeedsEnv; };
	static const SHelper Helpers[] = {
		{ "pkexec",    true  },
		{ "kdesu",     false },
		{ "lxqt-sudo", false },
		{ "gksudo",    false },
		{ "gksu",      false },
	};

	for (const SHelper& Helper : Helpers)
	{
		const QString Path = QStandardPaths::findExecutable(Helper.Name);
		if (Path.isEmpty())
			continue;

		QStringList FullArgs;
		if (Helper.bNeedsEnv)
		{
			//
			// pkexec resets the environment for safety, which would leave a Qt
			// application with no way to reach the X server. Passing the few
			// variables it needs through env(1) is the documented idiom.
			//
			// XAUTHORITY is what actually grants access to the display; without
			// it the elevated process is refused by the X server.
			//
			FullArgs << "env" << ("DISPLAY=" + Display);

			//
			// XAUTHORITY must be passed explicitly even when it is unset in our
			// own environment. An X client with no XAUTHORITY falls back to
			// $HOME/.Xauthority - and the elevated process has root's HOME, so
			// it would look in /root and be refused by the X server.
			//
			QString XAuthority = qEnvironmentVariable("XAUTHORITY");
			if (XAuthority.isEmpty())
			{
				const QString Fallback = QDir::homePath() + "/.Xauthority";
				if (QFile::exists(Fallback))
					XAuthority = Fallback;
			}
			if (!XAuthority.isEmpty())
				FullArgs << ("XAUTHORITY=" + XAuthority);

			// Keep the platform plugin choice, so a session that forced xcb
			// does not get a different one as root.
			const QString Platform = qEnvironmentVariable("QT_QPA_PLATFORM");
			if (!Platform.isEmpty())
				FullArgs << ("QT_QPA_PLATFORM=" + Platform);
		}

		FullArgs << Program << Arguments;

		qint64 Pid = 0;
		if (QProcess::startDetached(Path, FullArgs, QString(), &Pid))
		{
			if (pPid)
				*pPid = Pid;
			return OK;
		}

		return ERR(QObject::tr("Failed to launch %1.").arg(Helper.Name));
	}

	return ERR(QObject::tr("Cannot restart elevated: no graphical privilege escalation helper was found. "
	                       "Install polkit (for pkexec), or run TaskExplorer from a terminal with 'sudo'."));
}

QString LinuxIoPrioToString(int IoPrio)
{
	if (IoPrio < 0)
		return QObject::tr("Unknown");

	const int Class = IoPrio >> IOPRIO_CLASS_SHIFT;
	const int Level = IoPrio & IOPRIO_PRIO_MASK;

	switch (Class)
	{
		case 0: return QObject::tr("None");
		case 1: return QObject::tr("Realtime (%1)").arg(Level);
		case 2: return QObject::tr("Best Effort (%1)").arg(Level);
		case 3: return QObject::tr("Idle");
	}
	return QObject::tr("Unknown");
}
