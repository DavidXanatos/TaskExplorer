#include "stdafx.h"
#include "ProcessHacker.h"
#include "../../GUI/TaskExplorer.h"
#include "WinWnd.h"


CWinWnd::CWinWnd(QObject *parent) 
	: CWndInfo(parent) 
{
}

CWinWnd::~CWinWnd()
{
}

bool CWinWnd::InitStaticData(const CWindowsAPI::SWndInfo& WndInfo, void* QueryHandle)
{
	QWriteLocker Locker(&m_Mutex);

	HWND WindowHandle = (HWND)WndInfo.hwnd;

	m_hWnd = WndInfo.hwnd;
	m_ParentWnd = WndInfo.parent;
	m_ProcessId = WndInfo.processId;
	m_ThreadId = WndInfo.threadId;

    WCHAR WindowClass[64];
    GetClassName(WindowHandle, WindowClass, sizeof(WindowClass) / sizeof(WCHAR));
	m_WindowClass = QString::fromWCharArray(WindowClass);

    if (QueryHandle)
    {
		HANDLE processHandle = (HANDLE)QueryHandle;
        PVOID instanceHandle;
        PPH_STRING fileName;

        if (!(instanceHandle = (PVOID)GetWindowLongPtr(WindowHandle, GWLP_HINSTANCE)))
        {
            instanceHandle = (PVOID)GetClassLongPtr(WindowHandle, GCLP_HMODULE);
        }

        if (instanceHandle)
        {
            if (NT_SUCCESS(PhGetProcessMappedFileName(processHandle, instanceHandle, &fileName)))
            {
                PhMoveReference((PVOID*)&fileName, PhResolveDevicePrefix(fileName));
                PhMoveReference((PVOID*)&fileName, PhGetBaseName(fileName));

				m_ModuleString = CastPhString(fileName);
            }
        }
    }

	return true;
}

bool CWinWnd::UpdateDynamicData()
{
	QWriteLocker Locker(&m_Mutex);

	BOOLEAN modified = FALSE;

	PPH_STRING windowText;
	if (PhGetWindowTextEx((HWND)m_hWnd, PH_GET_WINDOW_TEXT_INTERNAL, &windowText) > 0)
	{
		QString WindowTitle = CastPhString(windowText);
		if (m_WindowTitle != WindowTitle)
		{
			modified = TRUE;

			m_WindowTitle = WindowTitle;
		}
	}

	//m_HasChildren = !!FindWindowEx((HWND)m_hWnd, NULL, NULL, NULL);

	//bool WindowVisible = (GetWindowLong((HWND)m_hWnd, GWL_STYLE) & WS_VISIBLE);
    bool WindowVisible = !!IsWindowVisible((HWND)m_hWnd);
	if (m_WindowVisible != WindowVisible)
	{
		modified = TRUE;

		m_WindowVisible = WindowVisible;
	}

	WINDOWPLACEMENT placement = { sizeof(placement) };
	if (GetWindowPlacement((HWND)m_hWnd, &placement))
	{
		//placement.showCmd // todo: do somethign with this
	}

	bool WindowEnabled = (GetWindowLong((HWND)m_hWnd, GWL_STYLE) & WS_DISABLED) == 0;
	if (m_WindowEnabled != WindowEnabled)
	{
		modified = TRUE;

		m_WindowEnabled = WindowEnabled;
	}

	bool WindowOnTop = (GetWindowLong((HWND)m_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST);
	if (m_WindowOnTop != WindowOnTop)
	{
		modified = TRUE;

		m_WindowOnTop = WindowOnTop;
	}

	BYTE alpha;
	ULONG flags;
    if (!GetLayeredWindowAttributes((HWND)m_hWnd, NULL, &alpha, &flags) || !(flags & LWA_ALPHA))
    {
        alpha = 255;
    }
	if (m_WindowAlpha != alpha)
	{
		modified = TRUE;

		m_WindowAlpha = alpha;
	}

	return modified;
}

STATUS CWinWnd::SetVisible(bool bSet)
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, bSet ? SW_SHOW : SW_HIDE);
	return OK;
}

STATUS CWinWnd::SetEnabled(bool bSet)
{
	QWriteLocker Locker(&m_Mutex);
	EnableWindow((HWND)m_hWnd, bSet);
	return OK;
}

STATUS CWinWnd::SetAlwaysOnTop(bool bSet)
{
	QWriteLocker Locker(&m_Mutex);
    LOGICAL topMost;
    topMost = GetWindowLong((HWND)m_hWnd, GWL_EXSTYLE) & WS_EX_TOPMOST;
    SetWindowPos((HWND)m_hWnd, bSet ? HWND_NOTOPMOST : HWND_TOPMOST,
        0, 0, 0, 0, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
	return OK;
}

STATUS CWinWnd::SetWindowAlpha(int iAlpha)
{
	QWriteLocker Locker(&m_Mutex);

    if (iAlpha == 255)
    {
        // Remove the WS_EX_LAYERED bit since it is not needed.
        PhSetWindowExStyle((HWND)m_hWnd, WS_EX_LAYERED, 0);
        RedrawWindow((HWND)m_hWnd, NULL, NULL, RDW_ERASE | RDW_INVALIDATE | RDW_FRAME | RDW_ALLCHILDREN);
    }
    else
    {
        // Add the WS_EX_LAYERED bit so opacity will work.
        PhSetWindowExStyle((HWND)m_hWnd, WS_EX_LAYERED, WS_EX_LAYERED);
        SetLayeredWindowAttributes((HWND)m_hWnd, 0, (BYTE)iAlpha, LWA_ALPHA);
    }
	return OK;
}


STATUS CWinWnd::BringToFront()
{
	QWriteLocker Locker(&m_Mutex);

	WINDOWPLACEMENT placement = { sizeof(placement) };

	GetWindowPlacement((HWND)m_hWnd, &placement);

	if (placement.showCmd == SW_MINIMIZE)
		ShowWindowAsync((HWND)m_hWnd, SW_RESTORE);
	else
		SetForegroundWindow((HWND)m_hWnd);

	return OK;
}

void WndHighlightFunc(HWND hWnd, int Counter) 
{
	WeInvertWindowBorder(hWnd);

	// Note the initial value of Counter must be a multiple of 2 or the frame will remain visible
	if (--Counter > 0)
	{
		QTimer::singleShot(100, [hWnd, Counter]() {
			WndHighlightFunc(hWnd, Counter);
		});
	}
};

STATUS CWinWnd::Highlight()
{
	QWriteLocker Locker(&m_Mutex);
	WndHighlightFunc((HWND)m_hWnd, 10);
	return OK;
}

STATUS CWinWnd::Restore()
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, SW_RESTORE);
	return OK;
}

STATUS CWinWnd::Minimize()
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, SW_MINIMIZE);
	return OK;
}

STATUS CWinWnd::Maximize()
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, SW_MAXIMIZE);
	return OK;
}

STATUS CWinWnd::Close()
{
	QWriteLocker Locker(&m_Mutex);
	PostMessage((HWND)m_hWnd, WM_CLOSE, 0, 0);
	return OK;
}

QRect WinRect2Q(RECT rect)
{
	return QRect(rect.left, rect.top, rect.right - rect.left, rect.bottom - rect.top);
}

typedef struct _STRING_INTEGER_PAIR
{
    PWSTR String;
    ULONG Integer;
} STRING_INTEGER_PAIR, *PSTRING_INTEGER_PAIR;

#define DEFINE_PAIR(Symbol) { L#Symbol, Symbol }

static STRING_INTEGER_PAIR WepStylePairs[] =
{
    DEFINE_PAIR(WS_POPUP),
    DEFINE_PAIR(WS_CHILD),
    DEFINE_PAIR(WS_MINIMIZE),
    DEFINE_PAIR(WS_VISIBLE),
    DEFINE_PAIR(WS_DISABLED),
    DEFINE_PAIR(WS_CLIPSIBLINGS),
    DEFINE_PAIR(WS_CLIPCHILDREN),
    DEFINE_PAIR(WS_MAXIMIZE),
    DEFINE_PAIR(WS_BORDER),
    DEFINE_PAIR(WS_DLGFRAME),
    DEFINE_PAIR(WS_VSCROLL),
    DEFINE_PAIR(WS_HSCROLL),
    DEFINE_PAIR(WS_SYSMENU),
    DEFINE_PAIR(WS_THICKFRAME),
    DEFINE_PAIR(WS_GROUP),
    DEFINE_PAIR(WS_TABSTOP),
    DEFINE_PAIR(WS_MINIMIZEBOX),
    DEFINE_PAIR(WS_MAXIMIZEBOX)
};

static STRING_INTEGER_PAIR WepExtendedStylePairs[] =
{
    DEFINE_PAIR(WS_EX_DLGMODALFRAME),
    DEFINE_PAIR(WS_EX_NOPARENTNOTIFY),
    DEFINE_PAIR(WS_EX_TOPMOST),
    DEFINE_PAIR(WS_EX_ACCEPTFILES),
    DEFINE_PAIR(WS_EX_TRANSPARENT),
    DEFINE_PAIR(WS_EX_MDICHILD),
    DEFINE_PAIR(WS_EX_TOOLWINDOW),
    DEFINE_PAIR(WS_EX_WINDOWEDGE),
    DEFINE_PAIR(WS_EX_CLIENTEDGE),
    DEFINE_PAIR(WS_EX_CONTEXTHELP),
    DEFINE_PAIR(WS_EX_RIGHT),
    DEFINE_PAIR(WS_EX_RTLREADING),
    DEFINE_PAIR(WS_EX_LEFTSCROLLBAR),
    DEFINE_PAIR(WS_EX_CONTROLPARENT),
    DEFINE_PAIR(WS_EX_STATICEDGE),
    DEFINE_PAIR(WS_EX_APPWINDOW),
    DEFINE_PAIR(WS_EX_LAYERED),
    DEFINE_PAIR(WS_EX_NOINHERITLAYOUT),
    DEFINE_PAIR(WS_EX_LAYOUTRTL),
    DEFINE_PAIR(WS_EX_COMPOSITED),
    DEFINE_PAIR(WS_EX_NOACTIVATE)
};

static STRING_INTEGER_PAIR WepClassStylePairs[] =
{
    DEFINE_PAIR(CS_VREDRAW),
    DEFINE_PAIR(CS_HREDRAW),
    DEFINE_PAIR(CS_DBLCLKS),
    DEFINE_PAIR(CS_OWNDC),
    DEFINE_PAIR(CS_CLASSDC),
    DEFINE_PAIR(CS_PARENTDC),
    DEFINE_PAIR(CS_NOCLOSE),
    DEFINE_PAIR(CS_SAVEBITS),
    DEFINE_PAIR(CS_BYTEALIGNCLIENT),
    DEFINE_PAIR(CS_BYTEALIGNWINDOW),
    DEFINE_PAIR(CS_GLOBALCLASS),
    DEFINE_PAIR(CS_IME),
    DEFINE_PAIR(CS_DROPSHADOW)
};

BOOL CALLBACK EnumPropsExCallback(_In_ HWND hwnd, _In_ PWSTR lpszString, _In_ HANDLE hData, _In_ ULONG_PTR dwData)
{
	CWinWnd::SWndInfo& WndInfo = *(CWinWnd::SWndInfo*)dwData;

	QString Key;
	if ((ULONG_PTR)lpszString < USHRT_MAX) // This is an integer atom.
		Key = CWinWnd::tr("#%1").arg((ULONG_PTR)lpszString);
	else
		Key = QString::fromWCharArray(lpszString);
    
	WndInfo.Properties.insert(Key, QString::number((quint64)hData, 0, 16));
    
    return TRUE;
}

CWinWnd::SWndInfo CWinWnd::GetWndInfo() const
{
	SWndInfo WndInfo;

	HWND WindowHandle = (HWND)GetHWnd();

	quint64 ProcessId = GetProcessId();
	quint64 ThreadId = GetThreadId();

	///////////////////////////////////////////////////////////////////////////////////////////////
	// General

	PPH_STRING appIdText;
	if (PhAppResolverGetAppIdForWindow(WindowHandle, &appIdText))
    {
		WndInfo.AppID = CastPhString(appIdText);
    }

	PPH_STRING windowText;
	PhGetWindowTextEx(WindowHandle, 0, &windowText);
	WndInfo.Text = CastPhString(windowText);

	CThreadPtr pThread = theAPI->GetThreadByID(ThreadId);
	WndInfo.Thread = tr("%1 (%2): %3").arg(QString(pThread ? pThread->GetName() : tr("unknown"))).arg(ProcessId).arg(ThreadId);
	
    WINDOWINFO windowInfo = { sizeof(WINDOWINFO) };
    WINDOWPLACEMENT windowPlacement = { sizeof(WINDOWPLACEMENT) };
    MONITORINFO monitorInfo = { sizeof(MONITORINFO) };
	if (GetWindowInfo(WindowHandle, &windowInfo))
	{
		WndInfo.Rect = WinRect2Q(windowInfo.rcWindow);
		WndInfo.ClientRect = WinRect2Q(windowInfo.rcClient);


		PH_STRING_BUILDER styleStringBuilder;
		PH_STRING_BUILDER styleExStringBuilder;

        PhInitializeStringBuilder(&styleStringBuilder, 100);
        PhInitializeStringBuilder(&styleExStringBuilder, 100);

        PhAppendFormatStringBuilder(&styleStringBuilder, L"0x%x (", windowInfo.dwStyle);
        PhAppendFormatStringBuilder(&styleExStringBuilder, L"0x%x (", windowInfo.dwExStyle);

        for (ULONG i = 0; i < RTL_NUMBER_OF(WepStylePairs); i++)
        {
            if (windowInfo.dwStyle & WepStylePairs[i].Integer)
            {
                // Skip irrelevant styles.
                if (WepStylePairs[i].Integer == WS_MAXIMIZEBOX ||
                    WepStylePairs[i].Integer == WS_MINIMIZEBOX)
                {
                    if (windowInfo.dwStyle & WS_CHILD)
                        continue;
                }

                if (WepStylePairs[i].Integer == WS_TABSTOP ||
                    WepStylePairs[i].Integer == WS_GROUP)
                {
                    if (!(windowInfo.dwStyle & WS_CHILD))
                        continue;
                }

                PhAppendStringBuilder2(&styleStringBuilder, WepStylePairs[i].String);
                PhAppendStringBuilder2(&styleStringBuilder, L", ");
            }
        }

        if (PhEndsWithString2(styleStringBuilder.String, L", ", FALSE))
        {
            PhRemoveEndStringBuilder(&styleStringBuilder, 2);
            PhAppendCharStringBuilder(&styleStringBuilder, ')');
        }
        else
        {
            PhRemoveEndStringBuilder(&styleStringBuilder, 1);
        }

        for (ULONG i = 0; i < RTL_NUMBER_OF(WepExtendedStylePairs); i++)
        {
            if (windowInfo.dwExStyle & WepExtendedStylePairs[i].Integer)
            {
                PhAppendStringBuilder2(&styleExStringBuilder, WepExtendedStylePairs[i].String);
                PhAppendStringBuilder2(&styleExStringBuilder, L", ");
            }
        }

        if (PhEndsWithString2(styleExStringBuilder.String, L", ", FALSE))
        {
            PhRemoveEndStringBuilder(&styleExStringBuilder, 2);
            PhAppendCharStringBuilder(&styleExStringBuilder, ')');
        }
        else
        {
            PhRemoveEndStringBuilder(&styleExStringBuilder, 1);
        }

		WndInfo.Styles = CastPhString(PhFinalStringBuilderString(&styleStringBuilder), false);
		WndInfo.StylesEx = CastPhString(PhFinalStringBuilderString(&styleExStringBuilder), false);

        PhDeleteStringBuilder(&styleStringBuilder);
        PhDeleteStringBuilder(&styleExStringBuilder);
	}

    if (GetWindowPlacement(WindowHandle, &windowPlacement))
    {
        // The rectangle is in workspace coordinates. Convert the values back to screen coordinates.
        if (GetMonitorInfo(MonitorFromRect(&windowPlacement.rcNormalPosition, MONITOR_DEFAULTTOPRIMARY), &monitorInfo))
        {
            windowPlacement.rcNormalPosition.left += monitorInfo.rcWork.left;
            windowPlacement.rcNormalPosition.top += monitorInfo.rcWork.top;
            windowPlacement.rcNormalPosition.right += monitorInfo.rcWork.left;
            windowPlacement.rcNormalPosition.bottom += monitorInfo.rcWork.top;
        }

		WndInfo.NormalRect = WinRect2Q(windowPlacement.rcNormalPosition);
    }

    WndInfo.MenuHandle = (quint64)GetMenu(WindowHandle);
    WndInfo.InstanceHandle = GetWindowLongPtr(WindowHandle, GWLP_HINSTANCE);
    WndInfo.UserdataHandle = GetWindowLongPtr(WindowHandle, GWLP_USERDATA);
	WndInfo.IsUnicode = IsWindowUnicode(WindowHandle);
    WndInfo.WindowId = GetWindowLongPtr(WindowHandle, GWLP_ID); // dialog controll id

	//WINDOW_PROPERTIES_INDEX_WNDPROC,
    //WINDOW_PROPERTIES_INDEX_DLGPROC,

    
	HFONT fontHandle = (HFONT)SendMessage(WindowHandle, WM_GETFONT, 0, 0);
    if (fontHandle)
    {
        LOGFONT logFont;

		if (GetObject(fontHandle, sizeof(LOGFONT), &logFont))
			WndInfo.Font = QString::fromWCharArray(logFont.lfFaceName);
		else
			WndInfo.Font = tr("N/A");
    }

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Class

    WCHAR className[256];
	if (GetClassName(WindowHandle, className, RTL_NUMBER_OF(className)))
		WndInfo.ClassName = QString::fromWCharArray(className);

	WNDCLASSEX ClassInfo;
	memset(&ClassInfo, 0, sizeof(WNDCLASSEX));
    ClassInfo.cbSize = sizeof(WNDCLASSEX);
    GetClassInfoEx(NULL, className, &ClassInfo);

	WndInfo.Atom = GetClassWord(WindowHandle, GCW_ATOM);
	WndInfo.hIcon = (quint64)ClassInfo.hIcon;
	WndInfo.hIconSm = (quint64)ClassInfo.hIconSm;
	WndInfo.lpszMenuName = (quint64)ClassInfo.lpszMenuName;
	WndInfo.hCursor = (quint64)ClassInfo.hCursor;
	WndInfo.hbrBackground = (quint64)ClassInfo.hbrBackground;
    
    PH_STRING_BUILDER stringBuilder;

    PhInitializeStringBuilder(&stringBuilder, 100);
    PhAppendFormatStringBuilder(&stringBuilder, L"0x%x (", ClassInfo.style);

    for (ULONG i = 0; i < RTL_NUMBER_OF(WepClassStylePairs); i++)
    {
        if (ClassInfo.style & WepClassStylePairs[i].Integer)
        {
            PhAppendStringBuilder2(&stringBuilder, WepClassStylePairs[i].String);
            PhAppendStringBuilder2(&stringBuilder, L", ");
        }
    }

    if (PhEndsWithString2(stringBuilder.String, L", ", FALSE))
    {
        PhRemoveEndStringBuilder(&stringBuilder, 2);
        PhAppendCharStringBuilder(&stringBuilder, ')');
    }
    else
    {
        // No styles. Remove the brackets.
        PhRemoveEndStringBuilder(&stringBuilder, 1);
    }

	WndInfo.StylesClass = CastPhString(PhFinalStringBuilderString(&stringBuilder), false);
    PhDeleteStringBuilder(&stringBuilder);

	WndInfo.InstanceHandle2 = GetClassLongPtr(WindowHandle, GCLP_HMODULE);

	// WINDOW_PROPERTIES_INDEX_CLASS_WNDPROC

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Properties

	EnumPropsEx(WindowHandle, EnumPropsExCallback, (LPARAM)&WndInfo);

	///////////////////////////////////////////////////////////////////////////////////////////////
	// Property Storage

	// todo

	///////////////////////////////////////////////////////////////////////////////////////////////
	//

	HANDLE processHandle;
	if (NT_SUCCESS(PhOpenProcess(&processHandle, PROCESS_QUERY_LIMITED_INFORMATION, (HANDLE)ProcessId)))
	{
		PPH_STRING fileName = NULL;

		if (NT_SUCCESS(PhGetProcessMappedFileName(processHandle, (HANDLE)WndInfo.InstanceHandle, &fileName)))
		{
			PhMoveReference((PVOID*)&fileName, PhResolveDevicePrefix(fileName));
            PhMoveReference((PVOID*)&fileName, PhGetBaseName(fileName));

			WndInfo.InstanceString = CastPhString(fileName);
		}

		if (NT_SUCCESS(PhGetProcessMappedFileName(processHandle, (HANDLE)WndInfo.InstanceHandle2, &fileName)))
		{
			PhMoveReference((PVOID*)&fileName, PhResolveDevicePrefix(fileName));
            PhMoveReference((PVOID*)&fileName, PhGetBaseName(fileName));

			WndInfo.InstanceString2 = CastPhString(fileName);
		}

		NtClose(processHandle);
	}

	return WndInfo;
}