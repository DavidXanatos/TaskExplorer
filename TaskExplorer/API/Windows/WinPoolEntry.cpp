#include "stdafx.h"
#include "WinPoolEntry.h"
#include "ProcessHacker.h"

CWinPoolEntry::CWinPoolEntry(QObject *parent) : CAbstractInfoEx(parent)
{
}

CWinPoolEntry::~CWinPoolEntry()
{
}

bool CWinPoolEntry::InitStaticData(quint64 TagName, const QString& Driver, const QString& Description)
{
	QWriteLocker Locker(&m_Mutex); 

	m_TagName = TagName;
	m_Driver = Driver;
	m_Description = Description;

	return true;
}

bool CWinPoolEntry::UpdateDynamicData(struct _SYSTEM_POOLTAG* pPoolTagInfo)
{
	QWriteLocker Locker(&m_Mutex); 

    m_EntryStats.PagedAllocsDelta.Update(pPoolTagInfo->PagedAllocs);
    m_EntryStats.PagedFreesDelta.Update(pPoolTagInfo->PagedFrees);
    m_EntryStats.PagedCurrentDelta.Update(pPoolTagInfo->PagedAllocs - pPoolTagInfo->PagedFrees);
    m_EntryStats.PagedTotalSizeDelta.Update(pPoolTagInfo->PagedUsed);     
    m_EntryStats.NonPagedAllocsDelta.Update(pPoolTagInfo->NonPagedAllocs);
    m_EntryStats.NonPagedFreesDelta.Update(pPoolTagInfo->NonPagedFrees);
    m_EntryStats.NonPagedCurrentDelta.Update(pPoolTagInfo->NonPagedAllocs - pPoolTagInfo->NonPagedFrees);
    m_EntryStats.NonPagedTotalSizeDelta.Update(pPoolTagInfo->NonPagedUsed);

	return true;
}

QString CWinPoolEntry::GetTagString() const
{ 
	QReadLocker Locker(&m_Mutex); 
	QByteArray Arr = QByteArray((char*)&m_TagName, 4);
	return Arr;
}