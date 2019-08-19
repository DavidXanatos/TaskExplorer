#pragma once
#include "../WndInfo.h"

#undef IsMinimized
#undef IsMaximized

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
	virtual bool IsNormal() const;
	virtual STATUS Restore();
	virtual bool IsMinimized() const;
	virtual STATUS Minimize();
	virtual bool IsMaximized() const;
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

		QMap<QString, QString> PropertyStorage;
	};

	virtual SWndInfo GetWndInfo() const;

	typedef void (*WNDENUMPROCEX)(quint64 hWnd, /*quint64 hParent,*/ void* Param);

	static void EnumAllWindows(WNDENUMPROCEX in_Proc, void* in_Param);

protected:
	friend class CWindowsAPI;
	friend class CWinProcess;

	bool InitStaticData(quint64 ProcessId, quint64 ThreadId, quint64 hWnd, void* QueryHandle);
	bool UpdateDynamicData();

	QString			m_WindowClass;
	QString			m_ModuleString;
};