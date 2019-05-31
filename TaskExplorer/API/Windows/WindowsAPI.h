#pragma once
#include "../SystemAPI.h"
#include "WinProcess.h"
#include "WinSocket.h"

class CWindowsAPI : public CSystemAPI
{
	Q_OBJECT

public:
	CWindowsAPI(QObject *parent = nullptr);
	virtual ~CWindowsAPI();

	virtual bool UpdateProcessList();

	virtual bool UpdateSocketList();

protected:
};


