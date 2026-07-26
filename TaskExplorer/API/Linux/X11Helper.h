#pragma once

#include <qobject.h>
#include <QList>
#include <QString>

//
// Minimal EWMH (Extended Window Manager Hints) client.
//
// This is how the window list is obtained: the window manager publishes
// _NET_CLIENT_LIST on the root window, and each managed window carries
// _NET_WM_PID, _NET_WM_NAME and friends. It is the same mechanism wmctrl and
// xdotool use.
//
// Everything here is a no-op when TE_HAVE_X11 is undefined, or when no X
// display can be opened. Under a pure Wayland session there is deliberately no
// protocol for one client to enumerate another's windows, so the window views
// stay empty; under XWayland only X clients are visible.
//
namespace X11Helper
{
	// True when a display connection is available. Everything else returns
	// empty/false when this is false, so callers do not need to branch.
	bool		IsAvailable();

	struct SWindow
	{
		quint64	Window = 0;		// the X window id
		quint64	ParentWindow = 0;
		//
		// _NET_WM_PID, falling back to asking the X server via the X-Resource
		// extension for clients that do not set it. 0 only for a remote client,
		// which has no local pid at all.
		//
		quint64	ProcessId = 0;
		QString	Title;			// _NET_WM_NAME, falling back to WM_NAME
		QString	WindowClass;		// WM_CLASS
		bool	Visible = false;	// mapped, and not _NET_WM_STATE_HIDDEN
		bool	Minimized = false;	// _NET_WM_STATE_HIDDEN
		bool	Maximized = false;	// _NET_WM_STATE_MAXIMIZED_{HORZ,VERT}
		bool	AlwaysOnTop = false;	// _NET_WM_STATE_ABOVE
		int	Alpha = 255;		// from _NET_WM_WINDOW_OPACITY
	};

	// Every window the window manager is managing, in its stacking order.
	QList<SWindow>	EnumWindows();

	// Re-reads the mutable properties of one window. Returns false if the
	// window has gone away.
	bool		QueryWindow(quint64 Window, SWindow& Info);

	// ---- actions ----
	//
	// These go through the window manager via client messages rather than
	// being applied directly, which is what EWMH requires - a compliant WM owns
	// the window's state and would otherwise undo the change.
	//
	bool		Activate(quint64 Window);
	bool		Close(quint64 Window);
	bool		SetMinimized(quint64 Window, bool bSet);
	bool		SetMaximized(quint64 Window, bool bSet);
	bool		SetAlwaysOnTop(quint64 Window, bool bSet);
	bool		SetVisible(quint64 Window, bool bSet);
	bool		SetOpacity(quint64 Window, int Alpha);

	// Draws a transient highlight frame around the window, so the user can see
	// which one a list entry refers to.
	bool		Highlight(quint64 Window);
}
