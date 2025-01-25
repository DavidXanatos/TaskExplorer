#include "stdafx.h"
#include "WinMemory.h"
#include "WindowsAPI.h"
#include "WinMemIO.h"

#include "ProcessHacker.h"
#include "ProcessHacker/memprv.h"

CWinMemory::CWinMemory(QObject *parent) 
	: CMemoryInfo(parent) 
{
	memset(&u, 0, sizeof(u));
}

CWinMemory::~CWinMemory()
{
	
}

void CWinMemory::InitBasicInfo(struct _MEMORY_BASIC_INFORMATION* basicInfo, void* ProcessId)
{
	m_ProcessId = (quint64)ProcessId;

	m_BaseAddress = (quint64)basicInfo->BaseAddress;
	m_AllocationBase = (quint64)basicInfo->AllocationBase;
	m_AllocationProtect = basicInfo->AllocationProtect;
	m_RegionSize = basicInfo->RegionSize;
	m_State = basicInfo->State;
	m_Protect = basicInfo->Protect;
	m_Type = basicInfo->Type;
}

QString CWinMemory::GetMemoryTypeString() const
{
	QReadLocker Locker(&m_Mutex);

    if (m_Type & MEM_PRIVATE)
        return tr("Private");
    else if (m_Type & MEM_MAPPED)
        return tr("Mapped");
    else if (m_Type & MEM_IMAGE)
        return tr("Image");
    return tr("Unknown");
}

QString CWinMemory::GetRegionTypeExStr() const
{
    if (!m_RegionTypeEx)
        return "";

	QStringList regionTypes;

    if (m_Private)
        regionTypes.append(tr("Private"));
    if (m_MappedDataFile)
        regionTypes.append(tr("MappedDataFile"));
    if (m_MappedImage)
        regionTypes.append(tr("MappedImage"));
    if (m_MappedPageFile)
        regionTypes.append(tr("MappedPageFile"));
    if (m_MappedPhysical)
        regionTypes.append(tr("MappedPhysical"));
    if (m_DirectMapped)
        regionTypes.append(tr("DirectMapped"));
    if (m_SoftwareEnclave)
        regionTypes.append(tr("Software enclave"));
    if (m_PageSize64K)
        regionTypes.append(tr("PageSize64K"));
    if (m_PlaceholderReservation)
        regionTypes.append(tr("Placeholder"));
    if (m_MappedAwe)
        regionTypes.append(tr("Mapped AWE"));
    if (m_MappedWriteWatch)
        regionTypes.append(tr("MappedWriteWatch"));
    if (m_PageSizeLarge)
        regionTypes.append(tr("PageSizeLarge"));
    if (m_PageSizeHuge)
        regionTypes.append(tr("PageSizeHuge"));

    return regionTypes.join(tr(", "));
}

QString CWinMemory::GetMemoryStateString() const
{
	QReadLocker Locker(&m_Mutex);

    if (m_State & MEM_COMMIT)
        return tr("Commit");
    else if (m_State & MEM_RESERVE)
        return tr("Reserved");
    else if (m_State & MEM_FREE)
        return tr("Free");
    return tr("Unknown");
}

QString CWinMemory::GetTypeString() const
{
	if (GetState() & MEM_FREE)
	{
		if (GetRegionType() == UnusableRegion)
			return tr("Free (Unusable)");
		else
			return tr("Free");
	}
	else if (IsAllocationBase())
	{
		return GetMemoryTypeString();
	}
	else
	{
		return tr("%1: %2").arg(GetMemoryTypeString()).arg(GetMemoryStateString());
	}
}

QString CWinMemory::GetProtectionString(quint32 Protect)
{
	if (!Protect)
		return "";

	QString str;
    if (Protect & PAGE_NOACCESS)
        str = tr("NA");
    else if (Protect & PAGE_READONLY)
        str = tr("R");
    else if (Protect & PAGE_READWRITE)
        str = tr("RW");
    else if (Protect & PAGE_WRITECOPY)
        str = tr("WC");
    else if (Protect & PAGE_EXECUTE)
        str = tr("X");
    else if (Protect & PAGE_EXECUTE_READ)
        str = tr("RX");
    else if (Protect & PAGE_EXECUTE_READWRITE)
        str = tr("RWX");
    else if (Protect & PAGE_EXECUTE_WRITECOPY)
        str = tr("WCX");
    else
        str = tr("?");

    if (Protect & PAGE_GUARD)
		str += tr("+G");

    if (Protect & PAGE_NOCACHE)
		str += tr("+NC");

    if (Protect & PAGE_WRITECOMBINE)
		str += tr("+WCM");

	return str;
}

QString CWinMemory::GetProtectionString() const
{
	return GetProtectionString(GetProtection());
}

QString CWinMemory::GetAllocProtectionString() const
{
	return GetProtectionString(GetAllocProtection());
}

bool CWinMemory::IsExecutable() const
{
	return (GetProtection() & (PAGE_EXECUTE | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY)) != 0;
}

bool CWinMemory::IsBitmapRegion() const
{
	QReadLocker Locker(&m_Mutex);
	return m_RegionType == CfgBitmapRegion || m_RegionType == CfgBitmap32Region;
}

bool CWinMemory::IsFree() const
{
	return (GetState() & MEM_FREE) != 0;
}

bool CWinMemory::IsMapped() const
{
	return (GetType() & (MEM_MAPPED | MEM_IMAGE)) != 0;
}

bool CWinMemory::IsPrivate() const
{
	return (GetType() & MEM_PRIVATE) != 0; 
}


QString CWinMemory::GetUseString() const
{
    PH_MEMORY_REGION_TYPE type = (PH_MEMORY_REGION_TYPE)m_RegionType;

    switch (type)
    {
    case UnknownRegion:
        return "";
    case CustomRegion:
        return u_Custom_Text;
    case UnusableRegion:
        return "";
    case MappedFileRegion:
        return u_Custom_Text;
    case UserSharedDataRegion:
        return tr("USER_SHARED_DATA");
    case HypervisorSharedDataRegion:
        return tr("HYPERVISOR_SHARED_DATA");
    case PebRegion:
    case Peb32Region:
        return tr("PEB%1").arg(QString(type == Peb32Region ? tr(" 32-bit") : ""));
    case TebRegion:
    case Teb32Region:
        return tr("TEB%1 (thread %2)").arg(QString(type == Teb32Region ? tr(" 32-bit") : "")).arg((quint64)u.Teb.ThreadId);
    case StackRegion:
    case Stack32Region:
        return tr("Stack%1 (thread %2)").arg(QString(type == Stack32Region ? tr(" 32-bit") : "")).arg((quint64)u.Stack.ThreadId);
    case HeapRegion:
    case Heap32Region:
        return tr("Heap%1 (ID %2)").arg(QString(type == Heap32Region ? tr(" 32-bit") : "")).arg((ULONG)u.Heap.Index + 1);
    case HeapSegmentRegion:
    case HeapSegment32Region:
        return tr("Heap segment%1 (ID %2)").arg(QString(type == HeapSegment32Region ? tr(" 32-bit") : "").arg((ULONG)u_HeapSegment_HeapItem->u.Heap.Index + 1));
    case CfgBitmapRegion:
    case CfgBitmap32Region:
        return tr("CFG Bitmap%1").arg(QString(type == CfgBitmap32Region ? tr(" 32-bit") : ""));
    case ApiSetMapRegion:
        return tr("ApiSetMap");
    default:
        return "";
    }
}

quint8 CWinMemory::GetSigningLevel() const 
{ 
    QReadLocker Locker(&m_Mutex);
    return u.MappedFile.SigningLevel; 
}

QString CWinMemory::GetSigningLevelString() const
{
	SE_SIGNING_LEVEL SigningLevel = (SE_SIGNING_LEVEL)GetSigningLevel();

    switch (SigningLevel)
    {
        case SE_SIGNING_LEVEL_UNCHECKED:
            return tr("Unchecked");
        case SE_SIGNING_LEVEL_UNSIGNED:
            return tr("Unsigned");
        case SE_SIGNING_LEVEL_ENTERPRISE:
            return tr("Enterprise");
        case SE_SIGNING_LEVEL_DEVELOPER:
            return tr("Developer");
        case SE_SIGNING_LEVEL_AUTHENTICODE:
            return tr("Authenticode");
        case SE_SIGNING_LEVEL_STORE:
            return tr("StoreApp");
        case SE_SIGNING_LEVEL_ANTIMALWARE:
            return tr("Antimalware");
        case SE_SIGNING_LEVEL_MICROSOFT:
            return tr("Microsoft");
        case SE_SIGNING_LEVEL_DYNAMIC_CODEGEN:
            return tr("CodeGen");
        case SE_SIGNING_LEVEL_WINDOWS:
            return tr("Windows");
        case SE_SIGNING_LEVEL_WINDOWS_TCB:
            return tr("WinTcb");
        case SE_SIGNING_LEVEL_CUSTOM_2:
        case SE_SIGNING_LEVEL_CUSTOM_4:
        case SE_SIGNING_LEVEL_CUSTOM_5:
        case SE_SIGNING_LEVEL_CUSTOM_6:
        case SE_SIGNING_LEVEL_CUSTOM_7:
            return tr("Custom");
        default:
            return tr("");
    }
}

STATUS CWinMemory::SetProtect(quint32 Protect)
{
	NTSTATUS status;
	HANDLE processHandle;

	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_VM_OPERATION, (HANDLE)GetProcessId())))
	{
		QWriteLocker Locker(&m_Mutex);

		PVOID baseAddress;
		SIZE_T regionSize;
		ULONG oldProtect;

		baseAddress = (PVOID)m_BaseAddress;
		regionSize = m_RegionSize;

		status = NtProtectVirtualMemory(
			processHandle,
			&baseAddress,
			&regionSize,
			(ULONG)Protect,
			&oldProtect
			);

		if (NT_SUCCESS(status))
			m_Protect = Protect;
	}

	if (!NT_SUCCESS(status))
		return ERR(tr("Unable to change memory protection"), status);
	return OK;
}

STATUS CWinMemory::DumpMemory(QIODevice* pFile)
{
	if (!IsAllocationBase() && (GetState() & MEM_COMMIT) == 0)
		return ERR(tr("Not dumpable memory item"), -1);

	QReadLocker Locker(&m_Mutex);

	NTSTATUS status;
	HANDLE processHandle;
	if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_VM_READ, (HANDLE)m_ProcessId)))
		return ERR(tr("Unable to open the process"), status);

	PVOID buffer = PhAllocatePage(PAGE_SIZE, NULL);

	for (size_t offset = 0; offset < m_RegionSize; offset += PAGE_SIZE)
	{
		if (NT_SUCCESS(NtReadVirtualMemory(processHandle, PTR_ADD_OFFSET((PVOID)m_BaseAddress, offset), buffer, PAGE_SIZE, NULL)))
		{
			pFile->write((char*)buffer, PAGE_SIZE);
		}
	}

	PhFreePage(buffer);

	NtClose(processHandle);

	return OK;
}

STATUS CWinMemory::FreeMemory(bool Free)
{
	NTSTATUS status;
    HANDLE processHandle;

	if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_VM_OPERATION, (HANDLE)GetProcessId())))
    {
        PVOID baseAddress;
        SIZE_T regionSize;

        baseAddress = (PVOID)GetBaseAddress();

        if (!IsMapped())
        {
            // The size needs to be 0 if we're freeing.
            if (Free)
                regionSize = 0;
            else
                regionSize = GetRegionSize();

            status = NtFreeVirtualMemory(
                processHandle,
                &baseAddress,
                &regionSize,
                Free ? MEM_RELEASE : MEM_DECOMMIT
                );
        }
        else
        {
            status = NtUnmapViewOfSection(processHandle, baseAddress);
        }

        NtClose(processHandle);
    }

    if (!NT_SUCCESS(status))
    {
        QString Message;
        if (IsMapped())
			Message = tr("Unable to unmap the section view");
		else if (Free)
            Message = tr("Unable to free the memory region");
        else
            Message = tr("Unable to decommit the memory region");

		return ERR(Message, status);
    }
	return OK;
}

QIODevice* CWinMemory::MkDevice()
{
	if (!IsAllocationBase() && (GetState() & MEM_COMMIT) == 0)
		return NULL;

	return new CWinMemIO(GetBaseAddress(), GetRegionSize(), GetProcessId());
}

