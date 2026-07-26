#pragma once
#include "../../Monitors/GpuMonitor.h"

//
// GPU adapters and utilisation.
//
// The vendor-neutral source is the DRM subsystem: /sys/class/drm/card* for the
// adapter list and, on kernels that expose it, per-process engine and memory
// usage from /proc/<pid>/fdinfo of the render-node fds. Vendor-specific
// interfaces (amdgpu's /sys/class/drm/card*/device/gpu_busy_percent, NVIDIA's
// NVML) fill in what DRM does not report.
//
class CLinuxGpuMonitor : public CGpuMonitor
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxGpuMonitor)
public:
	CLinuxGpuMonitor(QObject *parent = nullptr);
	virtual ~CLinuxGpuMonitor();

	virtual bool			Init();
	virtual bool			UpdateAdapters();
	virtual bool			UpdateGpuStats();

	virtual QMap<QString, SGpuInfo>	GetAllGpuList();
	virtual SGpuMemory		GetGpuMemory();

protected:
	QMap<QString, SGpuInfo>	m_GpuList;
};
