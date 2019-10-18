#include "stdafx.h"
#include "WinMemIO.h"

#include "ProcessHacker.h"

struct SWinMemIO
{
	SWinMemIO()
	{
		bUnMap = false;
		BaseAddress = 0;
		RegionSize = 0;
		MemPosition = 0;
		ProcessId = NULL;
		ProcessHandle = NULL;
	}

	bool bUnMap;
	quint64 BaseAddress;
	quint64 RegionSize;
	quint64 MemPosition;
	HANDLE ProcessId;
	HANDLE ProcessHandle;
};


CWinMemIO::CWinMemIO(quint64 BaseAddress, quint64 RegionSize, quint64 ProcessId, bool bUnMap, QObject* parent)
	: QIODevice(parent)
{
	m = new SWinMemIO;

	m->BaseAddress = BaseAddress;
	m->RegionSize = RegionSize;
	m->ProcessId = (HANDLE)ProcessId;
	m->bUnMap = bUnMap;
}

CWinMemIO::~CWinMemIO()
{
	if (m->ProcessHandle)
		close();

	if (m->bUnMap && m->ProcessId == NtCurrentProcessId())
		NtUnmapViewOfSection(NtCurrentProcess(), (PVOID)m->BaseAddress);

	delete m;
}

static NTSTATUS PhpDuplicateHandleFromProcessItem(_Out_ PHANDLE NewHandle, _In_ ACCESS_MASK DesiredAccess, _In_ HANDLE ProcessId, _In_ HANDLE Handle)
{
    NTSTATUS status;
    HANDLE processHandle;

    if (!NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_DUP_HANDLE, ProcessId )))
        return status;

    status = NtDuplicateObject(processHandle, Handle, NtCurrentProcess(), NewHandle, DesiredAccess, 0, 0 );

    NtClose(processHandle);

    return status;
}

CWinMemIO* CWinMemIO::FromHandle(quint64 ProcessId, quint64 HandleId)
{
    NTSTATUS status;
    HANDLE handle = NULL;
    BOOLEAN readOnly = FALSE;

    if (!NT_SUCCESS(status = PhpDuplicateHandleFromProcessItem(&handle, SECTION_QUERY | SECTION_MAP_READ | SECTION_MAP_WRITE, (HANDLE)ProcessId, (HANDLE)HandleId)))
    {
        status = PhpDuplicateHandleFromProcessItem(&handle, SECTION_QUERY | SECTION_MAP_READ, (HANDLE)ProcessId, (HANDLE)HandleId);
        readOnly = TRUE;
    }

    if (handle)
    {
        PPH_STRING sectionName = NULL;
        PhGetHandleInformation(NtCurrentProcess(), handle, ULONG_MAX, NULL, NULL, NULL, &sectionName);

		SECTION_BASIC_INFORMATION basicInfo;
        if (NT_SUCCESS(status = PhGetSectionBasicInformation(handle, &basicInfo)))
        {
			SIZE_T viewSize = (32 * 1024 * 1024); // 32 MB
			PVOID viewBase = NULL;
			BOOLEAN tooBig = FALSE;

            //if (basicInfo.MaximumSize.QuadPart <= viewSize)
                viewSize = (SIZE_T)basicInfo.MaximumSize.QuadPart;
            //else
            //    tooBig = TRUE;

            status = NtMapViewOfSection(handle, NtCurrentProcess(), &viewBase, 0, 0, NULL, &viewSize, ViewShare, 0, readOnly ? PAGE_READONLY : PAGE_READWRITE);
            if (status == STATUS_SECTION_PROTECTION && !readOnly)
                status = NtMapViewOfSection(handle, NtCurrentProcess(), &viewBase, 0, 0, NULL, &viewSize, ViewShare, 0, PAGE_READONLY);

            if (NT_SUCCESS(status))
            {
				return new CWinMemIO((quint64)viewBase, (quint64)viewSize, (quint64)NtCurrentProcessId(), true);
            }
        }

        PhClearReference((PVOID*)&sectionName);

        NtClose(handle);
    }

	return NULL;	
}

quint64 CWinMemIO::GetBaseAddress()
{
	return m->BaseAddress;
}

quint64 CWinMemIO::GetRegionSize()
{
	return m->RegionSize;
}

bool CWinMemIO::open(OpenMode flags)
{
	ACCESS_MASK DesiredAccess = 0;
	if (flags & QIODevice::ReadOnly)
		DesiredAccess = PROCESS_VM_READ;
	if (flags & QIODevice::WriteOnly)
		DesiredAccess = PROCESS_VM_READ | PROCESS_VM_WRITE;

	NTSTATUS status;
	if (!NT_SUCCESS(status = PhOpenProcess(&m->ProcessHandle, DesiredAccess, m->ProcessId))) {
		qDebug() << QString("CWinMemIO::open failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return false;
	}

	m->MemPosition = 0;
	return QIODevice::open(flags);
}

void CWinMemIO::close()
{
	if (m->ProcessHandle) {
		NtClose(m->ProcessHandle);
		m->ProcessHandle = NULL;
	}
	QIODevice::close();
}

qint64 CWinMemIO::size() const
{
	return m->RegionSize;
}

bool CWinMemIO::seek(qint64 pos)
{
	m->MemPosition = pos;
	return QIODevice::seek(pos);
}

qint64 CWinMemIO::readData(char *data, qint64 maxlen)
{
	if (m->MemPosition + maxlen > m->RegionSize)
		maxlen = m->RegionSize - m->MemPosition;

	NTSTATUS status = NtReadVirtualMemory(m->ProcessHandle, PTR_ADD_OFFSET((PVOID)m->BaseAddress, m->MemPosition), data, maxlen, NULL);
	/*if (status == STATUS_PARTIAL_COPY) 
		status = KphReadVirtualMemoryUnsafe(m->ProcessHandle, PTR_ADD_OFFSET((PVOID)m->BaseAddress, m->MemPosition), data, maxlen, NULL);*/
	if (!NT_SUCCESS(status)) {
		qDebug() << QString("CWinMemIO::readData failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return -1;
	}

	m->MemPosition += maxlen;

	return maxlen;
}

qint64 CWinMemIO::writeData(const char *data, qint64 len)
{
	NTSTATUS status = NtWriteVirtualMemory(m->ProcessHandle, PTR_ADD_OFFSET((PVOID)m->BaseAddress, m->MemPosition), (char*)data, len, NULL);
	if (!NT_SUCCESS(status)) {
		qDebug() << QString("CWinMemIO::writeData failed: 0x%1").arg((quint32)status, 8, 16, QChar('0'));
		return -1;
	}
	
	m->MemPosition += len;

	return len;
}