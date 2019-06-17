#pragma once
#include "WindowsAPI.h"
#include "../WndInfo.h"

class CWinWnd : public CWndInfo
{
	Q_OBJECT
public:
	CWinWnd(QObject *parent = nullptr);
	virtual ~CWinWnd();

	virtual QString GetWindowClass() const		{ QReadLocker Locker(&m_Mutex); return m_WindowClass; }
	virtual QString GetModuleString() const		{ QReadLocker Locker(&m_Mutex); return m_ModuleString; }

	virtual STATUS SetVisible(bool bSet);
	virtual STATUS SetEnabled(bool bSet);
	virtual STATUS SetAlwaysOnTop(bool bSet);
	virtual STATUS SetWindowAlpha(int iAlpha);

	virtual STATUS BringToFront();
	virtual STATUS Highlight();
	virtual STATUS Restore();
	virtual STATUS Minimize();
	virtual STATUS Maximize();
	virtual STATUS Close();

protected:
	friend class CWindowsAPI;
	friend class CWinProcess;

	bool  InitStaticData(const CWindowsAPI::SWndInfo& WndInfo, void* QueryHandle);
	bool UpdateDynamicData();

	QString			m_WindowClass;
	QString			m_ModuleString;
};
