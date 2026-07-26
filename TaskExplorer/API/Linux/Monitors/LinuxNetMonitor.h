#pragma once
#include "../../Monitors/NetMonitor.h"

//
// Per-interface counters from /sys/class/net/<if>/statistics, with addresses
// and link state from the same tree (or rtnetlink, if that proves easier to
// keep in sync).
//
class CLinuxNetMonitor : public CNetMonitor
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxNetMonitor)
public:
	CLinuxNetMonitor(QObject *parent = nullptr);
	virtual ~CLinuxNetMonitor();

	virtual bool			Init();
	virtual bool			UpdateAdapters();
	virtual void			UpdateNetStats();
};
