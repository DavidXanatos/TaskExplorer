#include "stdafx.h"
#include "LinuxNetMonitor.h"
#include "../ProcFs.h"

#include <QNetworkInterface>

CLinuxNetMonitor::CLinuxNetMonitor(QObject *parent)
	: CNetMonitor(parent)
{
}

CLinuxNetMonitor::~CLinuxNetMonitor()
{
}

bool CLinuxNetMonitor::Init()
{
	UpdateAdapters();
	return true;
}

bool CLinuxNetMonitor::UpdateAdapters()
{
	const QList<ProcFs::SNetDevice> Devices = ProcFs::ReadNetDevices();

	//
	// Addresses come from Qt rather than sysfs: /sys/class/net has no address
	// listing, and the alternative is parsing /proc/net/if_inet6 plus an
	// rtnetlink dump for IPv4.
	//
	QMap<QString, QNetworkInterface> Interfaces;
	for (const QNetworkInterface& Interface : QNetworkInterface::allInterfaces())
		Interfaces.insert(Interface.name(), Interface);

	const QMap<QString, QList<QHostAddress> > Gateways = ProcFs::ReadDefaultGateways();

	//
	// DNS configuration is system-wide here. systemd-resolved does track it per
	// link, but reading that means a D-Bus round trip per interface against
	// org.freedesktop.resolve1; the system-wide view is what resolv.conf
	// describes and is what most setups actually have.
	//
	const ProcFs::SDnsConfig Dns = ProcFs::ReadDnsConfig();

	QWriteLocker Locker(&m_StatsMutex);

	QSet<QString> Seen;

	for (const ProcFs::SNetDevice& Device : Devices)
	{
		// The loopback interface carries no real traffic and would badly skew
		// the totals, since every local connection is counted twice on it.
		if (Device.IsLoopback)
			continue;

		Seen.insert(Device.Name);

		SNicInfo& Entry = m_NicList[Device.Name];

		Entry.DevicePresent = true;
		Entry.DeviceIndex = Device.Index;
		Entry.DeviceInterface = Device.Name;
		Entry.DeviceGuid = Device.MacAddress;
		Entry.DeviceName = Device.Name;
		Entry.DeviceSupported = 1;

		//
		// operstate is the authoritative link state; carrier alone is not,
		// because a "dormant" interface (associating wifi) has no carrier yet
		// is not disconnected.
		//
		if (Device.OperState == "up")
			Entry.LinkState = SNicInfo::eConnected;
		else if (Device.OperState == "down")
			Entry.LinkState = SNicInfo::eDisconnected;
		else
			Entry.LinkState = SNicInfo::eUnknown;

		// The GUI wants bits per second; sysfs reports megabits.
		Entry.LinkSpeed = (Device.SpeedMbit > 0) ? (quint64)Device.SpeedMbit * 1000 * 1000 : 0;

		// No dial-up/RAS concept on Linux.
		Entry.IsRAS = false;

		Entry.Addresses.clear();
		Entry.NetMasks.clear();
		auto FoundInterface = Interfaces.constFind(Device.Name);
		if (FoundInterface != Interfaces.constEnd())
		{
			for (const QNetworkAddressEntry& Address : FoundInterface->addressEntries())
			{
				Entry.Addresses.append(Address.ip());
				Entry.NetMasks.append(Address.netmask());
			}
		}

		Entry.Gateways = Gateways.value(Device.Name);

		//
		// Only interfaces that carry a default route get the DNS servers
		// attributed to them. Listing the resolver against every NIC - including
		// ones that are down - would be misleading.
		//
		if (!Entry.Gateways.isEmpty())
		{
			Entry.DNS = Dns.Servers;
			Entry.Domains = Dns.Domains;
		}
		else
		{
			Entry.DNS.clear();
			Entry.Domains.clear();
		}
	}

	for (auto I = m_NicList.begin(); I != m_NicList.end(); ++I)
	{
		if (!Seen.contains(I.key()))
			I.value().DevicePresent = false;
	}

	return true;
}

void CLinuxNetMonitor::UpdateNetStats()
{
	const QList<ProcFs::SNetDevice> Devices = ProcFs::ReadNetDevices();
	const quint64 CurTick = GetCurTick();

	QWriteLocker Locker(&m_StatsMutex);

	for (const ProcFs::SNetDevice& Device : Devices)
	{
		auto I = m_NicList.find(Device.Name);
		if (I == m_NicList.end())
			continue;

		SNicInfo& Entry = I.value();

		// These are cumulative byte/packet counters since the interface came
		// up; SNetStats turns them into deltas and rates.
		Entry.SetReceive(Device.RxBytes, Device.RxPackets);
		Entry.SetSend(Device.TxBytes, Device.TxPackets);

		const quint64 TimeMs = CurTick - Entry.LastStatUpdate;
		Entry.LastStatUpdate = CurTick;
		Entry.UpdateStats(TimeMs);
	}
}
