#pragma once
#include "../MemDumper.h"

#include <QAtomicInt>

//
// Writes an ELF core file for a live process - the Linux counterpart of the
// Windows minidump.
//
// The format is the same one the kernel itself produces when a process dies on
// a fatal signal, and the same one gdb's "gcore" writes, so the result can be
// opened with:
//
//     gdb <executable> <corefile>
//
// Structure of the file:
//
//     Elf64_Ehdr                  e_type = ET_CORE
//     Elf64_Phdr * (1 + N)        one PT_NOTE, then one PT_LOAD per region
//     notes                       process/thread state, see BuildNotes()
//     region data                 page aligned, in program-header order
//
// Producing a *useful* core needs the threads stopped, both so that the memory
// is a consistent snapshot and so that the register sets can be read at all.
// That means ptrace, which under the default Yama policy (ptrace_scope=1) only
// works for descendants unless TaskExplorer runs elevated. When the attach is
// refused the memory is still dumped - the core just has no register state, so
// gdb can inspect data but cannot produce a backtrace. The dialog says so
// rather than silently writing a crippled file.
//
class CLinuxDumper : public CMemDumper
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxDumper)
public:
	CLinuxDumper(QObject* parent = nullptr);
	virtual ~CLinuxDumper();

	virtual STATUS			PrepareDump(const CProcessPtr& pProcess, quint32 DumpType, const QString& DumpPath);

public slots:
	virtual void			Cancel();

protected:
	virtual void			run();

	bool					IsCanceling() const		{ return m_Canceled.loadAcquire() != 0; }

	quint64					m_ProcessId;
	QString					m_ProcessName;
	QString					m_DumpPath;
	quint32					m_DumpType;

	QAtomicInt				m_Canceled;
};
