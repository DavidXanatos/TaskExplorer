//
// TaskHelper, Linux edition.
//
// A small, Qt-free privileged worker. TaskExplorer spawns it on demand - as root
// when a request needs privileges it does not have - and talks to it over a unix
// domain socket using the same length-prefixed CVariant packets the Windows
// helper speaks. The GUI side of that conversation is CTaskService, which uses
// QLocalSocket; on Linux that is a unix socket, so the transport needed no
// porting at all.
//
// Why a separate process rather than doing the work in the GUI:
//
//   - Privilege. The interesting operations (ptrace, reading another user's
//     /proc entries, writing a core file) need root. Elevating a whole Qt GUI to
//     get them is a much larger attack surface than elevating this.
//
//   - Isolation. Stack unwinding parses DWARF out of arbitrary, sometimes
//     corrupt binaries. A crash or a hang here costs a helper that the GUI
//     simply respawns; in-process it would take the window with it. This is not
//     hypothetical - eu-stack was observed to hang outright on a library
//     stripped of every symbol table.
//
//   - Lifetime. A ptrace attachment blocks every other debugger on the system
//     while it is held. A short-lived helper bounds that window tightly.
//
// Deliberately no Qt: the helper stays small, has no GUI dependencies, and can
// run in contexts where a Qt runtime may not be usable.
//

#include "stdafx.h"

#include "../TaskExplorer/Common/Variant.h"
#include "../TaskExplorer/Common/Buffer.h"

#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <dirent.h>
#include <time.h>
#include <unistd.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/procfs.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <elf.h>
#include <vector>

// The unwinder. See TraceStack() for why this is linked rather than shelled out to.
#include <elfutils/libdwfl.h>
#include <dwarf.h>

static bool g_Running = true;

//
// Asks the main loop to wind down. Only sets the flag - the cleanup happens on
// the way out of main(), where it is safe to do more than a signal handler may.
//
static void OnTerminate(int)
{
	g_Running = false;
}

//
// Idle timeout. The helper exits on its own once the GUI stops talking to it, so
// a crashed or killed TaskExplorer cannot leave a privileged process behind.
//
static int g_TimeoutMs = 10000;

static uint64 NowMs()
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
}

// ---- wire protocol ----

//
// Reads exactly Length bytes unless the peer goes away.
//
// A stream socket may return a short read for any reason, so every read has to
// loop; the length prefix in particular is only four bytes and losing part of it
// would desynchronise the whole connection.
//
static bool ReadExact(int Fd, void* pBuffer, size_t Length, int TimeoutMs)
{
	char* p = (char*)pBuffer;
	size_t Done = 0;

	while (Done < Length)
	{
		struct pollfd Poll = { Fd, POLLIN, 0 };
		const int Ready = poll(&Poll, 1, TimeoutMs);
		if (Ready <= 0)
			return false;	// timed out, or the socket failed

		const ssize_t Read = recv(Fd, p + Done, Length - Done, 0);
		if (Read == 0)
			return false;	// peer closed
		if (Read < 0)
		{
			if (errno == EINTR)
				continue;
			return false;
		}

		Done += (size_t)Read;
	}

	return true;
}

static bool WriteExact(int Fd, const void* pBuffer, size_t Length)
{
	const char* p = (const char*)pBuffer;
	size_t Done = 0;

	while (Done < Length)
	{
		// MSG_NOSIGNAL: a client that has gone away must yield EPIPE rather than
		// killing the helper with SIGPIPE.
		const ssize_t Written = send(Fd, p + Done, Length - Done, MSG_NOSIGNAL);
		if (Written <= 0)
		{
			if (Written < 0 && errno == EINTR)
				continue;
			return false;
		}

		Done += (size_t)Written;
	}

	return true;
}

//
// The framing, matching the Windows helper and CTaskService exactly: a native
// endian 32 bit length followed by that many bytes of serialised variant.
//
static bool RecvVariant(int Fd, CVariant& Variant, int TimeoutMs)
{
	uint32 Length = 0;
	if (!ReadExact(Fd, &Length, sizeof(Length), TimeoutMs))
		return false;

	// Same sanity bound the Windows side applies; a corrupt or hostile length
	// must not become a 4 GB allocation.
	if (Length == 0 || Length > 100 * 1024 * 1024)
		return false;

	std::vector<char> Data(Length);
	if (!ReadExact(Fd, Data.data(), Length, TimeoutMs))
		return false;

	CBuffer Buffer(Data.data(), Length, true);	// true = wrap, do not copy
	Variant.FromPacket(&Buffer);

	return true;
}

static bool SendVariant(int Fd, const CVariant& Variant)
{
	CBuffer Buffer;
	Variant.ToPacket(&Buffer);

	const uint32 Length = (uint32)Buffer.GetSize();
	if (!WriteExact(Fd, &Length, sizeof(Length)))
		return false;

	return WriteExact(Fd, Buffer.GetBuffer(), Length);
}

// ---- stack unwinding ----

//
// Unwinding is done here rather than by shelling out to eu-stack, which is what
// TaskExplorer used to do. eu-stack is a thin front end over this same libdwfl,
// so nothing is lost, and three things are gained:
//
//   - It can unwind ONE thread. eu-stack has no per-thread option, so the GUI had
//     to unwind every thread of the process and discard all but one: measured at
//     128 ms and 1033 frames for a 129-thread process, to keep about ten.
//   - Structured results. No text to parse, and no parsing bugs.
//   - Symbols and source lines in a single pass, instead of eu-stack followed by
//     eu-addr2line.
//
// libdw is a safe dependency for this: it has kept SONAME libdw.so.1 with full
// symbol versioning since 2007, and of its 85 dwfl_ entry points only three ever
// changed ABI - each keeping the old version alongside the new.
//

struct SFrameCollector
{
	CVariant*	pFrames = NULL;
	int			Count = 0;
	int			MaxFrames = 64;
};

static int FrameCallback(Dwfl_Frame* pFrame, void* pArg)
{
	SFrameCollector* pCollector = (SFrameCollector*)pArg;
	if (pCollector->Count >= pCollector->MaxFrames)
		return DWARF_CB_ABORT;

	Dwarf_Addr Pc = 0;
	bool bActivation = false;
	if (!dwfl_frame_pc(pFrame, &Pc, &bActivation))
		return DWARF_CB_ABORT;

	//
	// For anything but the innermost frame the program counter is a *return*
	// address, which can land on the instruction after a call - and if that call
	// was the last instruction of a function, on the following function
	// entirely. Stepping back one byte before looking the address up is what
	// keeps such a frame attributed to the caller rather than to its neighbour.
	//
	// The address reported to the user stays the real one.
	//
	const Dwarf_Addr LookupPc = bActivation ? Pc : Pc - 1;

	CVariant Frame;
	Frame.BeginMap();
	Frame.Write("Address", (uint64)Pc);

	Dwfl* pDwfl = dwfl_thread_dwfl(dwfl_frame_thread(pFrame));
	Dwfl_Module* pModule = pDwfl ? dwfl_addrmodule(pDwfl, LookupPc) : NULL;

	if (pModule)
	{
		//
		// The module's file name, and the offset of this address within it -
		// which is what "eu-addr2line -e <module> <offset>" takes, so a frame
		// stays actionable even when no symbol could be found.
		//
		Dwarf_Addr ModuleStart = 0;
		const char* pModuleName = dwfl_module_info(pModule, NULL, &ModuleStart, NULL, NULL, NULL, NULL, NULL);
		if (pModuleName)
			Frame.Write("Module", pModuleName);
		Frame.Write("Offset", (uint64)(LookupPc - ModuleStart));

		//
		// dwfl_module_addrinfo rather than dwfl_module_addrname: it also reports
		// how far into the symbol the address is, which is the only way to tell a
		// real match from a nearest-preceding-symbol guess in a stripped library.
		//
		GElf_Off SymbolOffset = 0;
		GElf_Sym Symbol;
		const char* pName = dwfl_module_addrinfo(pModule, LookupPc, &SymbolOffset, &Symbol, NULL, NULL, NULL);
		if (pName)
		{
			Frame.Write("Symbol", pName);
			Frame.Write("SymbolOffset", (uint64)SymbolOffset);
		}

		// Source location, when the module carries (or points at) debug info.
		Dwfl_Line* pLine = dwfl_module_getsrc(pModule, LookupPc);
		if (pLine)
		{
			int LineNumber = 0;
			int Column = 0;
			const char* pFile = dwfl_lineinfo(pLine, NULL, &LineNumber, &Column, NULL, NULL);
			if (pFile)
			{
				Frame.Write("File", pFile);
				Frame.Write("Line", (uint32)LineNumber);
				Frame.Write("Column", (uint32)Column);
			}
		}
	}

	Frame.Finish();

	pCollector->pFrames->WriteVariant(Frame);
	pCollector->Count++;

	return DWARF_CB_OK;
}

static CVariant TraceStack(uint64 Pid, uint64 Tid, int MaxFrames)
{
	CVariant Result;
	Result.BeginMap();

	//
	// Zero initialised, then only the fields we mean to provide are set. libdwfl
	// treats a null callback as "not supplied", so this also means a future
	// elfutils that appends a field sees a null rather than garbage.
	//
	static char* pDebugInfoPath = NULL;
	Dwfl_Callbacks Callbacks;
	memset(&Callbacks, 0, sizeof(Callbacks));
	Callbacks.find_elf = dwfl_linux_proc_find_elf;
	Callbacks.find_debuginfo = dwfl_standard_find_debuginfo;
	Callbacks.debuginfo_path = &pDebugInfoPath;

	Dwfl* pDwfl = dwfl_begin(&Callbacks);
	if (!pDwfl)
	{
		Result.Write("Error", "libdwfl could not be initialised");
		Result.Finish();
		return Result;
	}

	int Error = dwfl_linux_proc_report(pDwfl, (pid_t)Pid);
	if (Error == 0)
		Error = dwfl_report_end(pDwfl, NULL, NULL) < 0 ? -1 : 0;

	if (Error != 0)
	{
		dwfl_end(pDwfl);
		Result.Write("Error", "the process could not be examined");
		Result.Finish();
		return Result;
	}

	//
	// false: let libdwfl do the ptrace attach and stop itself, and detach when
	// unwinding finishes. Holding the attachment for no longer than the unwind
	// matters - while it is held, no other debugger on the system can attach.
	//
	Error = dwfl_linux_proc_attach(pDwfl, (pid_t)Pid, false);
	if (Error != 0)
	{
		dwfl_end(pDwfl);
		//
		// The usual cause is ptrace being refused. Reported as a code so the GUI
		// can phrase it, since only it knows whether it is running elevated.
		//
		Result.Write("Error", "ptrace");
		Result.Write("Errno", (uint32)(Error > 0 ? Error : EPERM));
		Result.Finish();
		return Result;
	}

	CVariant Frames;
	Frames.BeginList();

	SFrameCollector Collector;
	Collector.pFrames = &Frames;
	Collector.MaxFrames = MaxFrames > 0 ? MaxFrames : 64;

	// The whole point: one thread, not the entire process.
	const int Walked = dwfl_getthread_frames(pDwfl, (pid_t)Tid, FrameCallback, &Collector);

	Frames.Finish();

	//
	// Report libdwfl's own diagnosis when nothing came back, rather than an empty
	// list the GUI cannot explain.
	//
	if (Collector.Count == 0)
	{
		const char* pMessage = dwfl_errmsg(-1);
		Result.Write("Error", pMessage ? pMessage : "no frames were unwound");
		Result.Write("Walked", (sint32)Walked);
	}
	dwfl_end(pDwfl);

	Result.WriteVariant("Frames", Frames);
	Result.Finish();
	return Result;
}

// ---- privileged /proc access ----

//
// What the helper is willing to read out of /proc, by exact leaf name.
//
// This is a whitelist rather than a path parameter on purpose. The helper often
// runs as root, so "read me this file" would make it a general-purpose
// privileged file reader for anyone who can reach the socket - which is a much
// larger thing to have on the system than a task manager needs. Every entry
// here is something TaskExplorer already displays.
//
static bool IsReadableLeaf(const std::string& Leaf)
{
	static const char* Allowed[] = {
		"io",			// per-process read/write counters
		"cmdline",
		"environ",
		"status",
		"stat",
		"statm",
		"smaps_rollup",
		"limits",
		"wchan",
		"oom_score",
		"attr/current",	// LSM confinement label
	};

	for (size_t i = 0; i < sizeof(Allowed) / sizeof(Allowed[0]); i++)
	{
		if (Leaf == Allowed[i])
			return true;
	}

	return false;
}

//
// Likewise for the symlinks. "fd/<n>" is permitted with a numeric suffix only,
// so the leaf cannot be used to walk out of /proc/<pid>.
//
static bool IsReadableLink(const std::string& Leaf)
{
	if (Leaf == "exe" || Leaf == "cwd" || Leaf == "root")
		return true;

	if (Leaf.compare(0, 3, "fd/") == 0 && Leaf.size() > 3)
	{
		for (size_t i = 3; i < Leaf.size(); i++)
		{
			if (Leaf[i] < '0' || Leaf[i] > '9')
				return false;
		}
		return true;
	}

	return false;
}

//
// Parses an unsigned decimal number.
//
// Deliberately not strtoul/strtoull. Since glibc 2.38 those are redirected to
// __isoc23_strtoul in C++ mode - unconditionally, no -std flag avoids it - which
// would raise this binary's glibc floor to 2.38 and stop it running on anything
// older. Everything parsed here is a small decimal integer from /proc or from
// our own command line, so the redirect buys nothing and costs portability.
//
// Returns Fallback when there is no digit at all; trailing junk simply ends the
// number, which is how the callers already treated it.
//
static uint64 ParseUInt(const char* pText, uint64 Fallback = 0)
{
	if (!pText)
		return Fallback;

	while (*pText == ' ' || *pText == '\t')
		pText++;

	if (*pText < '0' || *pText > '9')
		return Fallback;

	uint64 Value = 0;
	for (; *pText >= '0' && *pText <= '9'; pText++)
	{
		const uint64 Digit = (uint64)(*pText - '0');

		// Saturate rather than wrap; a pid or fd never comes near this.
		if (Value > (UINT64_MAX - Digit) / 10)
			return UINT64_MAX;

		Value = Value * 10 + Digit;
	}

	return Value;
}

static std::string ProcPath(uint64 Pid, const std::string& Leaf)
{
	char Buffer[64];
	snprintf(Buffer, sizeof(Buffer), "/proc/%llu/", (unsigned long long)Pid);
	return std::string(Buffer) + Leaf;
}

static bool ReadWholeFile(const std::string& Path, std::vector<char>& Data)
{
	const int Fd = open(Path.c_str(), O_RDONLY | O_CLOEXEC);
	if (Fd < 0)
		return false;

	//
	// Files under /proc report a size of zero, so this reads until EOF rather
	// than trusting stat. Bounded, because a few of them (environ on a process
	// with a pathological environment) can be large.
	//
	const size_t MaxSize = 8 * 1024 * 1024;
	char Chunk[8192];

	for (;;)
	{
		const ssize_t Read = read(Fd, Chunk, sizeof(Chunk));
		if (Read < 0)
		{
			if (errno == EINTR)
				continue;
			close(Fd);
			return false;
		}
		if (Read == 0)
			break;

		if (Data.size() + (size_t)Read > MaxSize)
			break;

		Data.insert(Data.end(), Chunk, Chunk + Read);
	}

	close(Fd);
	return true;
}

//
// Reads one or more whitelisted files for a list of processes in a single
// request.
//
// Batched because of where this is used: the per-process I/O counters are wanted
// for every process on every refresh, and a round trip each would mean hundreds
// of them per second. One request per refresh instead.
//
static CVariant ReadProcFiles(const CVariant& Parameters)
{
	CVariant Result;
	Result.BeginMap();

	const CVariant Pids = Parameters.Find("Pids");
	const CVariant Leaves = Parameters.Find("Leaves");

	std::vector<std::string> LeafNames;
	Leaves.ReadRawList([&LeafNames](const CVariant& Data) {
		LeafNames.push_back(Data.ToString());
	});

	std::vector<uint64> PidList;
	Pids.ReadRawList([&PidList](const CVariant& Data) {
		PidList.push_back(Data.To<uint64>());
	});

	for (size_t i = 0; i < PidList.size(); i++)
	{
		CVariant Files;
		Files.BeginMap();

		bool bAny = false;
		for (size_t j = 0; j < LeafNames.size(); j++)
		{
			if (!IsReadableLeaf(LeafNames[j]))
				continue;

			std::vector<char> Data;
			if (!ReadWholeFile(ProcPath(PidList[i], LeafNames[j]), Data))
				continue;	// the process exited, or even root cannot read it

			Files.WriteVariant(LeafNames[j].c_str(),
				CVariant((const byte*)Data.data(), Data.size(), VAR_TYPE_BYTES));
			bAny = true;
		}

		Files.Finish();

		if (bAny)
		{
			char Key[32];
			snprintf(Key, sizeof(Key), "%llu", (unsigned long long)PidList[i]);
			Result.WriteVariant(Key, Files);
		}
	}

	Result.Finish();
	return Result;
}

static CVariant ReadProcLink(const CVariant& Parameters)
{
	const uint64 Pid = Parameters.Find("ProcessId").To<uint64>();
	const std::string Leaf = Parameters.Find("Leaf").ToString();

	if (!Pid || !IsReadableLink(Leaf))
		return CVariant("Invalid Parameters");

	//
	// /proc symlinks report a size of zero, so the buffer has to be grown
	// blindly rather than sized from stat.
	//
	std::vector<char> Buffer(1024);
	for (;;)
	{
		const ssize_t Length = readlink(ProcPath(Pid, Leaf).c_str(), Buffer.data(), Buffer.size());
		if (Length < 0)
			return CVariant();	// gone, or not permitted even here

		if ((size_t)Length < Buffer.size())
			return CVariant(std::string(Buffer.data(), (size_t)Length));

		if (Buffer.size() > 64 * 1024)
			return CVariant(std::string(Buffer.data(), (size_t)Length));

		Buffer.resize(Buffer.size() * 2);
	}
}

//
// The open file descriptors of a process: the number and what it points at.
//
static CVariant ListProcFds(const CVariant& Parameters)
{
	const uint64 Pid = Parameters.Find("ProcessId").To<uint64>();
	if (!Pid)
		return CVariant("Invalid Parameters");

	CVariant Result;
	Result.BeginList();

	const std::string DirPath = ProcPath(Pid, "fd");
	DIR* pDir = opendir(DirPath.c_str());
	if (pDir)
	{
		while (struct dirent* pEntry = readdir(pDir))
		{
			if (pEntry->d_name[0] < '0' || pEntry->d_name[0] > '9')
				continue;

			char LinkTarget[4096];
			const std::string LinkPath = DirPath + "/" + pEntry->d_name;
			const ssize_t Length = readlink(LinkPath.c_str(), LinkTarget, sizeof(LinkTarget) - 1);

			CVariant Entry;
			Entry.BeginMap();
			Entry.Write("Fd", ParseUInt(pEntry->d_name));
			if (Length > 0)
				Entry.Write("Target", std::string(LinkTarget, (size_t)Length).c_str());

			// fdinfo carries the open flags and file position.
			std::vector<char> Info;
			if (ReadWholeFile(ProcPath(Pid, std::string("fdinfo/") + pEntry->d_name), Info))
				Entry.WriteVariant("Info", CVariant((const byte*)Info.data(), Info.size(), VAR_TYPE_BYTES));

			Entry.Finish();
			Result.WriteVariant(Entry);
		}
		closedir(pDir);
	}

	Result.Finish();
	return Result;
}

//
// ---- core dump sessions ----
//
// Writing a useful core needs every thread of the target stopped for the whole
// time its memory is being read, otherwise the snapshot is internally
// inconsistent - pointers read from one region no longer describe the other.
//
// That means the ptrace attach has to outlive a single request, so a dump is a
// session: DumpAttach stops the threads and hands back everything that needs
// privilege and is only valid while stopped (register sets, the memory map, the
// auxiliary vector), the caller then reads memory with ReadProcMemory, and
// DumpDetach lets the target run again.
//
// Only one session exists at a time. A second DumpAttach releases the first
// rather than refusing, because a caller that crashed mid-dump must not be able
// to leave a process stopped forever - and for the same reason the threads are
// released on every exit path, see DumpDetachAll().
//
static std::vector<pid_t> g_DumpAttached;

static void DumpDetachAll()
{
	for (size_t i = 0; i < g_DumpAttached.size(); i++)
		ptrace(PTRACE_DETACH, g_DumpAttached[i], NULL, NULL);

	g_DumpAttached.clear();
}

static CVariant DumpAttach(const CVariant& Parameters)
{
	const uint64 Pid = Parameters.Find("ProcessId").To<uint64>();
	if (!Pid)
		return CVariant("Invalid Parameters");

	// Whatever was attached before is no longer being dumped.
	DumpDetachAll();

	CVariant Result;
	Result.BeginMap();

	CVariant Threads;
	Threads.BeginList();

	int AttachErrno = 0;

	DIR* pDir = opendir(ProcPath(Pid, "task").c_str());
	if (pDir)
	{
		while (struct dirent* pEntry = readdir(pDir))
		{
			if (pEntry->d_name[0] < '0' || pEntry->d_name[0] > '9')
				continue;

			const pid_t Tid = (pid_t)ParseUInt(pEntry->d_name);

			//
			// PTRACE_SEIZE rather than PTRACE_ATTACH: seizing does not itself
			// stop the thread, so the stop is asked for explicitly and no queued
			// SIGSTOP is left behind to surprise the target after detaching.
			//
			bool bAttached = false;

			//
			// elf_gregset_t / elf_fpregset_t, not struct user_regs_struct and
			// struct user_fpregs_struct.
			//
			// user_fpregs_struct is an x86 name; on aarch64 glibc calls the same
			// thing user_fpsimd_struct, so naming it directly does not compile
			// there. The elf_* typedefs in <sys/procfs.h> exist on every
			// architecture and resolve to whatever is right for it - and on x86_64
			// they are the very same structures, 216 and 512 bytes, so the bytes
			// on the wire do not change.
			//
			// This is also what the receiving end uses (SThreadState in
			// LinuxDumper.cpp), which is what makes its size check meaningful.
			//
			elf_gregset_t Regs;
			elf_fpregset_t FpRegs;
			bool bHaveRegs = false, bHaveFpRegs = false;

			memset(&Regs, 0, sizeof(Regs));
			memset(&FpRegs, 0, sizeof(FpRegs));

			if (ptrace(PTRACE_SEIZE, Tid, NULL, NULL) == 0)
			{
				if (ptrace(PTRACE_INTERRUPT, Tid, NULL, NULL) == 0)
				{
					int Status = 0;
					if (waitpid(Tid, &Status, __WALL) >= 0)
					{
						bAttached = true;
						g_DumpAttached.push_back(Tid);

						struct iovec Iov;
						Iov.iov_base = &Regs;
						Iov.iov_len = sizeof(Regs);
						bHaveRegs = ptrace(PTRACE_GETREGSET, Tid, (void*)(uintptr_t)NT_PRSTATUS, &Iov) == 0;

						Iov.iov_base = &FpRegs;
						Iov.iov_len = sizeof(FpRegs);
						bHaveFpRegs = ptrace(PTRACE_GETREGSET, Tid, (void*)(uintptr_t)NT_PRFPREG, &Iov) == 0;
					}
				}

				if (!bAttached)
					ptrace(PTRACE_DETACH, Tid, NULL, NULL);
			}

			// A thread that simply exited between the readdir and the seize is
			// not a permission problem and must not be reported as one.
			if (!bAttached && errno != ESRCH)
				AttachErrno = errno;

			CVariant Thread;
			Thread.BeginMap();
			Thread.Write("Tid", (uint64)Tid);
			Thread.Write("Attached", (bool)bAttached);
			if (bHaveRegs)
				Thread.WriteVariant("Regs", CVariant((const byte*)&Regs, sizeof(Regs), VAR_TYPE_BYTES));
			if (bHaveFpRegs)
				Thread.WriteVariant("FpRegs", CVariant((const byte*)&FpRegs, sizeof(FpRegs), VAR_TYPE_BYTES));
			Thread.Finish();

			Threads.WriteVariant(Thread);
		}
		closedir(pDir);
	}

	Threads.Finish();
	Result.WriteVariant("Threads", Threads);

	//
	// Read after the stop, so the layout cannot shift underneath the caller
	// while it is deciding what to write.
	//
	std::vector<char> Maps;
	if (ReadWholeFile(ProcPath(Pid, "maps"), Maps) && !Maps.empty())
		Result.WriteVariant("Maps", CVariant((const byte*)Maps.data(), Maps.size(), VAR_TYPE_BYTES));

	std::vector<char> Auxv;
	if (ReadWholeFile(ProcPath(Pid, "auxv"), Auxv) && !Auxv.empty())
		Result.WriteVariant("Auxv", CVariant((const byte*)Auxv.data(), Auxv.size(), VAR_TYPE_BYTES));

	Result.Write("AttachErrno", (uint32)AttachErrno);

	Result.Finish();
	return Result;
}

static CVariant DumpDetach(const CVariant& Parameters)
{
	DumpDetachAll();
	return CVariant((bool)true);
}

//
// A window of another process's address space, for the memory view, the hex
// editor and the string search.
//
static CVariant ReadProcMemory(const CVariant& Parameters)
{
	const uint64 Pid = Parameters.Find("ProcessId").To<uint64>();
	const uint64 Address = Parameters.Find("Address").To<uint64>();
	const uint64 Size = Parameters.Find("Size").To<uint64>();

	// Bounded so that one request cannot ask the helper to allocate arbitrarily.
	const uint64 MaxSize = 16 * 1024 * 1024;
	if (!Pid || !Size || Size > MaxSize)
		return CVariant("Invalid Parameters");

	const int Fd = open(ProcPath(Pid, "mem").c_str(), O_RDONLY | O_LARGEFILE | O_CLOEXEC);
	if (Fd < 0)
		return CVariant();

	std::vector<char> Data(Size);
	const ssize_t Read = pread(Fd, Data.data(), (size_t)Size, (off_t)Address);
	close(Fd);

	if (Read <= 0)
		return CVariant();	// unmapped, or refused

	return CVariant((const byte*)Data.data(), (size_t)Read, VAR_TYPE_BYTES);
}

// ---- command dispatch ----

//
// Requests are either a bare string command or a map of { Command, Parameters },
// which is the shape CTaskService sends and the Windows helper accepts.
//
static CVariant ProcessCommand(const CVariant& Request)
{
	std::string Command;
	CVariant Parameters;

	const uint32 Type = Request.GetType();
	if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
	{
		Command = Request.ToString();
	}
	else if (Type == VAR_TYPE_MAP)
	{
		CVariant CommandVar = Request.Find("Command");
		if (CommandVar.IsValid())
			Command = CommandVar.ToString();

		Parameters = Request.Find("Parameters");
	}
	else
	{
		return CVariant("Unknown Command");
	}

	if (Command == "Refresh")
	{
		// The liveness probe CTaskService::RunWorker uses after spawning.
		return CVariant(true);
	}

	if (Command == "Quit")
	{
		g_Running = false;
		return CVariant(true);
	}

	if (Command == "GetProcessId")
	{
		return CVariant((uint64)getpid());
	}

	if (Command == "ReadProcFiles")
		return ReadProcFiles(Parameters);

	if (Command == "ReadProcLink")
		return ReadProcLink(Parameters);

	if (Command == "ListProcFds")
		return ListProcFds(Parameters);

	if (Command == "ReadProcMemory")
		return ReadProcMemory(Parameters);

	if (Command == "DumpAttach")
		return DumpAttach(Parameters);

	if (Command == "DumpDetach")
		return DumpDetach(Parameters);

	if (Command == "TraceStack")
	{
		const uint64 Pid = Parameters.Find("ProcessId").To<uint64>();
		const uint64 Tid = Parameters.Find("ThreadId").To<uint64>();
		const uint32 MaxFrames = Parameters.Find("MaxFrames").To<uint32>();
		if (!Pid || !Tid)
			return CVariant("Invalid Parameters");
		return TraceStack(Pid, Tid, (int)MaxFrames);
	}

	if (Command == "GetHelperInfo")
	{
		// Lets the GUI report what it is talking to, and whether the helper
		// actually ended up privileged - asking for root is not the same as
		// getting it.
		CVariant Info;
		Info.BeginMap();
		Info.Write("Pid", (uint64)getpid());
		Info.Write("Uid", (uint64)getuid());
		Info.Write("EffectiveUid", (uint64)geteuid());
		Info.Write("IsRoot", (bool)(geteuid() == 0));
		Info.Finish();
		return Info;
	}

	return CVariant("Unknown Command");
}

// ---- server ----

static int CreateListeningSocket(const char* pPath)
{
	//
	// The path is handed over by the GUI, which picks it in the user's runtime
	// directory. A stale socket file from a helper that was killed would
	// otherwise make bind() fail with EADDRINUSE.
	//
	unlink(pPath);

	const int Fd = socket(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0);
	if (Fd < 0)
		return -1;

	struct sockaddr_un Address;
	memset(&Address, 0, sizeof(Address));
	Address.sun_family = AF_UNIX;

	if (strlen(pPath) >= sizeof(Address.sun_path))
	{
		close(Fd);
		return -1;	// sun_path is only 108 bytes
	}
	strncpy(Address.sun_path, pPath, sizeof(Address.sun_path) - 1);

	//
	// Created private from the outset. The socket may be owned by root while the
	// GUI runs as an ordinary user, and connecting to a unix socket needs write
	// access to it - so the mode has to be set deliberately rather than left to
	// the umask, and ownership handed to whoever asked for the helper.
	//
	const mode_t OldMask = umask(0177);	// 0600
	const int Result = bind(Fd, (struct sockaddr*)&Address, sizeof(Address));
	umask(OldMask);

	if (Result < 0 || listen(Fd, 4) < 0)
	{
		close(Fd);
		unlink(pPath);
		return -1;
	}

	return Fd;
}

static void ServeConnection(int Fd)
{
	//
	// One request per connection, which is what CTaskService::SendCommand does:
	// it connects, sends, reads the reply and disconnects. Keeping the helper
	// stateless per connection means a client that dies mid-request cannot leave
	// it wedged.
	//
	CVariant Request;
	if (!RecvVariant(Fd, Request, 5000))
		return;

	const CVariant Response = ProcessCommand(Request);
	SendVariant(Fd, Response);
}

int main(int argc, char* argv[])
{
	const char* pSocketPath = NULL;
	uid_t Owner = (uid_t)-1;

	for (int i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "-wrk") == 0 && i + 1 < argc)
			pSocketPath = argv[++i];
		else if (strcmp(argv[i], "-timeout") == 0 && i + 1 < argc)
			g_TimeoutMs = atoi(argv[++i]);
		else if (strcmp(argv[i], "-owner") == 0 && i + 1 < argc)
			Owner = (uid_t)ParseUInt(argv[++i], (uint64)-1);
	}

	if (!pSocketPath)
	{
		fprintf(stderr, "TaskHelper: -wrk <socket path> is required\n");
		return 1;
	}

	// A client vanishing mid-write must not take the helper with it.
	signal(SIGPIPE, SIG_IGN);

	//
	// Terminate through the loop rather than immediately, so that the exit path
	// below runs and any process stopped for a core dump is released. Killing the
	// helper with SIGKILL cannot be caught, but the kernel detaches a dead
	// tracer's tracees and resumes them, so that case is covered too.
	//
	signal(SIGTERM, OnTerminate);
	signal(SIGINT, OnTerminate);
	signal(SIGHUP, OnTerminate);

	//
	// Disable debuginfod before touching libdwfl.
	//
	// dwfl_standard_find_debuginfo consults $DEBUGINFOD_URLS, which on Ubuntu is
	// set system-wide from /etc/debuginfod/elfutils.urls, and will try to fetch
	// debug information over the network for any module that has none locally.
	// There is no timeout on that lookup, so displaying the stack of a process
	// built without -g blocks indefinitely - measured here as a hang rather than
	// a delay.
	//
	// Three reasons not to want it regardless of the hang: a task manager should
	// not make network requests to draw a list; the request tells a third party
	// which binaries are being inspected on this machine; and the helper often
	// runs as root, where outbound network access is the last thing it needs.
	//
	// Symbols still come from anything present locally, including the compressed
	// MiniDebugInfo distributions ship in .gnu_debugdata.
	//
	unsetenv("DEBUGINFOD_URLS");

	const int ListenFd = CreateListeningSocket(pSocketPath);
	if (ListenFd < 0)
	{
		fprintf(stderr, "TaskHelper: cannot listen on %s: %s\n", pSocketPath, strerror(errno));
		return 1;
	}

	//
	// When running elevated the socket belongs to root, so it is handed to the
	// user who asked for the helper - otherwise they could not connect to the
	// thing they just started. Mode stays 0600, so only that user can.
	//
	if (Owner != (uid_t)-1)
	{
		if (chown(pSocketPath, Owner, (gid_t)-1) != 0)
			fprintf(stderr, "TaskHelper: cannot chown %s: %s\n", pSocketPath, strerror(errno));
	}

	uint64 LastActivity = NowMs();

	while (g_Running)
	{
		struct pollfd Poll = { ListenFd, POLLIN, 0 };
		const int Ready = poll(&Poll, 1, 500);

		if (Ready < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}

		if (Ready == 0)
		{
			// Nothing is talking to us; give up rather than linger as a
			// privileged process nobody is using.
			if (g_TimeoutMs > 0 && NowMs() - LastActivity > (uint64)g_TimeoutMs)
				break;
			continue;
		}

		const int Fd = accept(ListenFd, NULL, NULL);
		if (Fd < 0)
			continue;

		ServeConnection(Fd);
		close(Fd);

		LastActivity = NowMs();
	}

	//
	// Release anything stopped for a core dump. A caller that died mid-dump would
	// otherwise leave the target stopped until this process exits, and on the
	// idle-timeout path that is up to the whole timeout later.
	//
	DumpDetachAll();

	close(ListenFd);
	unlink(pSocketPath);

	return 0;
}
