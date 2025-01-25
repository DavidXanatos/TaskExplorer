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
	virtual QString GetRegionTypeExStr() const;
	virtual QString GetMemoryStateString() const;
	virtual QString GetProtectionString() const;
	virtual QString GetAllocProtectionString() const;
	virtual bool IsExecutable() const;
	virtual bool IsBitmapRegion() const;
	virtual bool IsFree() const;
	virtual bool IsMapped() const;
	virtual bool IsPrivate() const;
	virtual QString GetUseString() const;

	virtual quint8 GetSigningLevel() const;
	virtual QString GetSigningLevelString() const;

	virtual STATUS SetProtect(quint32 Protect);
	virtual STATUS DumpMemory(QIODevice* pFile);
	virtual STATUS FreeMemory(bool Free);

	virtual QIODevice* MkDevice();

	static QString GetProtectionString(quint32 Protect);

protected:
	friend long PhQueryMemoryItemList(void* ProcessId, ulong Flags, QMap<quint64, CMemoryPtr>& MemoryMap);

	friend long PhpUpdateMemoryWsCounters(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessHandle);
	//friend long PhpUpdateMemoryWsCountersOld(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessHandle);
	friend QMap<quint64, CMemoryPtr>::iterator PhLookupMemoryItemList(QMap<quint64, CMemoryPtr>& MemoryMap, void* Address);
	friend QSharedPointer<CWinMemory> PhpSetMemoryRegionType(QMap<quint64, CMemoryPtr>& MemoryMap, void* Address, unsigned char GoToAllocationBase, int RegionType);
	friend long PhpUpdateMemoryRegionTypes(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessId, void* ProcessHandle);
	friend void PhpUpdateHeapRegions(QMap<quint64, CMemoryPtr>& MemoryMap, void* ProcessId, void* ProcessHandle);

	union
	{
		quint8			m_Attributes;
		struct
		{
			quint8		m_Valid : 1;
			quint8		m_Bad : 1;
			quint8		m_Spare : 6;
		};
	};

	union
	{
		quint32			m_RegionTypeEx;
		struct
		{
			quint32		m_Private : 1;
			quint32		m_MappedDataFile : 1;
			quint32		m_MappedImage : 1;
			quint32		m_MappedPageFile : 1;
			quint32		m_MappedPhysical : 1;
			quint32		m_DirectMapped : 1;
			quint32		m_SoftwareEnclave : 1; // REDSTONE3
			quint32		m_PageSize64K : 1;
			quint32		m_PlaceholderReservation : 1; // REDSTONE4
			quint32		m_MappedAwe : 1; // 21H1
			quint32		m_MappedWriteWatch : 1;
			quint32		m_PageSizeLarge : 1;
			quint32		m_PageSizeHuge : 1;
			quint32		m_Reserved : 19; // Sync with MemoryRegionInformationEx (dmex)
		};
	};

	union
	{
		struct
		{
			unsigned char PropertyOfAllocationBase;
		} Custom;
		struct
		{
			quint8 SigningLevelValid;
			quint8 SigningLevel;
		} MappedFile;
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
			quint32 Index;
			quint8 ClassValid;
			quint32 Class;
		} Heap;
		struct
		{
		} HeapSegment;
		struct
		{
			int Type; // PH_ACTIVATION_CONTEXT_DATA_TYPE
		} ActivationContextData;
	} u;
	QString u_Custom_Text; // u_MappedFile_FileName;
	QSharedPointer<CWinMemory> u_HeapSegment_HeapItem;

};
