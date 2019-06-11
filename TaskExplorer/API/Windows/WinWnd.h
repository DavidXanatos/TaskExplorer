#pragma once
#include "../WndInfo.h"

class CWinWnd : public CWndInfo
{
	Q_OBJECT
public:
	CWinWnd(QObject *parent = nullptr);
	virtual ~CWinWnd();


protected:
	friend class CWindowsAPI;

};
