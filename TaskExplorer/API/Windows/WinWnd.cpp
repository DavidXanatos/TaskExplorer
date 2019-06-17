#include "stdafx.h"
#include "../../GUI/TaskExplorer.h"
#include "WinWnd.h"
#include "ProcessHacker.h"


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
