#include "stdafx.h"
#include "LinuxWnd.h"
#include "LinuxHelper.h"
#include "X11Helper.h"

CLinuxWnd::CLinuxWnd(QObject *parent)
	: CWndInfo(parent)
{
	m_IsMinimized = false;
	m_IsMaximized = false;
}

CLinuxWnd::~CLinuxWnd()
{
}

bool CLinuxWnd::InitStaticData(quint64 Window)
{
	X11Helper::SWindow Info;
	if (!X11Helper::QueryWindow(Window, Info))
		return false;

	QWriteLocker Locker(&m_Mutex);
	m_hWnd = Window;
	Apply(Info);
	return true;
}

void CLinuxWnd::Apply(const X11Helper::SWindow& Info)
{
	// Caller holds the write lock.
	m_hWnd = Info.Window;
	m_ParentWnd = Info.ParentWindow;
	m_ProcessId = Info.ProcessId;
	m_WindowTitle = Info.Title;
	m_WindowClass = Info.WindowClass;
	m_WindowVisible = Info.Visible;
	m_WindowOnTop = Info.AlwaysOnTop;
	m_WindowAlpha = Info.Alpha;
	m_IsMinimized = Info.Minimized;
	m_IsMaximized = Info.Maximized;

	//
	// X11 has no per-window enabled flag and no "not responding" state that is
	// cheap to sample - detecting a hung client means round-tripping _NET_WM_PING
	// and waiting for the reply, which is not worth doing on every refresh.
	//
	m_WindowEnabled = true;
	m_WindowHung = false;

	// The show command mirrors the Windows SW_* values the shared views expect:
	// 2 minimized, 3 maximized, 1 normal.
	m_ShowCommand = Info.Minimized ? 2 : (Info.Maximized ? 3 : 1);
}

bool CLinuxWnd::UpdateDynamicData()
{
	X11Helper::SWindow Info;
	if (!X11Helper::QueryWindow(GetHWnd(), Info))
		return false;

	QWriteLocker Locker(&m_Mutex);

	const bool bChanged = (m_WindowTitle != Info.Title)
		|| (m_WindowVisible != Info.Visible)
		|| (m_IsMinimized != Info.Minimized)
		|| (m_IsMaximized != Info.Maximized)
		|| (m_WindowOnTop != Info.AlwaysOnTop);

	Apply(Info);
	return bChanged;
}

STATUS CLinuxWnd::SetVisible(bool bSet)
{
	if (!X11Helper::SetVisible(GetHWnd(), bSet))
		return ERR(tr("Failed to change the window visibility."));
	return OK;
}

STATUS CLinuxWnd::SetEnabled(bool bSet)
{
	// X11 has no notion of a disabled top-level window; input is either grabbed
	// by the client or not.
	return ERR(tr("Enabling or disabling a window is not supported on X11."));
}

STATUS CLinuxWnd::SetAlwaysOnTop(bool bSet)
{
	if (!X11Helper::SetAlwaysOnTop(GetHWnd(), bSet))
		return ERR(tr("Failed to change the always-on-top state."));
	return OK;
}

STATUS CLinuxWnd::SetWindowAlpha(int iAlpha)
{
	if (!X11Helper::SetOpacity(GetHWnd(), iAlpha))
		return ERR(tr("Failed to set the window opacity."));
	// Without a compositing manager the property is set but has no visible
	// effect, which is not an error worth reporting.
	return OK;
}

STATUS CLinuxWnd::BringToFront()
{
	if (!X11Helper::Activate(GetHWnd()))
		return ERR(tr("Failed to activate the window."));
	return OK;
}

STATUS CLinuxWnd::Highlight()
{
	if (!X11Helper::Highlight(GetHWnd()))
		return ERR(tr("Failed to highlight the window."));
	return OK;
}

bool CLinuxWnd::IsNormal() const
{
	QReadLocker Locker(&m_Mutex);
	return !m_IsMinimized && !m_IsMaximized;
}

STATUS CLinuxWnd::Restore()
{
	// Un-minimize, then drop out of maximized if it was maximized.
	if (IsMinimized())
	{
		if (!X11Helper::SetMinimized(GetHWnd(), false))
			return ERR(tr("Failed to restore the window."));
		return OK;
	}

	if (!X11Helper::SetMaximized(GetHWnd(), false))
		return ERR(tr("Failed to restore the window."));
	return OK;
}

bool CLinuxWnd::IsMinimized() const
{
	QReadLocker Locker(&m_Mutex);
	return m_IsMinimized;
}

STATUS CLinuxWnd::Minimize()
{
	if (!X11Helper::SetMinimized(GetHWnd(), true))
		return ERR(tr("Failed to minimize the window."));
	return OK;
}

bool CLinuxWnd::IsMaximized() const
{
	QReadLocker Locker(&m_Mutex);
	return m_IsMaximized;
}

STATUS CLinuxWnd::Maximize()
{
	if (!X11Helper::SetMaximized(GetHWnd(), true))
		return ERR(tr("Failed to maximize the window."));
	return OK;
}

STATUS CLinuxWnd::Close()
{
	if (!X11Helper::Close(GetHWnd()))
		return ERR(tr("Failed to close the window."));
	return OK;
}
