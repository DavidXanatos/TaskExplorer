#pragma once

#include "../../Monitors/DiskMonitor.h"

class CWinDiskMonitor : public CDiskMonitor
{
	Q_OBJECT
public:
	CWinDiskMonitor(QObject *parent = nullptr);
	virtual ~CWinDiskMonitor();

	virtual bool		Init();

	virtual bool		UpdateDisks();

	virtual void		UpdateDiskStats();
};