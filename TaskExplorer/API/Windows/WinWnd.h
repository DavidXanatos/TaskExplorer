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

	struct SWndInfo
	{
		QString AppID;
		QString Text;
		QString Thread;
		QRect	Rect;
		QRect	NormalRect;
		QRect	ClientRect;
		quint64 MenuHandle;
		quint64 InstanceHandle;
		QString InstanceString;
		quint64 UserdataHandle;
		bool	IsUnicode;
		ulong	WindowId;
		QString Font;
		QString Styles;
		QString StylesEx;

		QString ClassName;
		quint64 Atom;
		quint64 hIcon;
		quint64 hIconSm;
		quint64 lpszMenuName;
		quint64 hCursor;
		quint64 hbrBackground;
		QString StylesClass;
		quint64 InstanceHandle2;
		QString InstanceString2;

		QMap<QString, QString> Properties;

		//QMap<QString, QString> PropertyStorage; // todo:
	};

	virtual SWndInfo GetWndInfo() const;

protected:
	friend class CWindowsAPI;
	friend class CWinProcess;

	bool  InitStaticData(const CWindowsAPI::SWndInfo& WndInfo, void* QueryHandle);
	bool UpdateDynamicData();

	QString			m_WindowClass;
	QString			m_ModuleString;
};
