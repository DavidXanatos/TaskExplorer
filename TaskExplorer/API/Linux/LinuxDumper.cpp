#include "stdafx.h"
#include "LinuxDumper.h"
#include "LinuxHelper.h"
#include "ProcFs.h"
#include "../../../MiscHelpers/Common/Settings.h"
#include "../../../MiscHelpers/Common/Common.h"

#include <QFile>
#include <QFileInfo>

#include <algorithm>

#include <elf.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/procfs.h>
#include <sys/ptrace.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

//
// Note types. Modern glibc <elf.h> has all of these, but the values are ABI
// and spelling them out keeps the file building against older headers.
//
#ifndef NT_PRSTATUS
#define NT_PRSTATUS	1
#endif
#ifndef NT_PRFPREG
#define NT_PRFPREG	2
#endif
#ifndef NT_PRPSINFO
#define NT_PRPSINFO	3
#endif
#ifndef NT_AUXV
#define NT_AUXV		6
#endif
#ifndef NT_FILE
#define NT_FILE		0x46494c45	// "FILE"
#endif

#ifndef __WALL
#define __WALL		0x40000000
#endif

//
// e_machine for the core header. Only the architectures the rest of the Linux
// backend has been built for are listed; anything else refuses up front rather
// than writing a core gdb would misparse.
//
#if defined(__x86_64__)
  #define TE_CORE_MACHINE	EM_X86_64
#elif defined(__aarch64__)
  #define TE_CORE_MACHINE	EM_AARCH64
#elif defined(__powerpc64__)
  #define TE_CORE_MACHINE	EM_PPC64
#elif defined(__riscv) && __riscv_xlen == 64
  #define TE_CORE_MACHINE	EM_RISCV
#else
  #define TE_CORE_MACHINE	0
#endif

namespace {

// Register state of one stopped thread.
struct SThreadState
{
	quint64			Tid = 0;
	elf_gregset_t	Regs;
	elf_fpregset_t	FpRegs;
	bool			Attached = false;
	bool			HaveRegs = false;
	bool			HaveFpRegs = false;
};

// One PT_LOAD to be written.
struct SDumpRegion
{
	quint64	Start = 0;
	quint64	End = 0;
	quint64	DumpBytes = 0;	// how much of it goes in the file; may be 0
	quint64	FileOffset = 0;
	quint32	Flags = 0;		// PF_R | PF_W | PF_X
	quint64	PageOffset = 0;	// mapping offset, in pages, for NT_FILE
	QString	Path;			// only set for real file mappings
};

quint64 RoundUp(quint64 Value, quint64 Alignment)
{
	return (Value + Alignment - 1) & ~(Alignment - 1);
}

//
// Appends one note. The layout is a header, the NUL terminated name padded to
// 4 bytes, then the descriptor padded to 4 bytes.
//
void AppendNote(QByteArray& Notes, const char* Name, quint32 Type, const void* Desc, quint32 DescSize)
{
	const quint32 NameSize = (quint32)strlen(Name) + 1;

	Elf64_Nhdr Nhdr;
	Nhdr.n_namesz = NameSize;
	Nhdr.n_descsz = DescSize;
	Nhdr.n_type = Type;

	Notes.append((const char*)&Nhdr, sizeof(Nhdr));
	Notes.append(Name, NameSize);
	while (Notes.size() & 3)
		Notes.append('\0');
	if (DescSize)
		Notes.append((const char*)Desc, DescSize);
	while (Notes.size() & 3)
		Notes.append('\0');
}

//
// Regions that must never be read.
//
// [vvar] and its relatives are kernel pages that are mapped into every process
// but are not backed by anything readable through /proc/<pid>/mem - reading
// them returns EIO on some kernels and, historically, could wedge the reader.
// [vsyscall] is a fixed kernel mapping at a legacy address. Device mappings
// (a GPU's BAR, for instance) are worse: reading them touches real hardware
// registers, which is exactly the kind of side effect a task manager must not
// cause. The kernel's own coredump code skips all of these too.
//
bool IsUnreadableRegion(const ProcFs::SMapEntry& Map)
{
	if (Map.Path.startsWith("[vvar") || Map.Path == "[vsyscall]")
		return true;

	// A device mapping: has a device number but no filesystem inode.
	if (Map.Path.startsWith("/dev/"))
		return true;

	return false;
}

} // namespace

CLinuxDumper::CLinuxDumper(QObject* parent)
	: CMemDumper(parent)
{
	m_ProcessId = 0;
	m_DumpType = 0;
}

CLinuxDumper::~CLinuxDumper()
{
}

STATUS CLinuxDumper::PrepareDump(const CProcessPtr& pProcess, quint32 DumpType, const QString& DumpPath)
{
	if (pProcess.isNull())
		return ERR(tr("No process selected."));

	if (TE_CORE_MACHINE == 0)
		return ERR(tr("Creating a core dump is not supported on this architecture."));

	m_ProcessId = pProcess->GetProcessId();
	m_ProcessName = pProcess->GetName();
	m_DumpPath = DumpPath;
	m_DumpType = DumpType;
	m_Canceled.storeRelease(0);

	//
	// Dumping ourselves would mean this thread ptrace-stopping the thread it is
	// running on, which deadlocks.
	//
	if (m_ProcessId == (quint64)getpid())
		return ERR(tr("TaskExplorer cannot dump its own process."));

	if (!ProcFs::FileExists(ProcFs::ProcPath(m_ProcessId)))
		return ERR(tr("The process has exited."));

	const ProcFs::SStat Stat = ProcFs::ReadStat(m_ProcessId);
	if (Stat.Valid && Stat.IsKernelThread)
		return ERR(tr("Kernel threads have no user address space to dump."));

	//
	// A 32-bit target needs an ELF32 core with 32-bit register structures.
	// Writing a 64-bit core for it would produce a file gdb silently
	// misinterprets, so refuse rather than emit something misleading.
	//
	QFile Exe(ProcFs::ProcPath(m_ProcessId, "exe"));
	if (Exe.open(QIODevice::ReadOnly))
	{
		const QByteArray Ident = Exe.read(EI_NIDENT);
		Exe.close();
		if (Ident.size() >= EI_NIDENT && memcmp(Ident.constData(), ELFMAG, SELFMAG) == 0
			&& (unsigned char)Ident[EI_CLASS] == ELFCLASS32 && sizeof(void*) == 8)
			return ERR(tr("This is a 32-bit process; only 64-bit core dumps are supported."));
	}

	// Fail here, where the GUI shows a message box, rather than inside the
	// progress dialog after the process has already been stopped.
	QFile Test(m_DumpPath);
	if (!Test.open(QIODevice::WriteOnly | QIODevice::Truncate))
		return ERR(tr("Cannot write to %1: %2").arg(m_DumpPath).arg(Test.errorString()));
	Test.close();

	return OK;
}

void CLinuxDumper::Cancel()
{
	m_Canceled.storeRelease(1);
}

void CLinuxDumper::run()
{
	const quint64 Pid = m_ProcessId;
	const long PageSize = sysconf(_SC_PAGESIZE);
	const long ClockTicks = sysconf(_SC_CLK_TCK);

	//
	// 1. Stop every thread.
	//
	// The threads are enumerated first and then seized one by one. A thread
	// created after the enumeration is missed, which is unavoidable: there is
	// no atomic "stop this thread group" operation. Its memory is still in the
	// dump, only its registers are absent.
	//
	emit ProgressMessage(tr("Suspending threads..."), 0);

	QList<SThreadState> Threads;
	const QList<quint64> Tids = ProcFs::EnumThreads(Pid);

	int AttachFailures = 0;
	int AttachErrno = 0;

	foreach(quint64 Tid, Tids)
	{
		SThreadState Thread;
		memset(&Thread.Regs, 0, sizeof(Thread.Regs));
		memset(&Thread.FpRegs, 0, sizeof(Thread.FpRegs));
		Thread.Tid = Tid;

		//
		// PTRACE_SEIZE rather than PTRACE_ATTACH: it does not itself stop the
		// thread, so the stop is requested explicitly with PTRACE_INTERRUPT and
		// no queued SIGSTOP is left behind to confuse the target once detached.
		//
		if (ptrace(PTRACE_SEIZE, (pid_t)Tid, nullptr, nullptr) == 0)
		{
			if (ptrace(PTRACE_INTERRUPT, (pid_t)Tid, nullptr, nullptr) == 0)
			{
				int Status = 0;
				if (waitpid((pid_t)Tid, &Status, __WALL) >= 0)
				{
					Thread.Attached = true;

					struct iovec Iov;
					Iov.iov_base = &Thread.Regs;
					Iov.iov_len = sizeof(Thread.Regs);
					Thread.HaveRegs = (ptrace(PTRACE_GETREGSET, (pid_t)Tid, (void*)(uintptr_t)NT_PRSTATUS, &Iov) == 0);

					Iov.iov_base = &Thread.FpRegs;
					Iov.iov_len = sizeof(Thread.FpRegs);
					Thread.HaveFpRegs = (ptrace(PTRACE_GETREGSET, (pid_t)Tid, (void*)(uintptr_t)NT_PRFPREG, &Iov) == 0);
				}
			}

			if (!Thread.Attached)
				ptrace(PTRACE_DETACH, (pid_t)Tid, nullptr, nullptr);
		}

		if (!Thread.Attached)
		{
			// A thread that simply exited between the enumeration and the seize
			// is not a permission problem and should not be reported as one.
			if (errno != ESRCH)
			{
				AttachFailures++;
				AttachErrno = errno;
			}
		}

		// The main thread goes first: gdb treats the first NT_PRSTATUS as the
		// thread that "crashed" and selects it on load.
		if (Tid == Pid)
			Threads.prepend(Thread);
		else
			Threads.append(Thread);
	}

	bool bAnyRegs = std::any_of(Threads.begin(), Threads.end(),
		[](const SThreadState& T) { return T.HaveRegs; });

	//
	// Nothing could be stopped. Under the default ptrace_scope=1 that is the
	// normal outcome for any process this one did not start, so before giving up
	// on the register state ask an elevated helper to do the stopping.
	//
	// The helper holds the threads stopped until DumpDetach, which is what makes
	// the memory read afterwards a consistent snapshot rather than a series of
	// unrelated glimpses.
	//
	bool bViaHelper = false;
	QByteArray HelperMaps;
	QByteArray HelperAuxv;

	if (!bAnyRegs && LinuxHelperNeeded() && theConf->GetBool("Options/UseTaskHelper", false))
	{
		emit ProgressMessage(tr("Asking the privileged helper to suspend threads..."), 0);

		SHelperDumpInfo Info;
		if (LinuxHelperDumpAttach(Pid, &Info) && Info.Valid)
		{
			QList<SThreadState> HelperThreads;

			foreach(const SHelperDumpThread& Source, Info.Threads)
			{
				SThreadState Thread;
				memset(&Thread.Regs, 0, sizeof(Thread.Regs));
				memset(&Thread.FpRegs, 0, sizeof(Thread.FpRegs));

				Thread.Tid = Source.Tid;
				Thread.Attached = Source.Attached;

				//
				// Copied by size rather than cast: the helper ships the raw
				// struct, and a mismatch has to truncate rather than read past
				// the end of the reply.
				//
				if (Source.Regs.size() >= (int)sizeof(Thread.Regs))
				{
					memcpy(&Thread.Regs, Source.Regs.constData(), sizeof(Thread.Regs));
					Thread.HaveRegs = true;
				}
				if (Source.FpRegs.size() >= (int)sizeof(Thread.FpRegs))
				{
					memcpy(&Thread.FpRegs, Source.FpRegs.constData(), sizeof(Thread.FpRegs));
					Thread.HaveFpRegs = true;
				}

				// gdb selects the thread of the first NT_PRSTATUS, so the main
				// thread has to lead here as it does above.
				if (Thread.Tid == Pid)
					HelperThreads.prepend(Thread);
				else
					HelperThreads.append(Thread);
			}

			const bool bHelperRegs = std::any_of(HelperThreads.begin(), HelperThreads.end(),
				[](const SThreadState& T) { return T.HaveRegs; });

			if (bHelperRegs)
			{
				bViaHelper = true;
				Threads = HelperThreads;
				HelperMaps = Info.Maps;
				HelperAuxv = Info.Auxv;
				bAnyRegs = true;
				AttachErrno = Info.AttachErrno;
			}
			else
			{
				// It could not stop it either; do not leave a session open.
				LinuxHelperDumpDetach(Pid);
				if (Info.AttachErrno)
					AttachErrno = Info.AttachErrno;
			}
		}
	}

	if (AttachFailures > 0 && !bAnyRegs)
	{
		emit StatusMessage(tr("Could not stop the process (%1). The dump will contain memory but no "
		                      "register state, so gdb will not be able to produce a backtrace. "
		                      "Enable the privileged helper or restart TaskExplorer elevated, or "
		                      "lower kernel.yama.ptrace_scope.")
			.arg(QString::fromLocal8Bit(strerror(AttachErrno))));
	}

	//
	// 2. Decide what to dump.
	//
	// The maps are read after the stop so the layout cannot shift underneath
	// the writer.
	//
	const bool bFullMemory = (m_DumpType & MiniDumpWithFullMemory) != 0;

	QList<ProcFs::SMapEntry> Maps = bViaHelper && !HelperMaps.isEmpty()
		? ProcFs::ParseMaps(HelperMaps)
		: ProcFs::ReadMaps(Pid);
	QList<SDumpRegion> Regions;

	foreach(const ProcFs::SMapEntry& Map, Maps)
	{
		SDumpRegion Region;
		Region.Start = Map.Start;
		Region.End = Map.End;
		Region.PageOffset = PageSize ? (Map.Offset / (quint64)PageSize) : 0;
		Region.Flags = (Map.Read ? PF_R : 0) | (Map.Write ? PF_W : 0) | (Map.Exec ? PF_X : 0);

		// A real file mapping, as opposed to "[heap]", "[stack]" or anonymous.
		const bool bFileBacked = (Map.Inode != 0) && Map.Path.startsWith('/');
		if (bFileBacked)
			Region.Path = Map.Path;

		const quint64 Size = Map.End - Map.Start;

		if (!Map.Read || IsUnreadableRegion(Map))
		{
			// Recorded in the core with the right address and size but no
			// contents, which is how the kernel represents them too.
			Region.DumpBytes = 0;
		}
		else if (bFullMemory)
		{
			Region.DumpBytes = Size;
		}
		else if (!bFileBacked)
		{
			// Anonymous memory: the heap, the stacks, and everything malloc'd.
			// This is where a program's actual state lives.
			Region.DumpBytes = Size;
		}
		else
		{
			//
			// A file-backed mapping whose contents can be recovered from the
			// file on disk. Only the first page is written, which is enough for
			// gdb to read the ELF header and identify the module - the same
			// trade-off as the kernel's coredump_filter "ELF headers" bit.
			//
			Region.DumpBytes = qMin<quint64>(Size, (quint64)PageSize);
		}

		Regions.append(Region);
	}

	//
	// 3. Build the notes.
	//
	QByteArray Notes;

	const ProcFs::SStat Stat = ProcFs::ReadStat(Pid);
	const QMap<QString, QString> Status = ProcFs::ReadStatus(Pid);

	// NT_PRPSINFO - who this process is.
	{
		struct elf_prpsinfo PsInfo;
		memset(&PsInfo, 0, sizeof(PsInfo));

		PsInfo.pr_state = 0;
		PsInfo.pr_sname = Stat.State ? Stat.State : '?';
		PsInfo.pr_zomb = (Stat.State == 'Z') ? 1 : 0;
		PsInfo.pr_nice = (char)Stat.Nice;
		PsInfo.pr_flag = Stat.Flags;
		PsInfo.pr_uid = Status.value("Uid").section('\t', 0, 0).trimmed().toUInt();
		PsInfo.pr_gid = Status.value("Gid").section('\t', 0, 0).trimmed().toUInt();
		PsInfo.pr_pid = (int)Pid;
		PsInfo.pr_ppid = (int)Stat.PPid;
		PsInfo.pr_pgrp = (int)Stat.PGrp;
		PsInfo.pr_sid = (int)Stat.Session;

		const QByteArray Name = Stat.Comm.toLocal8Bit();
		strncpy(PsInfo.pr_fname, Name.constData(), sizeof(PsInfo.pr_fname) - 1);

		const QByteArray Args = ProcFs::ReadNulList(ProcFs::ProcPath(Pid, "cmdline")).join(' ').toLocal8Bit();
		strncpy(PsInfo.pr_psargs, Args.constData(), sizeof(PsInfo.pr_psargs) - 1);

		AppendNote(Notes, "CORE", NT_PRPSINFO, &PsInfo, sizeof(PsInfo));
	}

	// NT_PRSTATUS + NT_FPREGSET, once per thread.
	foreach(const SThreadState& Thread, Threads)
	{
		if (!Thread.HaveRegs)
			continue;	// a thread with no registers is worse than no entry at all

		const ProcFs::SStat ThreadStat = ProcFs::ReadThreadStat(Pid, Thread.Tid);

		struct elf_prstatus PrStatus;
		memset(&PrStatus, 0, sizeof(PrStatus));

		PrStatus.pr_pid = (pid_t)Thread.Tid;
		PrStatus.pr_ppid = (pid_t)Stat.PPid;
		PrStatus.pr_pgrp = (pid_t)Stat.PGrp;
		PrStatus.pr_sid = (pid_t)Stat.Session;

		if (ClockTicks > 0)
		{
			PrStatus.pr_utime.tv_sec = ThreadStat.UTime / ClockTicks;
			PrStatus.pr_utime.tv_usec = (ThreadStat.UTime % ClockTicks) * (1000000 / ClockTicks);
			PrStatus.pr_stime.tv_sec = ThreadStat.STime / ClockTicks;
			PrStatus.pr_stime.tv_usec = (ThreadStat.STime % ClockTicks) * (1000000 / ClockTicks);
		}

		memcpy(&PrStatus.pr_reg, &Thread.Regs, qMin(sizeof(PrStatus.pr_reg), sizeof(Thread.Regs)));
		PrStatus.pr_fpvalid = Thread.HaveFpRegs ? 1 : 0;

		AppendNote(Notes, "CORE", NT_PRSTATUS, &PrStatus, sizeof(PrStatus));

		if (Thread.HaveFpRegs)
			AppendNote(Notes, "CORE", NT_PRFPREG, &Thread.FpRegs, sizeof(Thread.FpRegs));
	}

	// NT_AUXV - the auxiliary vector, which is how gdb locates the dynamic
	// loader and therefore the shared library list.
	{
		const QByteArray Auxv = bViaHelper && !HelperAuxv.isEmpty()
			? HelperAuxv
			: ProcFs::ReadFile(ProcFs::ProcPath(Pid, "auxv"));
		if (!Auxv.isEmpty())
			AppendNote(Notes, "CORE", NT_AUXV, Auxv.constData(), (quint32)Auxv.size());
	}

	//
	// NT_FILE - which file is mapped where, so gdb can find the binaries even
	// though their contents were not dumped.
	//
	// Layout: a count and a page size, then one {start, end, file offset in
	// pages} triple per mapping, then the paths as a run of NUL terminated
	// strings in the same order.
	//
	{
		QByteArray Table;
		QByteArray Paths;
		quint64 Count = 0;

		foreach(const SDumpRegion& Region, Regions)
		{
			if (Region.Path.isEmpty())
				continue;

			const quint64 Triple[3] = { Region.Start, Region.End, Region.PageOffset };
			Table.append((const char*)Triple, sizeof(Triple));

			const QByteArray Path = Region.Path.toLocal8Bit();
			Paths.append(Path);
			Paths.append('\0');

			Count++;
		}

		if (Count)
		{
			const quint64 Header[2] = { Count, (quint64)PageSize };

			QByteArray Note;
			Note.append((const char*)Header, sizeof(Header));
			Note.append(Table);
			Note.append(Paths);

			AppendNote(Notes, "CORE", NT_FILE, Note.constData(), (quint32)Note.size());
		}
	}

	//
	// 4. Lay the file out.
	//
	// Everything but the region data is at the front, then the data starts on a
	// page boundary so that each PT_LOAD's file offset and virtual address are
	// congruent modulo the page size - which is what the ELF spec requires and
	// what lets a debugger mmap the core directly.
	//
	const int PhdrCount = 1 + Regions.count();

	//
	// e_phnum is 16 bits. A process with more mappings than that is vanishingly
	// rare but the escape hatch exists: e_phnum is set to PN_XNUM and the real
	// count goes in the first section header's sh_info.
	//
	const bool bExtendedPhnum = (PhdrCount >= PN_XNUM);

	quint64 Offset = sizeof(Elf64_Ehdr);
	if (bExtendedPhnum)
		Offset += sizeof(Elf64_Shdr);
	const quint64 PhdrOffset = Offset;
	Offset += (quint64)PhdrCount * sizeof(Elf64_Phdr);

	const quint64 NoteOffset = Offset;
	Offset += Notes.size();

	Offset = RoundUp(Offset, (quint64)PageSize);

	quint64 TotalBytes = 0;
	for (int i = 0; i < Regions.count(); i++)
	{
		Regions[i].FileOffset = Offset;
		Offset += Regions[i].DumpBytes;
		TotalBytes += Regions[i].DumpBytes;
	}

	//
	// 5. Write it.
	//
	//
	// Whoever stopped the threads has to release them, and every path out of here
	// has to go through this - a target left stopped is worse than a failed dump.
	//
	auto ReleaseThreads = [&]() {
		if (bViaHelper)
		{
			LinuxHelperDumpDetach(Pid);
			return;
		}

		foreach(const SThreadState& Thread, Threads)
			if (Thread.Attached)
				ptrace(PTRACE_DETACH, (pid_t)Thread.Tid, nullptr, nullptr);
	};

	QFile Core(m_DumpPath);
	if (!Core.open(QIODevice::WriteOnly | QIODevice::Truncate))
	{
		ReleaseThreads();

		emit StatusMessage(tr("Failed to create %1: %2").arg(m_DumpPath).arg(Core.errorString()), -1);
		return;
	}

	Elf64_Ehdr Ehdr;
	memset(&Ehdr, 0, sizeof(Ehdr));
	memcpy(Ehdr.e_ident, ELFMAG, SELFMAG);
	Ehdr.e_ident[EI_CLASS] = ELFCLASS64;
	Ehdr.e_ident[EI_DATA] = (Q_BYTE_ORDER == Q_LITTLE_ENDIAN) ? ELFDATA2LSB : ELFDATA2MSB;
	Ehdr.e_ident[EI_VERSION] = EV_CURRENT;
	Ehdr.e_type = ET_CORE;
	Ehdr.e_machine = TE_CORE_MACHINE;
	Ehdr.e_version = EV_CURRENT;
	Ehdr.e_ehsize = sizeof(Elf64_Ehdr);
	Ehdr.e_phentsize = sizeof(Elf64_Phdr);
	Ehdr.e_phoff = PhdrOffset;
	Ehdr.e_phnum = bExtendedPhnum ? PN_XNUM : (Elf64_Half)PhdrCount;
	if (bExtendedPhnum)
	{
		Ehdr.e_shoff = sizeof(Elf64_Ehdr);
		Ehdr.e_shentsize = sizeof(Elf64_Shdr);
		Ehdr.e_shnum = 1;
	}
	Core.write((const char*)&Ehdr, sizeof(Ehdr));

	if (bExtendedPhnum)
	{
		Elf64_Shdr Shdr;
		memset(&Shdr, 0, sizeof(Shdr));
		Shdr.sh_type = SHT_NULL;
		Shdr.sh_size = 1;			// would-be section count
		Shdr.sh_info = PhdrCount;	// the real program header count
		Core.write((const char*)&Shdr, sizeof(Shdr));
	}

	{
		Elf64_Phdr Phdr;
		memset(&Phdr, 0, sizeof(Phdr));
		Phdr.p_type = PT_NOTE;
		Phdr.p_offset = NoteOffset;
		Phdr.p_filesz = Notes.size();
		Phdr.p_align = 4;
		Core.write((const char*)&Phdr, sizeof(Phdr));
	}

	foreach(const SDumpRegion& Region, Regions)
	{
		Elf64_Phdr Phdr;
		memset(&Phdr, 0, sizeof(Phdr));
		Phdr.p_type = PT_LOAD;
		Phdr.p_flags = Region.Flags;
		Phdr.p_vaddr = Region.Start;
		Phdr.p_memsz = Region.End - Region.Start;
		// A region with no contents still gets an entry, with p_filesz 0, so
		// the debugger knows the address range existed.
		Phdr.p_offset = Region.DumpBytes ? Region.FileOffset : 0;
		Phdr.p_filesz = Region.DumpBytes;
		Phdr.p_align = PageSize;
		Core.write((const char*)&Phdr, sizeof(Phdr));
	}

	Core.write(Notes);

	// Pad out to the first page boundary.
	{
		const quint64 Pad = RoundUp(Core.pos(), (quint64)PageSize) - Core.pos();
		if (Pad)
			Core.write(QByteArray((int)Pad, '\0'));
	}

	//
	// The region contents. /proc/<pid>/mem is used rather than
	// process_vm_readv: it takes an explicit file offset, so a failing page can
	// be skipped and the read resumed after it, and while ptrace-attached it
	// can read mappings process_vm_readv refuses.
	//
	const int MemFd = ::open(ProcFs::ProcPath(Pid, "mem").toLocal8Bit().constData(), O_RDONLY | O_LARGEFILE);

	QByteArray Buffer(1024 * 1024, Qt::Uninitialized);
	quint64 WrittenBytes = 0;
	quint64 UnreadableBytes = 0;
	bool bCanceled = false;

	for (int i = 0; i < Regions.count() && !bCanceled; i++)
	{
		const SDumpRegion& Region = Regions[i];
		if (!Region.DumpBytes)
			continue;

		quint64 Done = 0;
		while (Done < Region.DumpBytes)
		{
			if (IsCanceling())
			{
				bCanceled = true;
				break;
			}

			const quint64 Chunk = qMin<quint64>(Region.DumpBytes - Done, (quint64)Buffer.size());

			qint64 Read = -1;
			if (MemFd >= 0)
				Read = pread(MemFd, Buffer.data(), (size_t)Chunk, (off_t)(Region.Start + Done));
			else if (bViaHelper)
			{
				//
				// The helper is still holding the threads stopped, so this reads
				// the same snapshot the register sets came from.
				//
				const QByteArray Data = LinuxHelperReadMemory(Pid, Region.Start + Done, Chunk);
				if (!Data.isEmpty())
				{
					Read = qMin<qint64>(Data.size(), (qint64)Chunk);
					memcpy(Buffer.data(), Data.constData(), (size_t)Read);
				}
			}

			if (Read < (qint64)Chunk)
			{
				//
				// Something in this chunk is not readable - most often a single
				// page that has never been faulted in, or the guard page at the
				// bottom of a thread stack.
				//
				// The rest of the chunk is retried a page at a time so that one
				// bad page costs one page: reading the whole megabyte as a unit
				// and giving up on it would throw away a megabyte of perfectly
				// good memory. Only the pages that really fail become zeros.
				//
				// Zeros rather than a short write, because the core has to match
				// the layout the program headers already promise - shortening a
				// region would put every later one at the wrong file offset.
				//
				quint64 Got = (Read > 0) ? (quint64)Read : 0;
				while (Got < Chunk)
				{
					const quint64 PageBytes = qMin<quint64>((quint64)PageSize, Chunk - Got);

					qint64 PageRead = -1;
					if (MemFd >= 0)
						PageRead = pread(MemFd, Buffer.data() + Got, (size_t)PageBytes, (off_t)(Region.Start + Done + Got));
					else if (bViaHelper)
					{
						const QByteArray Data = LinuxHelperReadMemory(Pid, Region.Start + Done + Got, PageBytes);
						if (Data.size() >= (int)PageBytes)
						{
							memcpy(Buffer.data() + Got, Data.constData(), (size_t)PageBytes);
							PageRead = (qint64)PageBytes;
						}
					}

					if (PageRead != (qint64)PageBytes)
					{
						memset(Buffer.data() + Got, 0, (size_t)PageBytes);
						UnreadableBytes += PageBytes;
					}

					Got += PageBytes;
				}

				Read = (qint64)Chunk;
			}

			if (Core.write(Buffer.constData(), Read) != Read)
			{
				emit StatusMessage(tr("Failed to write the dump: %1").arg(Core.errorString()), -1);
				bCanceled = true;
				break;
			}

			Done += (quint64)Read;
			WrittenBytes += (quint64)Read;
		}

		if (TotalBytes)
		{
			emit ProgressMessage(tr("Writing memory (%1 of %2)...")
				.arg(FormatSize(WrittenBytes)).arg(FormatSize(TotalBytes)),
				(int)(WrittenBytes * 100 / TotalBytes));
		}
	}

	if (MemFd >= 0)
		::close(MemFd);

	Core.close();

	//
	// 6. Let the process run again. This happens before the result is reported
	// so the target is never left stopped while a dialog waits for the user.
	//
	ReleaseThreads();

	if (bCanceled)
	{
		QFile::remove(m_DumpPath);
		emit StatusMessage(tr("The dump was canceled."), 0);
		return;
	}

	QString Message = tr("Core dump completed: %1").arg(FormatSize(QFileInfo(m_DumpPath).size()));
	if (!bAnyRegs)
		Message += tr(" - without register state, as the threads could not be stopped.");
	else if (UnreadableBytes)
		Message += tr(" - %1 could not be read and was written as zeros.").arg(FormatSize(UnreadableBytes));

	emit StatusMessage(Message, 0);
}
