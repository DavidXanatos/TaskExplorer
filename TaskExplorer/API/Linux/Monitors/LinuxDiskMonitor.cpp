#include "stdafx.h"
#include "LinuxDiskMonitor.h"
#include "../ProcFs.h"

CLinuxDiskMonitor::CLinuxDiskMonitor(QObject *parent)
	: CDiskMonitor(parent)
{
}

CLinuxDiskMonitor::~CLinuxDiskMonitor()
{
}

bool CLinuxDiskMonitor::Init()
{
	UpdateDisks();
	return true;
}

bool CLinuxDiskMonitor::UpdateDisks()
{
	const QList<ProcFs::SDiskStat> Stats = ProcFs::ReadDiskStats();

	QWriteLocker Locker(&m_StatsMutex);

	// Anything not seen in this pass has gone away (removable media, USB).
	QSet<QString> Seen;

	for (const ProcFs::SDiskStat& Stat : Stats)
	{
		Seen.insert(Stat.Name);

		SDiskInfo& Entry = m_DiskList[Stat.Name];

		Entry.DevicePath = "/dev/" + Stat.Name;
		Entry.DeviceIndex = Stat.Minor;
		Entry.DiskIndex = Stat.Minor;
		Entry.DevicePresent = true;

		//
		// /proc/diskstats reports the same counters for every block device, so
		// unlike the Windows backend - where some controllers refuse the
		// statistics IOCTL - there is no such thing as an unsupported disk.
		//
		Entry.DeviceSupported = true;

		// Re-read on every pass: capacity changes when removable media is
		// swapped, and an empty drive reports 0 which filters it from the view.
		Entry.TotalSize = ProcFs::ReadDiskSize(Stat.Name);

		if (Entry.DeviceName.isEmpty())
		{
			const QString Model = ProcFs::ReadDiskModel(Stat.Name);
			Entry.DeviceName = Model.isEmpty() ? Stat.Name : QString("%1 (%2)").arg(Model).arg(Stat.Name);
		}

		Entry.DeviceMountPoints = ProcFs::ReadDiskMountPoints(Stat.Name).join(", ");
	}

	for (auto I = m_DiskList.begin(); I != m_DiskList.end(); ++I)
	{
		if (!Seen.contains(I.key()))
			I.value().DevicePresent = false;
	}

	return true;
}

void CLinuxDiskMonitor::UpdateDiskStats()
{
	const QList<ProcFs::SDiskStat> Stats = ProcFs::ReadDiskStats();
	const quint64 CurTick = GetCurTick();

	QWriteLocker Locker(&m_StatsMutex);

	for (const ProcFs::SDiskStat& Stat : Stats)
	{
		auto I = m_DiskList.find(Stat.Name);
		if (I == m_DiskList.end())
			continue; // appeared since the last UpdateDisks(); picked up next pass

		SDiskInfo& Entry = I.value();

		Entry.SetRead(Stat.BytesRead, Stat.ReadsCompleted);
		Entry.SetWrite(Stat.BytesWritten, Stat.WritesCompleted);

		Entry.ReadTimeDelta.Update(Stat.ReadTimeMs);
		Entry.WriteTimeDelta.Update(Stat.WriteTimeMs);
		Entry.QueryTimeDelta.Update(Stat.IoTicksMs);

		const quint64 TimeMs = CurTick - Entry.LastStatUpdate;
		Entry.LastStatUpdate = CurTick;
		Entry.UpdateStats(TimeMs);

		//
		// Mean service time per request, in milliseconds. The kernel's read and
		// write time counters are already in ms, so unlike the Windows path
		// there is no tick conversion.
		//
		const quint64 Requests = Entry.ReadDelta.Delta + Entry.WriteDelta.Delta;
		if (Requests)
			Entry.ResponseTime = (float)(Entry.ReadTimeDelta.Delta + Entry.WriteTimeDelta.Delta) / Requests;
		else
			Entry.ResponseTime = 0.0f;

		//
		// io_ticks counts milliseconds during which the request queue was not
		// empty, so its delta over the elapsed wall time is utilisation.
		//
		// This is the same quantity iostat calls %util. It saturates at 100%
		// well before a modern SSD is actually saturated, because the kernel
		// only tracks "some request outstanding", not queue depth.
		//
		if (TimeMs)
			Entry.ActiveTime = (float)Entry.QueryTimeDelta.Delta / TimeMs * 100.0f;
		else
			Entry.ActiveTime = 0.0f;

		Entry.ActiveTime = qBound(0.0f, Entry.ActiveTime, 100.0f);

		Entry.QueueDepth = (quint32)Stat.IosInProgress;
		// The block layer does not expose a split-request counter.
		Entry.SplitCount = 0;
	}
}
