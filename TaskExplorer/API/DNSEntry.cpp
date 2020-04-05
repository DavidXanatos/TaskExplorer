#include "stdafx.h"
#include "DNSEntry.h"

CDnsLogEntry::CDnsLogEntry(const QString& HostName, const QList<QHostAddress>& Addresses)
{
	m_HostName = HostName;
	m_Addresses = Addresses;
}

QList<QHostAddress> CDnsLogEntry::UpdateAddresses(const QList<QHostAddress>& Addresses)
{
	QWriteLocker Locker(&m_Mutex);

	QList<QHostAddress> NewAddresses;
	if (m_Addresses == Addresses)
		return NewAddresses;

	foreach(const QHostAddress& Address, Addresses)
	{
		if (m_Addresses.contains(Address))
			continue;
		m_Addresses.append(Address);
		NewAddresses.append(Address);
	}

	return NewAddresses;
}

/////////////////////////////////
//

CDnsCacheEntry::CDnsCacheEntry(const QString& HostName, quint16 Type, const QHostAddress& Address, const QString& ResolvedString, QObject *parent) : CAbstractInfoEx(parent)
{
	m_CreateTimeStamp = GetTime() * 1000;

	m_HostName = HostName;
	m_Type = Type;
	m_Address = Address;
	m_ResolvedString = ResolvedString;
	m_TTL = 0;

	m_QueryCounter = 0;
}

#ifndef DNS_TYPE_A
#define DNS_TYPE_A          0x0001      //  1
#define DNS_TYPE_AAAA       0x001c      //  28
#define DNS_TYPE_PTR        0x000c      //  12
#define DNS_TYPE_CNAME      0x0005      //  5
#define DNS_TYPE_SRV        0x0021      //  33
#define DNS_TYPE_MX         0x000f      //  15
#endif

QString CDnsCacheEntry::GetTypeString() const
{
	return GetTypeString(GetType());
}

QString CDnsCacheEntry::GetTypeString(quint16 Type)
{
	switch (Type)
	{
		case DNS_TYPE_A:	return "A";
		case DNS_TYPE_AAAA:	return "AAAA";
		case DNS_TYPE_PTR:	return "PTR";
		case DNS_TYPE_CNAME:return "CNAME";
		case DNS_TYPE_SRV:	return "SRV";
		case DNS_TYPE_MX:	return "MX";
		default:			return QString("UNKNOWN (%1)").arg(Type);
	}
}

void CDnsCacheEntry::SetTTL(quint64 TTL)
{
	QWriteLocker Locker(&m_Mutex); 

	if (m_TTL <= 0) {
		//m_CreateTimeStamp = GetTime() * 1000;
		m_RemoveTimeStamp = 0;
		m_QueryCounter++;
	}

	m_TTL = TTL; 
}

void CDnsCacheEntry::SubtractTTL(quint64 Delta)
{ 
	QWriteLocker Locker(&m_Mutex); 

	if (m_TTL > 0) // in case we flushed the cache and the entries were gone before the TTL expired
		m_TTL = 0;

	m_TTL -= Delta;	
}

/*
void CDnsCacheEntry::RecordProcess(const QString& ProcessName, quint64 ProcessId, const QWeakPointer<QObject>& pProcess, bool bUpdate)
{
	QWriteLocker Locker(&m_Mutex); 

	CDnsProcRecordPtr &pRecord = m_ProcessRecords[qMakePair(ProcessName, ProcessId)];
	if (!pRecord)
		pRecord = CDnsProcRecordPtr(new CDnsProcRecord(ProcessName, ProcessId));

	pRecord->Update(pProcess, bUpdate);
}
*/
