#include "stdafx.h"
#include "X11Helper.h"

#ifdef TE_HAVE_X11

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

#include <dlfcn.h>

namespace X11Helper
{

static Display* GetDisplay();

//
// The X-Resource extension, version 1.2, declared here and loaded at runtime.
//
// This is the authoritative way to find out which process owns a window: the
// question is put to the X server, which knows the pid behind every local
// client connection, so it works whether or not the application cooperates.
//
// It is used as a fallback for windows with no _NET_WM_PID. That property is a
// convention rather than a requirement, and older toolkits simply do not set
// it - the FOX toolkit (xfe) is one, and such a window would otherwise be
// attributable to no process at all.
//
// libXRes is dlopen'd rather than linked: the runtime library is present on
// essentially every X installation, but the development headers frequently are
// not, and making them a build requirement for a fallback would be a poor
// trade. If it cannot be loaded, or the server is older than 1.2, the fallback
// simply does not engage.
//
struct XResClientIdSpec { XID client; unsigned int mask; };
struct XResClientIdValue { XResClientIdSpec spec; long length; void* value; };

#define XRES_LOCAL_CLIENT_PID_MASK	(1 << 1)

static quint64 QueryPidViaXRes(quint64 WindowId)
{
	struct SXRes
	{
		Bool	(*QueryExtension)(Display*, int*, int*) = nullptr;
		Status	(*QueryVersion)(Display*, int*, int*) = nullptr;
		Status	(*QueryClientIds)(Display*, long, XResClientIdSpec*, long*, XResClientIdValue**) = nullptr;
		pid_t	(*GetClientPid)(XResClientIdValue*) = nullptr;
		void	(*IdsDestroy)(long, XResClientIdValue*) = nullptr;
		bool	bUsable = false;
	};

	// Resolved once; the result, including "not available", is cached.
	static const SXRes XRes = []() -> SXRes {
		SXRes Result;

		Display* pDisplay = GetDisplay();
		if (!pDisplay)
			return Result;

		void* pLib = dlopen("libXRes.so.1", RTLD_LAZY | RTLD_LOCAL);
		if (!pLib)
			return Result;

		*(void**)&Result.QueryExtension = dlsym(pLib, "XResQueryExtension");
		*(void**)&Result.QueryVersion = dlsym(pLib, "XResQueryVersion");
		*(void**)&Result.QueryClientIds = dlsym(pLib, "XResQueryClientIds");
		*(void**)&Result.GetClientPid = dlsym(pLib, "XResGetClientPid");
		*(void**)&Result.IdsDestroy = dlsym(pLib, "XResClientIdsDestroy");

		if (!Result.QueryExtension || !Result.QueryVersion || !Result.QueryClientIds
			|| !Result.GetClientPid || !Result.IdsDestroy)
			return Result;

		int EventBase = 0, ErrorBase = 0;
		if (!Result.QueryExtension(pDisplay, &EventBase, &ErrorBase))
			return Result;

		// QueryClientIds arrived in 1.2; earlier servers only report aggregate
		// resource usage, which cannot answer this question.
		int Major = 0, Minor = 0;
		if (!Result.QueryVersion(pDisplay, &Major, &Minor))
			return Result;
		if (Major < 1 || (Major == 1 && Minor < 2))
			return Result;

		Result.bUsable = true;
		return Result;
	}();

	if (!XRes.bUsable)
		return 0;

	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return 0;

	//
	// Any XID owned by the client identifies it - the server masks the id down
	// to the client's base - so the window itself is a valid specification.
	//
	XResClientIdSpec Spec;
	Spec.client = (XID)WindowId;
	Spec.mask = XRES_LOCAL_CLIENT_PID_MASK;

	long Count = 0;
	XResClientIdValue* pValues = nullptr;
	if (XRes.QueryClientIds(pDisplay, 1, &Spec, &Count, &pValues) != Success)
		return 0;

	quint64 Pid = 0;
	for (long i = 0; i < Count && !Pid; i++)
	{
		const pid_t Value = XRes.GetClientPid(&pValues[i]);
		// A remote client has no local pid, and the server reports -1.
		if (Value > 0)
			Pid = (quint64)Value;
	}

	if (pValues)
		XRes.IdsDestroy(Count, pValues);

	return Pid;
}

//
// A private display connection rather than Qt's.
//
// Qt's connection belongs to the GUI thread and is not safe to use from the
// API worker thread that drives the refresh; opening our own avoids having to
// marshal every property read onto the GUI thread.
//
// Opened once, lazily, and never closed - the process lives as long as the
// connection is useful.
//
static Display* GetDisplay()
{
	static Display* pDisplay = []() -> Display* {
		// Honours $DISPLAY; returns null under a pure Wayland session with no
		// XWayland, which is the correct "no windows visible" answer.
		return XOpenDisplay(nullptr);
	}();
	return pDisplay;
}

static Atom GetAtom(const char* Name)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return None;
	return XInternAtom(pDisplay, Name, False);
}

//
// Reads a window property. The X property API is awkward: it returns data in
// units of 8, 16 or 32 bits, and for 32 bit properties the "32 bit" values are
// actually long, which is 64 bits on LP64. Callers must respect *pFormat.
//
static unsigned char* GetProperty(quint64 WindowId, Atom Property, Atom Type,
                                  unsigned long* pCount, int* pFormat = nullptr)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay || Property == None)
		return nullptr;

	Atom ActualType = None;
	int ActualFormat = 0;
	unsigned long Count = 0;
	unsigned long BytesAfter = 0;
	unsigned char* pData = nullptr;

	// Errors here are routine - windows disappear between enumeration and read.
	if (XGetWindowProperty(pDisplay, (::Window)WindowId, Property, 0, (~0L), False, Type,
	                       &ActualType, &ActualFormat, &Count, &BytesAfter, &pData) != Success)
		return nullptr;

	if (!pData)
		return nullptr;

	if (ActualType == None || Count == 0)
	{
		XFree(pData);
		return nullptr;
	}

	if (pCount)
		*pCount = Count;
	if (pFormat)
		*pFormat = ActualFormat;
	return pData;
}

static QString GetStringProperty(quint64 WindowId, const char* Name)
{
	unsigned long Count = 0;
	// AnyPropertyType, because _NET_WM_NAME is UTF8_STRING while the legacy
	// WM_NAME is STRING.
	unsigned char* pData = GetProperty(WindowId, GetAtom(Name), AnyPropertyType, &Count);
	if (!pData)
		return QString();

	const QString Value = QString::fromUtf8((const char*)pData, (int)Count);
	XFree(pData);
	return Value;
}

static quint64 GetCardinalProperty(quint64 WindowId, const char* Name, bool* pFound = nullptr)
{
	if (pFound)
		*pFound = false;

	unsigned long Count = 0;
	unsigned char* pData = GetProperty(WindowId, GetAtom(Name), XA_CARDINAL, &Count);
	if (!pData)
		return 0;

	// 32 bit properties come back as long, not quint32.
	const quint64 Value = (quint64)(*(unsigned long*)pData);
	XFree(pData);

	if (pFound)
		*pFound = true;
	return Value;
}

static QList<Atom> GetAtomListProperty(quint64 WindowId, const char* Name)
{
	QList<Atom> Atoms;

	unsigned long Count = 0;
	unsigned char* pData = GetProperty(WindowId, GetAtom(Name), XA_ATOM, &Count);
	if (!pData)
		return Atoms;

	const unsigned long* pValues = (const unsigned long*)pData;
	for (unsigned long i = 0; i < Count; i++)
		Atoms.append((Atom)pValues[i]);

	XFree(pData);
	return Atoms;
}

//
// Sends a client message to the root window, which is how EWMH state changes
// are requested. The window manager owns the state; poking the window directly
// would be undone at the next restack.
//
static bool SendRootMessage(quint64 WindowId, const char* MessageType,
                            long Data0 = 0, long Data1 = 0, long Data2 = 0)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	const Atom Type = GetAtom(MessageType);
	if (Type == None)
		return false;

	XEvent Event;
	memset(&Event, 0, sizeof(Event));
	Event.xclient.type = ClientMessage;
	Event.xclient.window = (::Window)WindowId;
	Event.xclient.message_type = Type;
	Event.xclient.format = 32;
	Event.xclient.data.l[0] = Data0;
	Event.xclient.data.l[1] = Data1;
	Event.xclient.data.l[2] = Data2;
	// Source indication: 2 means "a pager or task manager", which window
	// managers honour without the focus-stealing-prevention heuristics they
	// apply to ordinary applications.
	Event.xclient.data.l[3] = 2;

	const bool bOk = XSendEvent(pDisplay, DefaultRootWindow(pDisplay), False,
	                            SubstructureNotifyMask | SubstructureRedirectMask, &Event) != 0;
	XFlush(pDisplay);
	return bOk;
}

bool IsAvailable()
{
	return GetDisplay() != nullptr;
}

static void FillState(quint64 WindowId, SWindow& Info)
{
	const Atom Hidden = GetAtom("_NET_WM_STATE_HIDDEN");
	const Atom MaxHorz = GetAtom("_NET_WM_STATE_MAXIMIZED_HORZ");
	const Atom MaxVert = GetAtom("_NET_WM_STATE_MAXIMIZED_VERT");
	const Atom AtomAbove = GetAtom("_NET_WM_STATE_ABOVE");

	bool bMaxHorz = false, bMaxVert = false;
	for (Atom State : GetAtomListProperty(WindowId, "_NET_WM_STATE"))
	{
		if (State == Hidden)  Info.Minimized = true;
		if (State == MaxHorz) bMaxHorz = true;
		if (State == MaxVert) bMaxVert = true;
		if (State == AtomAbove) Info.AlwaysOnTop = true;
	}
	Info.Maximized = bMaxHorz && bMaxVert;

	//
	// _NET_WM_WINDOW_OPACITY is a 32 bit cardinal where 0xFFFFFFFF is opaque.
	// An absent property also means opaque.
	//
	bool bFound = false;
	const quint64 Opacity = GetCardinalProperty(WindowId, "_NET_WM_WINDOW_OPACITY", &bFound);
	Info.Alpha = bFound ? (int)((Opacity & 0xFFFFFFFFu) * 255 / 0xFFFFFFFFu) : 255;
}

bool QueryWindow(quint64 WindowId, SWindow& Info)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	XWindowAttributes Attributes;
	if (!XGetWindowAttributes(pDisplay, (::Window)WindowId, &Attributes))
		return false; // gone

	Info.Window = WindowId;

	//
	// _NET_WM_PID first, since it costs one property read and nearly every
	// modern toolkit sets it. Ask the X server itself only when the client did
	// not - see QueryPidViaXRes.
	//
	Info.ProcessId = GetCardinalProperty(WindowId, "_NET_WM_PID");
	if (!Info.ProcessId)
		Info.ProcessId = QueryPidViaXRes(WindowId);

	// _NET_WM_NAME is the UTF-8 modern one; WM_NAME is the latin-1 fallback
	// that older toolkits still set.
	Info.Title = GetStringProperty(WindowId, "_NET_WM_NAME");
	if (Info.Title.isEmpty())
		Info.Title = GetStringProperty(WindowId, "WM_NAME");

	//
	// WM_CLASS is two consecutive NUL terminated strings: instance then class.
	// The class is the more useful of the two for identification.
	//
	const QString WmClass = GetStringProperty(WindowId, "WM_CLASS");
	if (!WmClass.isEmpty())
	{
		const QStringList Parts = WmClass.split(QChar('\0'), Qt::SkipEmptyParts);
		Info.WindowClass = Parts.value(Parts.size() > 1 ? 1 : 0);
	}

	FillState(WindowId, Info);

	// IsViewable means mapped and all ancestors mapped. A minimized window is
	// still "managed" but not viewable.
	Info.Visible = (Attributes.map_state == IsViewable) && !Info.Minimized;

	::Window Root = 0, Parent = 0, *pChildren = nullptr;
	unsigned int ChildCount = 0;
	if (XQueryTree(pDisplay, (::Window)WindowId, &Root, &Parent, &pChildren, &ChildCount))
	{
		Info.ParentWindow = (quint64)Parent;
		if (pChildren)
			XFree(pChildren);
	}

	return true;
}

QList<SWindow> EnumWindows()
{
	QList<SWindow> Windows;

	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return Windows;

	//
	// _NET_CLIENT_LIST_STACKING is in stacking order (bottom to top);
	// _NET_CLIENT_LIST is in initial mapping order. Either is fine, but the
	// stacking one is preferred and some window managers only set one.
	//
	unsigned long Count = 0;
	unsigned char* pData = GetProperty((quint64)DefaultRootWindow(pDisplay),
	                                   GetAtom("_NET_CLIENT_LIST_STACKING"), XA_WINDOW, &Count);
	if (!pData)
	{
		pData = GetProperty((quint64)DefaultRootWindow(pDisplay),
		                    GetAtom("_NET_CLIENT_LIST"), XA_WINDOW, &Count);
	}
	if (!pData)
		return Windows; // no EWMH compliant window manager running

	const unsigned long* pWindows = (const unsigned long*)pData;
	for (unsigned long i = 0; i < Count; i++)
	{
		SWindow Info;
		if (QueryWindow((quint64)pWindows[i], Info))
			Windows.append(Info);
	}

	XFree(pData);
	return Windows;
}

bool Activate(quint64 WindowId)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	// Raising as well as activating, because a WM that declines the activation
	// may still honour the restack.
	XRaiseWindow(pDisplay, (::Window)WindowId);
	return SendRootMessage(WindowId, "_NET_ACTIVE_WINDOW", 2, CurrentTime, 0);
}

bool Close(quint64 WindowId)
{
	// Asks the window manager to close it, which lets the application run its
	// own shutdown path rather than having the connection killed.
	return SendRootMessage(WindowId, "_NET_CLOSE_WINDOW", CurrentTime, 2, 0);
}

//
// _NET_WM_STATE actions: 0 remove, 1 add, 2 toggle.
//
static bool SetState(quint64 WindowId, bool bSet, const char* State1, const char* State2 = nullptr)
{
	const Atom Atom1 = GetAtom(State1);
	const Atom Atom2 = State2 ? GetAtom(State2) : None;
	if (Atom1 == None)
		return false;

	return SendRootMessage(WindowId, "_NET_WM_STATE", bSet ? 1 : 0, (long)Atom1, (long)Atom2);
}

bool SetMinimized(quint64 WindowId, bool bSet)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	if (bSet)
	{
		//
		// There is no _NET_WM_STATE_HIDDEN request - the hint is set by the
		// window manager to report state, not to change it. Iconifying is the
		// supported way to minimize.
		//
		return XIconifyWindow(pDisplay, (::Window)WindowId, DefaultScreen(pDisplay)) != 0;
	}

	// Restoring is just activating it again.
	return Activate(WindowId);
}

bool SetMaximized(quint64 WindowId, bool bSet)
{
	return SetState(WindowId, bSet, "_NET_WM_STATE_MAXIMIZED_HORZ", "_NET_WM_STATE_MAXIMIZED_VERT");
}

bool SetAlwaysOnTop(quint64 WindowId, bool bSet)
{
	return SetState(WindowId, bSet, "_NET_WM_STATE_ABOVE");
}

bool SetVisible(quint64 WindowId, bool bSet)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	//
	// Unmapping a window behind the window manager's back is not really
	// supported by EWMH, but it is what a task manager's "hide" does and the
	// window can be mapped again.
	//
	if (bSet)
		XMapWindow(pDisplay, (::Window)WindowId);
	else
		XUnmapWindow(pDisplay, (::Window)WindowId);

	XFlush(pDisplay);
	return true;
}

bool SetOpacity(quint64 WindowId, int Alpha)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	const Atom Property = GetAtom("_NET_WM_WINDOW_OPACITY");
	if (Property == None)
		return false;

	//
	// Requires a compositing manager to have any visible effect; without one
	// the property is set and simply ignored.
	//
	const unsigned long Opacity = (unsigned long)((double)qBound(0, Alpha, 255) / 255.0 * 0xFFFFFFFFu);
	XChangeProperty(pDisplay, (::Window)WindowId, Property, XA_CARDINAL, 32, PropModeReplace,
	                (unsigned char*)&Opacity, 1);
	XFlush(pDisplay);
	return true;
}

bool Highlight(quint64 WindowId)
{
	Display* pDisplay = GetDisplay();
	if (!pDisplay)
		return false;

	XWindowAttributes Attributes;
	if (!XGetWindowAttributes(pDisplay, (::Window)WindowId, &Attributes))
		return false;

	//
	// Draws directly on the root window with an inverting GC, twice, so the
	// frame appears and is then erased - the same trick Windows' own window
	// finder uses. Nothing is left behind.
	//
	::Window Root = DefaultRootWindow(pDisplay);
	int X = 0, Y = 0;
	::Window Child = 0;
	XTranslateCoordinates(pDisplay, (::Window)WindowId, Root, 0, 0, &X, &Y, &Child);

	XGCValues Values;
	Values.function = GXinvert;
	Values.subwindow_mode = IncludeInferiors;
	Values.line_width = 4;
	GC Gc = XCreateGC(pDisplay, Root, GCFunction | GCSubwindowMode | GCLineWidth, &Values);
	if (!Gc)
		return false;

	for (int i = 0; i < 2; i++)
	{
		XDrawRectangle(pDisplay, Root, Gc, X, Y, Attributes.width, Attributes.height);
		XFlush(pDisplay);
		QThread::msleep(120);
	}

	XFreeGC(pDisplay, Gc);
	XFlush(pDisplay);
	return true;
}

} // namespace X11Helper

#else // !TE_HAVE_X11

//
// Built without X11. Every entry point reports "nothing available", which is
// the same answer a Wayland session gives at runtime.
//
namespace X11Helper
{
	bool IsAvailable() { return false; }
	QList<SWindow> EnumWindows() { return QList<SWindow>(); }
	bool QueryWindow(quint64, SWindow&) { return false; }
	bool Activate(quint64) { return false; }
	bool Close(quint64) { return false; }
	bool SetMinimized(quint64, bool) { return false; }
	bool SetMaximized(quint64, bool) { return false; }
	bool SetAlwaysOnTop(quint64, bool) { return false; }
	bool SetVisible(quint64, bool) { return false; }
	bool SetOpacity(quint64, int) { return false; }
	bool Highlight(quint64) { return false; }
}

#endif // TE_HAVE_X11
