#pragma once
#include "../../Monitors/DiskMonitor.h"

//
// Per-block-device I/O counters from /proc/diskstats, with device geometry and
// naming from /sys/block/<dev> and mount points from /proc/self/mountinfo.
//
class CLinuxDiskMonitor : public CDiskMonitor
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxDiskMonitor)
public:
	CLinuxDiskMonitor(QObject *parent = nullptr);
	virtual ~CLinuxDiskMonitor();

	virtual bool			Init();
	virtual bool			UpdateDisks();
	virtual void			UpdateDiskStats();
};
