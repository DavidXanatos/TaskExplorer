#pragma once
#include "../MemoryInfo.h"
#include "../ProcessInfo.h"

class CWinMemory : public CMemoryInfo
{
	Q_OBJECT
public:
	CWinMemory(QObject *parent = nullptr);
	virtual ~CWinMemory();

	void	InitBasicInfo(struct _MEMORY_BASIC_INFORMATION* basicInfo, void* ProcessId);

	virtual QString GetTypeString() const;
	virtual QString GetMemoryTypeString() const;
	virtual QString GetMemoryStateString() const;
	virtual QString GetProtectString() const;
	virtual bool IsExecutable() const;
	virtual bool IsBitmapRegion() const;
	virtual bool IsFree() const;
	virtual bool IsMapped() const;
	virtual bool IsPrivate() const;
	virtual QString GetUseString() const;

	virtual STATUS SetProtect(quint32 Protect);
	virtual STATUS DumpMemory(QIODevice* pFile);
	virtual STATUS FreeMemory(bool Free);

	virtual QIODevice* MkDevice();

protected:
	friend long PhQueryMemoryItemList(void* ProcessId, ulong Flags, QMap<quint64, CMemoryPtr>& MemoryMap);

	friend long PhpUpdateMemoryWsCounters(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessHandle);
	//friend long PhpUpdateMemoryWsCountersOld(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessHandle);
	friend QMap<quint64, CMemoryPtr>::iterator PhLookupMemoryItemList(QMap<quint64, CMemoryPtr>& MemoryMap, void* Address);
	friend QSharedPointer<CWinMemory> PhpSetMemoryRegionType(QMap<quint64, CMemoryPtr>& MemoryMap, void* Address, unsigned char GoToAllocationBase, int RegionType);
	friend long PhpUpdateMemoryRegionTypes(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessId, void* ProcessHandle);

    union
    {
        struct
        {
            unsigned char PropertyOfAllocationBase;
        } Custom;
        struct
        {
            void* ThreadId;
        } Teb;
        struct
        {
            void* ThreadId;
        } Stack;
        struct
        {
            ulong Index;
        } Heap;
    } u;
	QString u_Custom_Text; // u_MappedFile_FileName;
	QSharedPointer<CWinMemory> u_HeapSegment_HeapItem;
};
