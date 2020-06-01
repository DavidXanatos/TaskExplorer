/*
 * Task Explorer -
 *   qt port of DNS Cache Plugin
 *
 * Copyright (C) 2014 dmex
 * Copyright (C) 2019 David Xanatos
 *
 * This file is part of Task Explorer and contains Process Hacker code.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Process Hacker.  If not, see <http://www.gnu.org/licenses/>.
 */


#include "stdafx.h"
#include "DnsResolver.h"
#include "ProcessHacker.h"
#include "../../../MiscHelpers/Common/Settings.h"

#pragma comment(lib, "dnsapi.lib")


typedef struct _DNS_CACHE_ENTRY
{
    struct _DNS_CACHE_ENTRY* Next;  // Pointer to next entry
    PCWSTR Name;                    // DNS Record Name
    USHORT Type;                    // DNS Record Type
    USHORT DataLength;              // Not referenced
    ULONG Flags;                    // DNS Record Flags
} DNS_CACHE_ENTRY, *PDNS_CACHE_ENTRY;

typedef DNS_STATUS (WINAPI* _DnsGetCacheDataTable)(
    _Inout_ PDNS_CACHE_ENTRY* DnsCacheEntry
    );

typedef BOOL (WINAPI* _DnsFlushResolverCache)(
    VOID
    );

typedef BOOL (WINAPI* _DnsFlushResolverCacheEntry)(
    _In_ PCWSTR Name
    );

struct SDnsResolver
{
	SDnsResolver()
	{
		DnsApiHandle = NULL;
		DnsGetCacheDataTable_I = NULL;
		DnsFlushResolverCache_I = NULL;
		DnsFlushResolverCacheEntry_I = NULL;
	}

	HINSTANCE DnsApiHandle;
	_DnsGetCacheDataTable DnsGetCacheDataTable_I;
	_DnsFlushResolverCache DnsFlushResolverCache_I;
	_DnsFlushResolverCacheEntry DnsFlushResolverCacheEntry_I;
};

CDnsResolver::CDnsResolver(QObject* parent)
{
	m_bRunning = false;

	m_LastCacheTraverse = 0;

	m = new SDnsResolver();
}

CDnsResolver::~CDnsResolver()
{
	m_bRunning = false;

	if (!wait(10 * 1000))
		terminate();

	// cleanup unfinished tasks
	foreach(CDnsResolverJob* pJob, m_JobQueue)
		pJob->deleteLater();
	m_JobQueue.clear();

	if(m->DnsApiHandle)
		FreeLibrary(m->DnsApiHandle);
	delete m;
}

bool CDnsResolver::Init()
{
    if (m->DnsApiHandle = LoadLibrary(L"dnsapi.dll"))
    {
        m->DnsGetCacheDataTable_I = (_DnsGetCacheDataTable)PhGetProcedureAddress(m->DnsApiHandle, "DnsGetCacheDataTable", 0);
        m->DnsFlushResolverCache_I = (_DnsFlushResolverCache)PhGetProcedureAddress(m->DnsApiHandle, "DnsFlushResolverCache", 0);
        m->DnsFlushResolverCacheEntry_I = (_DnsFlushResolverCacheEntry)PhGetProcedureAddress(m->DnsApiHandle, "DnsFlushResolverCacheEntry_W", 0);
    }

	return m->DnsApiHandle != NULL;
}

QMultiMap<QString, CDnsCacheEntryPtr>::iterator FindDnsEntry(QMultiMap<QString, CDnsCacheEntryPtr> &Entries, const QString& HostName, const QHostAddress& Address)
{
	for (QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = Entries.find(HostName); I != Entries.end() && I.key() == HostName; I++)
	{
		if (I.value()->GetAddress() == Address)
			return I;
	}

	// no matching entry
	return Entries.end();
}

QMultiMap<QString, CDnsCacheEntryPtr>::iterator FindDnsEntry(QMultiMap<QString, CDnsCacheEntryPtr> &Entries, const QString& HostName, int Type, const QString& ResolvedString)
{
	for (QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = Entries.find(HostName); I != Entries.end() && I.key() == HostName; I++)
	{
		if (I.value()->GetType() == Type && I.value()->GetResolvedString() == ResolvedString)
			return I;
	}

	// no matching entry
	return Entries.end();
}

QHostAddress RevDnsHost2Address(const QString& HostName)
{
	// 10.0.0.1 -> 1.0.0.10.in-addr.arpa
	// 2001:db8::1 -> 1.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.8.b.d.0.1.0.0.2.ip6.arpa

	QStringList HostStr = HostName.split(".");
	if (HostStr.size() >= 32 + 2 && HostStr[32] == "ip6" && HostStr[33] == "arpa")
	{
		QStringList AddrStr;
		for (int i = 32 - 1; i > 0; i -= 4)
			AddrStr.append(HostStr[i] + HostStr[i - 1] + HostStr[i - 2] + HostStr[i - 3]);
		return QHostAddress(AddrStr.join(":"));
	}
	else if (HostStr.size() >= 4 + 2 && HostStr[4] == "in-addr" && HostStr[5] == "arpa")
	{
		return QHostAddress(QString("%1.%2.%3.%4").arg(HostStr[3]).arg(HostStr[2]).arg(HostStr[1]).arg(HostStr[0]));
	}
	return QHostAddress();
}

//#define DNS_SCRAPE

#ifdef DNS_SCRAPE
BOOLEAN RecordSetContains(_In_ PDNS_RECORD Head, _In_ PDNS_RECORD Target)
{
    while (Head)
    {
        if (DnsRecordCompare(Head, Target))
            return TRUE;

        Head = Head->pNext;
    }

    return FALSE;
}

PDNS_RECORD TraverseDnsCacheTable(SDnsResolver* m)
{
    USHORT typeList[] = { DNS_TYPE_A, DNS_TYPE_AAAA, DNS_TYPE_MX, DNS_TYPE_SRV, DNS_TYPE_PTR }; // Only interested in these queries, to boost traversing performance

    PDNS_CACHE_ENTRY dnsCacheDataTable = NULL;
    if (!m->DnsGetCacheDataTable_I(&dnsCacheDataTable))
        goto CleanupExit;

	PDNS_RECORD root = NULL;
	for (PDNS_CACHE_ENTRY tablePtr = dnsCacheDataTable; tablePtr; tablePtr = tablePtr->Next)
	{
        for (USHORT i = 0; i < ARRSIZE(typeList); i++)
        {
            PDNS_RECORD dnsQueryResultPtr;
            if (DnsQuery(tablePtr->Name, typeList[i], DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL) == ERROR_SUCCESS)
			//if (DnsQuery(tablePtr->Name, tablePtr->Type, DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL) == ERROR_SUCCESS)
            {
                for (PDNS_RECORD dnsRecordPtr = dnsQueryResultPtr; dnsRecordPtr; dnsRecordPtr = dnsRecordPtr->pNext)
                {
                    if (!RecordSetContains(root, dnsRecordPtr))
                    {
                        PDNS_RECORD temp = root;
                        root = DnsRecordCopy(dnsRecordPtr);
                        root->pNext = temp;
                    }
                }

                DnsRecordListFree(dnsQueryResultPtr, DnsFreeRecordList);
            }
        }
    }

CleanupExit:

    if (dnsCacheDataTable)
        DnsRecordListFree(dnsCacheDataTable, DnsFreeRecordList);

    return root;
}
#endif

bool CDnsResolver::UpdateDnsCache()
{
#ifdef DNS_SCRAPE
	PDNS_RECORD dnsRecordRootPtr = TraverseDnsCacheTable(m); 
#else
    PDNS_CACHE_ENTRY dnsCacheDataTable = NULL;
	if (!m->DnsGetCacheDataTable_I(&dnsCacheDataTable))
		return false;
#endif

//#define DUMP_DNS

	// Copy the emtry map Map
	QMultiMap<QString, CDnsCacheEntryPtr> OldEntries = GetEntryList();

#ifdef DNS_SCRAPE
	for (PDNS_RECORD dnsRecordPtr = dnsRecordRootPtr; dnsRecordPtr != NULL; dnsRecordPtr = dnsRecordPtr->pNext)
	{
#else
	for (PDNS_CACHE_ENTRY tablePtr = dnsCacheDataTable; tablePtr != NULL; tablePtr = tablePtr->Next)
	{
		//QString HostName_ = QString::fromWCharArray(tablePtr->Name);
		//quint16 Type_ = tablePtr->Type;

#ifdef DUMP_DNS
		qDebug() << "Dns Table Entry:" << QString::fromWCharArray(tablePtr->Name) << "Type:" << CDnsCacheEntry::GetTypeString(tablePtr->Type);
#endif

		PDNS_RECORD dnsQueryResultPtr;

		DNS_STATUS ret = DnsQuery(tablePtr->Name, tablePtr->Type, DNS_QUERY_NO_WIRE_QUERY | 32768 /*Undocumented flags*/, NULL, &dnsQueryResultPtr, NULL);
		if (ret != ERROR_SUCCESS)
			continue;

		for (PDNS_RECORD dnsRecordPtr = dnsQueryResultPtr; dnsRecordPtr; dnsRecordPtr = dnsRecordPtr->pNext)
		{

#ifdef DUMP_DNS
			qDebug() << "Dns Query Result:" << QString::fromWCharArray(dnsRecordPtr->pName) << "Type:" << CDnsCacheEntry::GetTypeString(dnsRecordPtr->wType);
#endif
#endif	
			QString HostName = QString::fromWCharArray(dnsRecordPtr->pName);
			quint16 Type = dnsRecordPtr->wType;
			QHostAddress Address;
			QString ResolvedString;

			QMultiMap<QString, CDnsCacheEntryPtr>::iterator I;
			if (Type == DNS_TYPE_A || Type == DNS_TYPE_AAAA)
			{
				switch (Type)
				{
					case DNS_TYPE_A:	Address = QHostAddress(ntohl(dnsRecordPtr->Data.A.IpAddress)); break;
					case DNS_TYPE_AAAA:	Address = QHostAddress(dnsRecordPtr->Data.AAAA.Ip6Address.IP6Byte); break;
				}
				ResolvedString = Address.toString();

				if (Address == QHostAddress::LocalHost || Address == QHostAddress::LocalHostIPv6)
					continue;

				I = FindDnsEntry(OldEntries, HostName, Address);

				// Note the table may contain duplicates, filter them out
				if (I == OldEntries.end())
				{
					QReadLocker Locker(&m_Mutex);
					if (FindDnsEntry(m_DnsCache, HostName, Address) != m_DnsCache.end())
						continue;
				}
			}
			else
			{
				switch (Type)
				{
				case DNS_TYPE_PTR:		ResolvedString = QString::fromWCharArray(dnsRecordPtr->Data.PTR.pNameHost);	break;
										Address = RevDnsHost2Address(HostName);
										// we don't care for entries without a valid address
										if (Address.isNull())
											continue;
				//case DNS_TYPE_DNAME:	ResolvedString = QString::fromWCharArray(dnsRecordPtr->Data.DNAME.pNameHost); break; // entire zone
				case DNS_TYPE_CNAME:	ResolvedString = QString::fromWCharArray(dnsRecordPtr->Data.CNAME.pNameHost); break; // one host
				case DNS_TYPE_SRV:		ResolvedString = QString("%1:%2").arg(QString::fromWCharArray(dnsRecordPtr->Data.SRV.pNameTarget)).arg((quint16)dnsRecordPtr->Data.SRV.wPort); break;
				case DNS_TYPE_MX:		ResolvedString = QString::fromWCharArray(dnsRecordPtr->Data.MX.pNameExchange); break;
				default:
					continue;
				}

				I = FindDnsEntry(OldEntries, HostName, Type, ResolvedString);

				// Note the table may contain duplicates, filter them out
				if (I == OldEntries.end())
				{
					QReadLocker Locker(&m_Mutex);
					if (FindDnsEntry(m_DnsCache, HostName, Type, ResolvedString) != m_DnsCache.end())
						continue;
				}
			}

#ifdef DUMP_DNS
			//qDebug() << "Dns Query Result:" << HostName << "Type:" << CDnsCacheEntry::GetTypeString(Type) << ResolvedString;
#endif

			CDnsCacheEntryPtr pEntry;
			if (I == OldEntries.end())
			{
				pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry(HostName, Type, Address, ResolvedString));
				QWriteLocker Locker(&m_Mutex);
				m_DnsCache.insertMulti(HostName, pEntry);
				if(!Address.isNull())
					m_AddressCache.insertMulti(Address, pEntry);
				else
					m_RedirectionCache.insertMulti(ResolvedString, pEntry);
			}
			else
			{
				pEntry = I.value();
				OldEntries.erase(I);
			}

			pEntry->SetTTL(dnsRecordPtr->dwTtl * 1000); // we need that in ms

#ifndef DNS_SCRAPE
		}

		DnsRecordListFree(dnsQueryResultPtr, DnsFreeRecordList);
#endif
	}


	// keep DNS results cached for  long time, as we are using them to monitor with wha domains a programm was communicating
	quint64 CurTick = GetCurTick();
	quint64 TimeToSubtract = CurTick - m_LastCacheTraverse;
	m_LastCacheTraverse = CurTick;

	quint64 DnsPersistenceTime = theConf->GetUInt64("Options/DnsPersistenceTime", 60*60*1000); // 60 minutes
	for (QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = OldEntries.begin(); I != OldEntries.end(); )
	{
		if (I.value()->GetDeadTime() < DnsPersistenceTime)
		{
			I.value()->SubtractTTL(TimeToSubtract); // continue TTL countdown
			I = OldEntries.erase(I);
		}
		else
			I++;
	}

	QWriteLocker Locker(&m_Mutex);
	for(QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = OldEntries.begin(); I != OldEntries.end(); ++I)
	{
		CDnsCacheEntryPtr pEntry = I.value();
		if (pEntry->CanBeRemoved())
		{
			m_DnsCache.remove(I.key(), pEntry);
			QHostAddress Address = pEntry->GetAddress();
			if (!Address.isNull())
				m_AddressCache.remove(Address, pEntry);
			else
				m_RedirectionCache.remove(pEntry->GetResolvedString(), pEntry);
		}
		else if (!pEntry->IsMarkedForRemoval())
			pEntry->MarkForRemoval();
	}
	Locker.unlock();

#ifdef DNS_SCRAPE
	if (dnsRecordRootPtr)
		DnsRecordListFree(dnsRecordRootPtr, DnsFreeRecordList);
#else
    if (dnsCacheDataTable)
        DnsRecordListFree(dnsCacheDataTable, DnsFreeRecordList);
#endif

	emit DnsCacheUpdated();

	return true;
}

void CDnsResolver::ClearPersistence()
{
	foreach(const CDnsCacheEntryPtr& pEntry, GetEntryList())
		pEntry->ClearPersistence();
}

void CDnsResolver::FlushDnsCache()
{
    if (m->DnsFlushResolverCache_I)
        m->DnsFlushResolverCache_I();

	m_DnsCache.clear();
	m_AddressCache.clear();
	m_RedirectionCache.clear();
}

QString CDnsResolver::GetHostName(const QHostAddress& Address, QObject *receiver, const char *member)
{
    if (receiver && !QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CDnsResolver::GetHostNames() called with no event dispatcher");
        return QString();
    }

#ifdef _DEBUG
	QString AddressStr = Address.toString();
#endif

	if (Address.isNull() || Address == QHostAddress::AnyIPv4 || Address == QHostAddress::AnyIPv6)
		return "";

	// if its the local host return imminetly
	if (Address == QHostAddress::LocalHost || Address == QHostAddress::LocalHostIPv6)
		return "localhost";

	// Check if we have a valid reverse DNS entry in the cache
	QReadLocker ReadLocker(&m_Mutex);
	//int ValidReverseEntries = 0;
	QStringList RevHostNames;
	for (QMultiMap<QHostAddress, CDnsCacheEntryPtr>::iterator I = m_AddressCache.find(Address); I != m_AddressCache.end() && I.key() == Address; ++I)
	{
		//qDebug() << I.key().toString() << I.value()->GetResolvedString();
		if (I.value()->GetType() != DNS_TYPE_PTR)
			continue;

		if (I.value()->GetTTL() == 0)
		{
			if (I.value()->GetDeadTime() > Min(10*1000 * (I.value()->GetQueryCounter() - 1), 300*1000))
				continue;
		}

		//ValidReverseEntries++;
		RevHostNames.append(I.value()->GetResolvedString());
	}
	ReadLocker.unlock();

	// if we dont have a valid entry start a lookup job and finisch asynchroniously
	//if (ValidReverseEntries == 0)
	if(RevHostNames.size() == 0 && theConf->GetBool("Options/UserReverseDns", false))
	{
		QMutexLocker Locker(&m_JobMutex);

		// check if this address is knowwn to have no results to avoid repetitve pointeless dns queries
		if(m_NoResultBlackList[Address] > GetCurTick())
			return QString();

		if (!m_bRunning)
		{
			m_bRunning = true;
			start();
		}

		CDnsResolverJob* &pJob = m_JobQueue[Address];
		if (!pJob)
		{
			pJob = new CDnsResolverJob(Address);
			pJob->moveToThread(this);
			//QObject::connect(pJob, SIGNAL(HostResolved(const QHostAddress&, const QStringList&)), this, SLOT(OnHostResolved(const QHostAddress&, const QStringList&)), Qt::QueuedConnection);
		}
		if (receiver)
			QObject::connect(pJob, SIGNAL(HostResolved(const QHostAddress&, const QString&)), receiver, member, Qt::QueuedConnection);

		return QString();
	}

	return GetHostNameSmart(Address, RevHostNames);
}

QString CDnsResolver::GetHostNamesSmart(const QString& HostName, int Limit)
{
	// WARNING: this function is not thread safe

	QMultiMap<quint64, QString> Domins;
	if (Limit > 0)  // in case 1.a.com -> 2.a.com -> 3.a.com -> 1.a.com
	{
		// use maps to sort newest entries (biggest creation timestamp) first
		for (QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = m_RedirectionCache.find(HostName); I != m_RedirectionCache.end() && I.key() == HostName; ++I)
		{
			CDnsCacheEntryPtr pEntry = I.value();
			quint16 Type = pEntry->GetType();
			if (Type == DNS_TYPE_CNAME)
				Domins.insertMulti(-1 - pEntry->GetCreateTimeStamp(), GetHostNamesSmart(pEntry->GetHostName(), Limit - 1));
		}
	}
	if (Domins.isEmpty())
		return HostName;

	// (bla.1.com, blup.1.com) -> 1.com
	QString HostNames;
	if (Domins.size() == 1)
		HostNames = Domins.first();
	else if (Domins.size() > 1)
		HostNames = "(" + Domins.values().join(", ") + ")";
	HostNames += " -> " + HostName;
	return HostNames;
}

QString CDnsResolver::GetHostNameSmart(const QHostAddress& Address, const QStringList& RevHostNames)
{
#ifdef _DEBUG
	QString AddressStr = Address.toString();
#endif

	QMultiMap<quint64, CDnsCacheEntryPtr> Entries;
	if (theConf->GetBool("Options/MonitorDnsCache", false))
	{
		QReadLocker Locker(&m_Mutex);

		// use maps to sort newest entries (biggest creation timestamp) first
		for (QMultiMap<QHostAddress, CDnsCacheEntryPtr>::iterator I = m_AddressCache.find(Address); I != m_AddressCache.end() && I.key() == Address; ++I)
		{
			CDnsCacheEntryPtr pEntry = I.value();

			quint16 Type = pEntry->GetType();
			if (Type == DNS_TYPE_A || Type == DNS_TYPE_AAAA)
				Entries.insertMulti(-1 - pEntry->GetCreateTimeStamp(), pEntry);
		}
	}
	
	QString HostName;
	if (!Entries.isEmpty())
	{
		QReadLocker Locker(&m_Mutex);

		QStringList Domins;
		foreach(const CDnsCacheEntryPtr& pEntry, Entries)
			Domins.append(GetHostNamesSmart(pEntry->GetHostName())); // GetHostNamesSmart is recursive and not threadsafe

		if (Domins.size() == 1)
			HostName = Domins.first();
		else if (Domins.size() > 1)
			HostName = "(" + Domins.join(" | ") + ")";

		if (!RevHostNames.isEmpty())
		{
			QString RevHostName = RevHostNames.join(", ");
			if(HostName != RevHostName)
				HostName.append(" [" + RevHostName + "]");
		}
	}
	else if(!RevHostNames.isEmpty()) // no A or AAAA entries lets show only the reverse DNS result
		HostName = RevHostNames.join(", ");
	
	return HostName;
}

/*void CDnsResolver::OnHostResolved(const QHostAddress& Address, const QStringList& HostNames)
{

}*/

void CDnsResolver::run()
{
	//if(WindowsVersion >= WINDOWS_10_RS1)
	//	SetThreadDescription(GetCurrentThread(), L"DNS Resolver");

	int IdleCount = 0;
	while (m_bRunning)
	{
		QMutexLocker Locker(&m_JobMutex);
		if (m_JobQueue.isEmpty()) 
		{
			if (IdleCount++ > 4 * 10) // if we were idle for 10 seconds end the thread
			{
				m_bRunning = false;
				break;
			}
			Locker.unlock();
			QThread::msleep(250);
			continue;
		}
		IdleCount = 0;
		CDnsResolverJob* pJob = m_JobQueue.begin().value();
		Locker.unlock();

		QString AddressReverse;
		if (pJob->m_Address.protocol() == QAbstractSocket::IPv4Protocol)
		{
			quint32 ipv4 = pJob->m_Address.toIPv4Address();
			for (int i = 0; i < sizeof(quint32); i++)
				AddressReverse.append(QString("%1.").arg(((quint8*)&ipv4)[i]));
			AddressReverse.append("in-addr.arpa");
		}
		else if(pJob->m_Address.protocol() == QAbstractSocket::IPv6Protocol)
		{
			Q_IPV6ADDR ipv6 = pJob->m_Address.toIPv6Address();
			for (int i = sizeof(Q_IPV6ADDR) - 1; i >= 0; i--)
				AddressReverse.append(QString("%1.%2.").arg(ipv6.c[i] & 0xF, 0, 16).arg((ipv6.c[i] >> 4) & 0xF, 0, 16));
			AddressReverse.append("ip6.arpa");
		}

		QString HostName;
		PDNS_RECORD addressResultsRoot = NULL;
		if (DnsQuery((AddressReverse + ".").toStdWString().c_str(), DNS_TYPE_PTR, DNS_QUERY_NO_HOSTS_FILE /* | DNS_QUERY_BYPASS_CACHE*/, NULL, &addressResultsRoot, NULL) == ERROR_SUCCESS)
		{
			QStringList RevHostNames;
			for (PDNS_RECORD addressResults = addressResultsRoot; addressResults != NULL; addressResults = addressResults->pNext)
			{
				if (addressResults->wType != DNS_TYPE_PTR)
					continue;

				QString ResolvedString = QString::fromWCharArray(addressResults->Data.PTR.pNameHost);
				QHostAddress Address = RevDnsHost2Address(AddressReverse);

				QWriteLocker Locker(&m_Mutex);

				CDnsCacheEntryPtr pEntry = NULL;
				QMultiMap<QString, CDnsCacheEntryPtr>::iterator I = FindDnsEntry(m_DnsCache, AddressReverse, DNS_TYPE_PTR, ResolvedString);
				if (I == m_DnsCache.end())
				{
					pEntry = CDnsCacheEntryPtr(new CDnsCacheEntry(AddressReverse, DNS_TYPE_PTR, Address, ResolvedString));
					m_DnsCache.insertMulti(AddressReverse, pEntry);
					m_AddressCache.insertMulti(Address, pEntry);
				}
				else
					pEntry = I.value();

				pEntry->SetTTL(addressResults->dwTtl * 1000); // we need that in ms

				RevHostNames.append(ResolvedString);
			}

			if (addressResultsRoot)
				DnsRecordListFree(addressResultsRoot, DnsFreeRecordList);

			HostName = GetHostNameSmart(pJob->m_Address, RevHostNames);
		}

		Locker.relock();
		if(!HostName.isNull())
			emit pJob->HostResolved(pJob->m_Address, HostName);
		else
			m_NoResultBlackList[pJob->m_Address] = GetCurTick() + 10*60*1000; // wait 10 minute to retry
		m_JobQueue.take(pJob->m_Address)->deleteLater();
		Locker.unlock();
	}
}
