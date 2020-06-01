#pragma once
#include <qobject.h>
#include "../../MiscHelpers/Common/FlexError.h"
#include "AbstractInfo.h"

#ifdef WIN32
#undef IsMinimized
#undef IsMaximized
#endif

class CWndInfo: public CAbstractInfo
{
	Q_OBJECT

public:
	CWndInfo(QObject *parent = nullptr);
	virtual ~CWndInfo();

	virtual quint64 GetHWnd() const				{ QReadLocker Locker(&m_Mutex); return m_hWnd; }
	virtual quint64 GetParentWnd() const		{ QReadLocker Locker(&m_Mutex); return m_ParentWnd; }
	virtual quint64 GetProcessId() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessId; }
	virtual quint64 GetThreadId() const			{ QReadLocker Locker(&m_Mutex); return m_ThreadId; }
	virtual QString GetProcessName() const		{ QReadLocker Locker(&m_Mutex); return m_ProcessName; }

	virtual QString GetWindowTitle() const		{ QReadLocker Locker(&m_Mutex); return m_WindowTitle; }
	virtual bool IsVisible() const				{ QReadLocker Locker(&m_Mutex); return m_WindowVisible; }
	virtual STATUS SetVisible(bool bSet) = 0;
	virtual bool IsEnabled() const				{ QReadLocker Locker(&m_Mutex); return m_WindowEnabled; }
	virtual bool IsHung() const					{ QReadLocker Locker(&m_Mutex); return m_WindowHung; }
	virtual int GetShowCommand() const			{ QReadLocker Locker(&m_Mutex); return m_ShowCommand; }
	virtual STATUS SetEnabled(bool bSet) = 0;
	virtual bool IsAlwaysOnTop() const			{ QReadLocker Locker(&m_Mutex); return m_WindowOnTop; }
	virtual STATUS SetAlwaysOnTop(bool bSet) = 0;
	virtual int GetWindowAlpha() const			{ QReadLocker Locker(&m_Mutex); return m_WindowAlpha; }
	virtual STATUS SetWindowAlpha(int iAlpha) = 0;

	virtual STATUS BringToFront() = 0;
	virtual STATUS Highlight() = 0;
	virtual bool IsNormal() const = 0;
	virtual STATUS Restore() = 0;
	virtual bool IsMinimized() const = 0;
	virtual STATUS Minimize() = 0;
	virtual bool IsMaximized() const = 0;
	virtual STATUS Maximize() = 0;
	virtual STATUS Close() = 0;
		

protected:

	quint64			m_hWnd;
	quint64			m_ParentWnd;
	quint64			m_ProcessId;
    quint64			m_ThreadId;
	QString			m_ProcessName;

	QString			m_WindowTitle;
	bool			m_WindowVisible;
	bool			m_WindowEnabled;
	bool			m_WindowHung;
	int				m_ShowCommand;
	bool			m_WindowOnTop;
	int				m_WindowAlpha;

	/*QWeakPointer<CWndInfo>						m_ParentWindow;
	QMap<quint64, QSharedPointer<CWndInfo> >	m_ChildWindows;*/
};

typedef QSharedPointer<CWndInfo> CWndPtr;
typedef QWeakPointer<CWndInfo> CWndRef;