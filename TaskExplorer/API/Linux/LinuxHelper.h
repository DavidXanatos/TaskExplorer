#pragma once

#include <qobject.h>
#include "ProcFs.h"
#include "../../../MiscHelpers/Common/FlexError.h"

//
// Small shared helpers for the Linux backend: turning errno into the STATUS
// type the rest of TaskExplorer speaks, and the handful of string mappings
// that more than one Linux class needs.
//

// Builds a STATUS carrying strerror(err) as its text. Pass the errno value
// captured immediately after the failing call.
STATUS	ErrnoToStatus(const QString& Context, int Error);

// Convenience wrapper: uses the current errno.
STATUS	ErrnoToStatus(const QString& Context);

// /proc/<pid>/stat single-letter state -> human readable ("Running",
// "Sleeping", "Zombie", ...).
QString	LinuxStateToString(char State);

// Scheduling policy (SCHED_OTHER, SCHED_FIFO, ...) -> name.
QString	LinuxSchedPolicyToString(int Policy);

// Maps a nice value (-20..19) onto a coarse priority name so the shared GUI,
// which was written against Windows priority classes, has something to show.
QString	LinuxNiceToPriorityString(int Nice);

// ioprio_get/ioprio_set are not exposed by glibc; these wrap the raw syscalls.
// Returns -1 and sets errno on failure.
int	LinuxGetIoPrio(quint64 Pid);
int	LinuxSetIoPrio(quint64 Pid, int Priority);

// Decodes the class/level packed into an ioprio value into a name.
QString	LinuxIoPrioToString(int IoPrio);

// Packs an I/O scheduling class (0 none, 1 realtime, 2 best effort, 3 idle)
// and a level of 0..7 into the value ioprio_set expects.
int	LinuxMakeIoPrio(int Class, int Level);

//
// CPU affinity as a bitmask, one bit per logical cpu.
//
// The mask type here is 64 bits, which is what the shared CAbstractTask
// interface uses, so machines with more than 64 cpus are reported truncated.
// sched_getaffinity itself has no such limit.
//
quint64	LinuxGetAffinity(quint64 Pid, bool* pTruncated = nullptr);
bool	LinuxSetAffinity(quint64 Pid, quint64 Mask);

//
// ---- privileged reads, by way of TaskHelper ----
//
// Ubuntu's default kernel.yama.ptrace_scope=1 makes several /proc entries
// unreadable for processes TaskExplorer did not start - the I/O counters, the
// command line, the environment, the open descriptors, and the address space
// itself. Running the whole GUI as root to see them is a large hammer; these
// route the request through an elevated TaskHelper instead, so only the small
// Qt-free worker is privileged.
//
// All of them are no-ops that return empty when the helper cannot be started or
// the user declines the authentication, so callers can simply fall back to
// whatever they could read directly.
//
// The first call spawns the helper, which involves one authentication prompt;
// after that the same helper answers until it times out. See
// CTaskService::RunWorker.
//

// Whether a privileged helper is worth asking at all: false when we already have
// the access ourselves, in which case direct reads are cheaper.
bool	LinuxHelperNeeded();

//
// Reads one whitelisted /proc/<pid>/<leaf> file. Batched form below.
//
QByteArray	LinuxHelperReadProcFile(quint64 Pid, const QString& Leaf, bool bMayStart = true);

//
// The same for many processes at once, returning pid -> leaf -> contents.
//
// Batched because the I/O counters are wanted for every process on every
// refresh; one request per process per second would be hundreds of round trips.
//
QMap<quint64, QMap<QString, QByteArray>>
		LinuxHelperReadProcFiles(const QList<quint64>& Pids, const QStringList& Leaves, bool bMayStart = true);

QString		LinuxHelperReadProcLink(quint64 Pid, const QString& Leaf, bool bMayStart = true);

// One entry per open descriptor: Fd, Target, and the raw fdinfo text.
QList<QMap<QString, QVariant>>
		LinuxHelperListFds(quint64 Pid);

QByteArray	LinuxHelperReadMemory(quint64 Pid, quint64 Address, quint64 Size);

//
// ---- core dump sessions ----
//
// A dump has to hold every thread of the target stopped while its memory is
// read, or the snapshot is internally inconsistent. The stop therefore has to
// outlive a single request, which makes a dump a session rather than a call:
//
//     LinuxHelperDumpAttach(Pid, &Info)   stops the threads, returns the state
//     LinuxHelperReadMemory(...)          as many times as needed
//     LinuxHelperDumpDetach(Pid)          lets the target run again
//
// Every path out of the dump must reach DumpDetach, including cancellation and
// failure. If it does not, the helper still releases the target when it exits -
// but that can be an idle timeout away, so it is a backstop, not the plan.
//
struct SHelperDumpThread
{
	quint64		Tid = 0;
	bool		Attached = false;
	QByteArray	Regs;		// elf_gregset_t, empty if unavailable
	QByteArray	FpRegs;		// elf_fpregset_t, empty if unavailable
};

struct SHelperDumpInfo
{
	bool		Valid = false;
	QList<SHelperDumpThread>	Threads;
	QByteArray	Maps;		// /proc/<pid>/maps, read after the stop
	QByteArray	Auxv;		// /proc/<pid>/auxv
	int			AttachErrno = 0;	// why the stop failed, 0 if it did not
};

bool	LinuxHelperDumpAttach(quint64 Pid, SHelperDumpInfo* pInfo);
void	LinuxHelperDumpDetach(quint64 Pid);

//
// Names the container or sandbox a process is running in, or an empty string
// when it is running plainly on the host.
//
// There is no single kernel notion of "container": what exists is a set of
// namespaces plus, by convention, a recognisable cgroup path. So this checks
// the conventions of the common runtimes first - docker, podman, LXC,
// systemd-nspawn, snap, flatpak - and only falls back to naming the namespaces
// that differ from pid 1's when none of them match.
//
// Pass the values already collected for the process so this costs no extra
// reads; only the comparison against pid 1 is done here, and that is cached.
//
QString	LinuxDescribeContainer(quint64 Pid, const ProcFs::SNamespaces& Namespaces,
                               const QString& CGroupPath, const QString& Confinement);

//
// Runs an interactive command in whichever terminal emulator the desktop
// provides, since a program like gdb is useless without one.
//
// The Debian "x-terminal-emulator" alternative is preferred because it is
// whatever the user configured; the named fallbacks cover the common desktops.
// Returns an error listing what was looked for when none is installed.
//
STATUS	LinuxRunInTerminal(const QString& Program, const QStringList& Arguments);

//
// Renders an address inside a process as "module+offset", the way a debugger
// would - "libc.so.6+0x9cae2".
//
// Falls back to a bare "0x..." when the address is in anonymous memory (a JIT
// buffer, or a thread stack), and returns an empty string when the process's
// mappings cannot be read at all.
//
// Resolution needs /proc/<pid>/maps, so consecutive calls for the same process
// share one read through a short lived cache - initialising the threads of a
// process would otherwise re-read a mapping table that can be hundreds of
// kilobytes, once per thread.
//
// pOffset and pModulePath, when given, receive the offset into the mapped file
// and its full path - useful for feeding addr2line, and for labelling a frame
// whose symbol could not be resolved.
QString	LinuxResolveAddress(quint64 Pid, quint64 Address, quint64* pOffset = nullptr, QString* pModulePath = nullptr);

//
// Human readable name for an executable, taken from the freedesktop .desktop
// entry that launches it - "/usr/lib/firefox/firefox" gives "Firefox Web
// Browser".
//
// This is the closest Linux equivalent of the FileDescription string the
// Windows build reads out of VERSIONINFO, and it is what fills the Description
// column of the process tree. Returns an empty string for anything with no
// desktop entry, which is most daemons and helper processes - they are
// deliberately left blank rather than described by guesswork.
//
// The index over the applications directories is built once, on first use, and
// is safe to call from the worker threads that refresh the process list.
//
QString	LinuxDescribeExecutable(const QString& Path);

//
// Relaunches a program with root privileges through whichever graphical
// privilege-escalation helper the desktop provides, and reports the new
// process id in pPid.
//
// pkexec is preferred: it is part of polkit, so the same authentication agent
// that authorises service control handles the prompt. Because pkexec
// deliberately does not forward the environment, the GUI variables a Qt
// application needs are passed explicitly.
//
// Returns an error describing what was missing when no helper is available, or
// when the session cannot support a root GUI process at all (Wayland).
//
STATUS	LinuxRunElevated(const QString& Program, const QStringList& Arguments, qint64* pPid = nullptr);
