#include "stdafx.h"
#include "NetMonitor.h"

CNetMonitor::CNetMonitor(QObject *parent)
	: QObject(parent)
{
}

CNetMonitor::~CNetMonitor()
{
}


QMap<QString, CNetMonitor::SNicInfo> CNetMonitor::GetNicList(bool bFilterDisconnected) const 
{ 
	QReadLocker Locker(&m_StatsMutex);

	QMap<QString, SNicInfo> NicList;
	for (QMap<QString, SNicInfo>::const_iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		const SNicInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;
		
		if(bFilterDisconnected && entry->LinkState == SNicInfo::eDisconnected)
			continue;

		NicList.insert(I.key(), I.value());
	}
	return NicList;
}

CNetMonitor::SDataRates CNetMonitor::GetTotalDataRate(ERates RAS) const
{
	QReadLocker Locker(&m_StatsMutex);

	SDataRates DataRates;

	for (QMap<QString, SNicInfo>::const_iterator I = m_NicList.begin(); I != m_NicList.end(); I++)
	{
		const SNicInfo* entry = &I.value();

		if (!entry->DevicePresent)
			continue;

		if (RAS != eAll && (RAS == eRas) != entry->IsRAS)
			continue;

		DataRates.ReceiveRate += entry->ReceiveRate.Get();
		DataRates.SendRate += entry->SendRate.Get();
	}

	return DataRates;
}

bool CNetMonitor::IsLanIP(const QHostAddress& Address) const
{
	/*
    Returns true if the address is an IPv4 or IPv6 global address, false
    otherwise. A global address is an address that is not reserved for
    special purposes (like loopback or multicast) or future purposes.

    Note that IPv6 unique local unicast addresses are considered global
    addresses (see isUniqueLocalUnicast()), as are IPv4 addresses reserved for
    local networks by https://tools.ietf.org/html/rfc1918.

    Also note that IPv6 site-local addresses are deprecated and should be
    considered as global in new applications. This function returns true for
    site-local addresses too.
	*/
	if (!Address.isGlobal())
		return true;

	// ToDo: check with actual NIC's in the system

	if (quint32 ipv4 = Address.toIPv4Address())
	{
		if ((ipv4 & 0xff000000) == 0x0a000000) // 10.0.0.0 - 10.255.255.255 (10/8 prefix)
			return true;
		if ((ipv4 & 0xfff00000) == 0xac100000) // 172.16.0.0 - 172.31.255.255 (172.16/12 prefix)
			return true;
		if ((ipv4 & 0xffff0000) == 0xc0a80000) // 192.168.0.0 - 192.168.255.255 (192.168/16 prefix)
			return true;
	}
	else // IPv6
	{
		Q_IPV6ADDR ipv6 = Address.toIPv6Address();
		if (ipv6.c[0] == 0xfc || ipv6.c[0] == 0xfd) // Unique local address
			return true;
	}

	return false;
}