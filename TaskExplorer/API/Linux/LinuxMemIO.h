#pragma once

#include <QIODevice>

//
// Random-access reader/writer over another process's address space.
//
// Two mechanisms are available and the implementation picks whichever works:
//   - process_vm_readv/process_vm_writev, which need no ptrace attach but are
//     gated by ptrace_scope and cannot cross a read-only mapping;
//   - /proc/<pid>/mem, which needs PTRACE_MODE_ATTACH but supports seeking;
//   - an elevated TaskHelper, when neither of the above is permitted - which is
//     the normal case for another user's process, since ptrace_scope=1 refuses
//     both even between processes of the same user.
//
// The helper can only read. Writing to a process this user has no access to
// would need a write command in the helper, which is deliberately not there.
//
class CLinuxMemIO : public QIODevice
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxMemIO)
public:
	CLinuxMemIO(quint64 BaseAddress, quint64 RegionSize, quint64 ProcessId, QObject* parent = nullptr);
	virtual ~CLinuxMemIO();

	virtual quint64		GetBaseAddress()	{ return m_BaseAddress; }
	virtual quint64		GetRegionSize()		{ return m_RegionSize; }

	virtual bool		open(OpenMode flags);
	virtual void		close();
	virtual qint64		size() const;
	virtual bool		seek(qint64 pos);
	virtual bool		isSequential() const	{ return false; }
	virtual bool		atEnd() const		{ return pos() >= size(); }

protected:
	virtual qint64		readData(char *data, qint64 maxlen);
	virtual qint64		writeData(const char *data, qint64 len);

	quint64				m_BaseAddress;
	quint64				m_RegionSize;
	quint64				m_ProcessId;

	// fd on /proc/<pid>/mem, or -1 when using process_vm_readv.
	int					m_MemFd;

	// Set when neither local mechanism is permitted and reads have to go out to
	// an elevated helper instead.
	bool				m_bUseHelper;
};
