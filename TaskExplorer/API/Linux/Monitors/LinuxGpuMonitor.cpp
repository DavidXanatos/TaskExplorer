#include "stdafx.h"
#include "LinuxGpuMonitor.h"
#include "../ProcFs.h"

#include <QDir>
#include <QFileInfo>

CLinuxGpuMonitor::CLinuxGpuMonitor(QObject *parent)
	: CGpuMonitor(parent)
{
}

CLinuxGpuMonitor::~CLinuxGpuMonitor()
{
}

bool CLinuxGpuMonitor::Init()
{
	UpdateAdapters();
	return true;
}

//
// A readable vendor name from the PCI vendor id. Only the handful that ship
// desktop/laptop GPUs are worth naming; anything else falls back to the raw id
// rather than pulling in the whole pci.ids database.
//
static QString VendorName(quint32 VendorId)
{
	switch (VendorId)
	{
		case 0x1002: return "AMD";
		case 0x10DE: return "NVIDIA";
		case 0x8086: return "Intel";
		case 0x1AF4: return "Virtio";
		case 0x1234: return "Bochs";		// QEMU's emulated VGA
		case 0x15AD: return "VMware";
		case 0x1414: return "Microsoft";	// Hyper-V synthetic
	}
	return QString("0x%1").arg(VendorId, 4, 16, QChar('0'));
}

bool CLinuxGpuMonitor::UpdateAdapters()
{
	//
	// /sys/class/drm contains both the cards themselves ("card0") and one entry
	// per connector ("card0-Virtual-1", "card0-HDMI-A-1"). Only the former are
	// adapters, and they are distinguished by having a "device" symlink.
	//
	const QStringList Entries = QDir("/sys/class/drm").entryList(QStringList("card*"), QDir::Dirs | QDir::System);

	QWriteLocker Locker(&m_StatsMutex);

	QMap<QString, SGpuInfo> NewList;

	for (const QString& Entry : Entries)
	{
		if (Entry.contains('-'))
			continue; // connector, not a card

		const QString Base = "/sys/class/drm/" + Entry;
		const QString Device = Base + "/device";
		if (!QFileInfo::exists(Device + "/vendor"))
			continue;

		SGpuInfo Info;
		Info.DeviceInterface = Entry;

		const quint32 VendorId = ProcFs::ReadFileStr(Device + "/vendor").trimmed().mid(2).toUInt(nullptr, 16);
		const quint32 DeviceId = ProcFs::ReadFileStr(Device + "/device").trimmed().mid(2).toUInt(nullptr, 16);
		Info.VendorID = VendorId;
		Info.DeviceID = DeviceId;

		// The kernel driver bound to the card, e.g. amdgpu / i915 / nvidia.
		const QString DriverPath = ProcFs::ReadLink(Device + "/driver");
		const QString DriverName = DriverPath.isEmpty() ? QString() : QFileInfo(DriverPath).fileName();

		Info.Description = QString("%1 %2 [%3:%4]")
			.arg(VendorName(VendorId))
			.arg(DriverName.isEmpty() ? QString("GPU") : DriverName)
			.arg(VendorId, 4, 16, QChar('0'))
			.arg(DeviceId, 4, 16, QChar('0'));

		Info.DriverVersion = DriverName;
		Info.LocationInfo = QFileInfo(ProcFs::ReadLink(Device)).fileName(); // PCI address

		//
		// VRAM size. Only amdgpu publishes this generically; Intel integrated
		// graphics has no dedicated memory to report, and NVIDIA's proprietary
		// driver exposes it through NVML rather than sysfs.
		//
		const QString VramTotal = ProcFs::ReadFileStr(Device + "/mem_info_vram_total").trimmed();
		if (!VramTotal.isEmpty())
		{
			Info.InstalledMemory = VramTotal.toULongLong();
			Info.Memory.DedicatedLimit = Info.InstalledMemory;
		}

		NewList.insert(Entry, Info);
	}

	// Preserve the usage figures already sampled for cards that still exist.
	for (auto I = NewList.begin(); I != NewList.end(); ++I)
	{
		auto Old = m_GpuList.constFind(I.key());
		if (Old != m_GpuList.constEnd())
		{
			I->TimeUsage = Old->TimeUsage;
			I->Memory.DedicatedUsage = Old->Memory.DedicatedUsage;
			I->Nodes = Old->Nodes;
		}
	}

	m_GpuList = NewList;

	return true;
}

bool CLinuxGpuMonitor::UpdateGpuStats()
{
	QWriteLocker Locker(&m_StatsMutex);

	for (auto I = m_GpuList.begin(); I != m_GpuList.end(); ++I)
	{
		const QString Device = "/sys/class/drm/" + I.key() + "/device";

		//
		// There is no vendor-neutral GPU utilisation counter in sysfs.
		//
		// amdgpu publishes gpu_busy_percent; Intel and NVIDIA do not, and the
		// portable alternative - summing the per-engine nanosecond counters in
		// each process's DRM fdinfo - needs a walk of every process's fd table
		// per refresh and is only present on newer kernels.
		//
		// Where nothing is available the usage stays at 0 rather than being
		// guessed at. This VM's emulated bochs-drm adapter is one such case.
		//
		const QString Busy = ProcFs::ReadFileStr(Device + "/gpu_busy_percent").trimmed();
		if (!Busy.isEmpty())
		{
			bool bOk = false;
			const int Percent = Busy.toInt(&bOk);
			if (bOk)
				I->TimeUsage = qBound(0.0f, (float)Percent / 100.0f, 1.0f);
		}

		const QString VramUsed = ProcFs::ReadFileStr(Device + "/mem_info_vram_used").trimmed();
		if (!VramUsed.isEmpty())
			I->Memory.DedicatedUsage = VramUsed.toULongLong();
	}

	return true;
}

QMap<QString, CGpuMonitor::SGpuInfo> CLinuxGpuMonitor::GetAllGpuList()
{
	QReadLocker Locker(&m_StatsMutex);
	return m_GpuList;
}

CGpuMonitor::SGpuMemory CLinuxGpuMonitor::GetGpuMemory()
{
	QReadLocker Locker(&m_StatsMutex);

	SGpuMemory Total;
	for (const SGpuInfo& Info : m_GpuList)
	{
		Total.DedicatedLimit += Info.Memory.DedicatedLimit;
		Total.DedicatedUsage += Info.Memory.DedicatedUsage;
		Total.SharedLimit += Info.Memory.SharedLimit;
		Total.SharedUsage += Info.Memory.SharedUsage;
	}
	return Total;
}
