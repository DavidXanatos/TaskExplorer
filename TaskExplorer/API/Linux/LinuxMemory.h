#pragma once
#include "../MemoryInfo.h"
#include "ProcFs.h"

//
// One mapped region of a process address space, from /proc/<pid>/maps with
// per-region detail from /proc/<pid>/smaps.
//
class CLinuxMemory : public CMemoryInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxMemory)
public:
	CLinuxMemory(QObject *parent = nullptr);
	virtual ~CLinuxMemory();

	// Region classification, mirroring the PH_MEMORY_REGION_TYPE role that
	// m_RegionType plays on Windows.
	enum ERegionType
	{
		eUnknown = 0,
		eImage,		// file-backed, executable
		eMapped,	// file-backed, not executable
		ePrivate,	// anonymous
		eHeap,		// [heap]
		eStack,		// [stack]
		eVdso,		// [vdso] / [vsyscall] / [vvar]
		eMax
	};

	virtual bool			InitStaticData(quint64 Pid, const ProcFs::SMapEntry& Entry);

	// Per-region residency from smaps, supplied separately because reading it
	// is expensive and only worth doing when the memory view is populated.
	virtual void			SetDetail(const ProcFs::SMapDetail& Detail);

	virtual QString			GetTypeString() const;
	virtual QString			GetProtectionString() const;
	virtual QString			GetAllocProtectionString() const;
	virtual bool			IsFree() const;
	virtual bool			IsExecutable() const;
	virtual bool			IsMapped() const;
	virtual bool			IsPrivate() const;
	virtual QString			GetUseString() const;

	virtual STATUS			SetProtect(quint32 Protect);
	virtual STATUS			DumpMemory(QIODevice* pFile);
	virtual STATUS			FreeMemory(bool Free);

	virtual QIODevice*		MkDevice();

	virtual QString			GetPath() const		{ QReadLocker Locker(&m_Mutex); return m_Path; }

protected:
	QString					m_Path;
	bool					m_Readable;
	bool					m_Writable;
	bool					m_Executable;
	bool					m_Shared;
};

typedef QSharedPointer<CLinuxMemory> CLinuxMemoryPtr;
typedef QWeakPointer<CLinuxMemory> CLinuxMemoryRef;
