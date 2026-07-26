#pragma once
#include "../WndInfo.h"
#include "X11Helper.h"

//
// A top-level window.
//
// On X11 this maps onto the EWMH properties (_NET_CLIENT_LIST, _NET_WM_PID,
// _NET_WM_NAME, _NET_WM_STATE) and the corresponding client messages for the
// actions.
//
// On Wayland there is deliberately no protocol for one client to enumerate or
// manipulate another client's windows, so this view will remain empty under a
// Wayland session regardless of privileges.
//
class CLinuxWnd : public CWndInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxWnd)
public:
	CLinuxWnd(QObject *parent = nullptr);
	virtual ~CLinuxWnd();

	virtual bool			InitStaticData(quint64 Window);
	virtual bool			UpdateDynamicData();

	virtual STATUS			SetVisible(bool bSet);
	virtual STATUS			SetEnabled(bool bSet);
	virtual STATUS			SetAlwaysOnTop(bool bSet);
	virtual STATUS			SetWindowAlpha(int iAlpha);

	virtual STATUS			BringToFront();
	virtual STATUS			Highlight();
	virtual bool			IsNormal() const;
	virtual STATUS			Restore();
	virtual bool			IsMinimized() const;
	virtual STATUS			Minimize();
	virtual bool			IsMaximized() const;
	virtual STATUS			Maximize();
	virtual STATUS			Close();

	virtual QString			GetWindowClass() const	{ QReadLocker Locker(&m_Mutex); return m_WindowClass; }

protected:
	// Copies a freshly queried snapshot into the members. Caller must hold the
	// write lock.
	void					Apply(const X11Helper::SWindow& Info);

	QString					m_WindowClass;	// WM_CLASS
	bool					m_IsMinimized;
	bool					m_IsMaximized;
};

typedef QSharedPointer<CLinuxWnd> CLinuxWndPtr;
typedef QWeakPointer<CLinuxWnd> CLinuxWndRef;
