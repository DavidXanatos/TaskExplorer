#pragma once

#include "../../Monitors/NetMonitor.h"

class CWinNetMonitor : public CNetMonitor
{
	Q_OBJECT
public:
	CWinNetMonitor(QObject *parent = nullptr);
	virtual ~CWinNetMonitor();

	virtual bool		Init();

	virtual bool		UpdateAdapters();

	virtual void		UpdateNetStats();

	void QueueUpdateAddresses();

public slots:
	void UpdateAddresses();

protected:
	void StartWatch();
	void StopWatch();

private:
	struct SWinNetMonitor* m;
};