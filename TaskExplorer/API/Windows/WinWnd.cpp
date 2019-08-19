#include "stdafx.h"
#include "ProcessHacker.h"
#include "../../GUI/TaskExplorer.h"
#include "WinWnd.h"

#define NOSHLWAPI
#include <shellapi.h>
#include <propsys.h>
#include <propvarutil.h>

CWinWnd::CWinWnd(QObject *parent) 
	: CWndInfo(parent) 
{
}

CWinWnd::~CWinWnd()
{
}

bool CWinWnd::InitStaticData(quint64 ProcessId, quint64 ThreadId, quint64 hWnd, void* QueryHandle)
{
	QWriteLocker Locker(&m_Mutex);

	HWND WindowHandle = (HWND)hWnd;

	m_hWnd = hWnd;
	m_ParentWnd = (quint64)::GetParent(WindowHandle);
	m_ProcessId = ProcessId;
	m_ThreadId = ThreadId;

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
		if (m_ShowCommand != placement.showCmd)
		{
			modified = TRUE;

			m_ShowCommand = placement.showCmd;
		}
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
	if (m_ShowCommand == SW_MINIMIZE || m_ShowCommand == SW_SHOWMINIMIZED)
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

bool CWinWnd::IsNormal() const
{
	QReadLocker Locker(&m_Mutex);
	return m_ShowCommand == SW_NORMAL;
}

STATUS CWinWnd::Restore()
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, SW_RESTORE);
	return OK;
}

bool CWinWnd::IsMinimized() const
{
	QReadLocker Locker(&m_Mutex);
	return m_ShowCommand == SW_MINIMIZE || m_ShowCommand == SW_SHOWMINIMIZED;
}

STATUS CWinWnd::Minimize()
{
	QWriteLocker Locker(&m_Mutex);
	ShowWindowAsync((HWND)m_hWnd, SW_MINIMIZE);
	return OK;
}

bool CWinWnd::IsMaximized() const
{
	QReadLocker Locker(&m_Mutex);
	return m_ShowCommand == SW_MAXIMIZE;
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

	// todo:
	/*
    // Initialization code
	HRESULT result = -1;
	if(QThread::currentThread() != theAPI->thread())
		result = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);

    IPropertyStore *propstore;
    ULONG count;
    ULONG i;

    if (SUCCEEDED(SHGetPropertyStoreForWindow(WindowHandle, *(_GUID*)&IID_IPropertyStore, (PVOID*)&propstore)))
    {
        if (SUCCEEDED(IPropertyStore_GetCount(propstore, &count)))
        {
            for (i = 0; i < count; i++)
            {
                PROPERTYKEY propkey;

                if (SUCCEEDED(IPropertyStore_GetAt(propstore, i, &propkey)))
                {
                    INT lvItemIndex;
                    PROPVARIANT propKeyVariant = { 0 };
                    PWSTR propKeyName;

                    if (SUCCEEDED(PSGetNameFromPropertyKey(propkey, &propKeyName)))
                    {
						; //lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, propKeyName, NULL);

                        CoTaskMemFree(propKeyName);
                    }
                    else
                    {
                        WCHAR propKeyString[PKEYSTR_MAX];

                        if (SUCCEEDED(PSStringFromPropertyKey(propkey, propKeyString, RTL_NUMBER_OF(propKeyString))))
                            ; //lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, propKeyString, NULL);
                        else
                            ; //lvItemIndex = PhAddListViewItem(ListViewHandle, MAXINT, L"Unknown", NULL);
						int x = 0;
                    }

                    if (SUCCEEDED(IPropertyStore_GetValue(propstore, propkey, &propKeyVariant)))
                    {
                        if (SUCCEEDED(PSFormatForDisplayAlloc(propkey, propKeyVariant, PDFF_DEFAULT, &propKeyName)))
                        {
                            ; //PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, propKeyName);
                            CoTaskMemFree(propKeyName);
                        }

                        //if (SUCCEEDED(PropVariantToStringAlloc(&propKeyVariant, &propKeyName)))
                        //{
                        //    PhSetListViewSubItem(ListViewHandle, lvItemIndex, 1, propKeyName);
                        //    CoTaskMemFree(propKeyName);
                        //}

                        PropVariantClear(&propKeyVariant);
                    }
                }
            }
        }

        IPropertyStore_Release(propstore);
    }

    // De-initialization code
    if (result == S_OK || result == S_FALSE)
        CoUninitialize();
	*/

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

// Undocumented windows API from win32u.dll
// https://stackoverflow.com/questions/38205375/enumwindows-function-in-win10-enumerates-only-desktop-apps
//

typedef NTSTATUS (WINAPI *NtUserBuildHwndList) (
	HDESK in_hDesk, 
	HWND  in_hWndNext, 
	BOOL  in_EnumChildren, 
	BOOL  in_RemoveImmersive, 
	DWORD in_ThreadID, 
	UINT  in_Max, 
	HWND *out_List, 
	UINT *out_Cnt
);

HWND* BuildWindowList
(
	HDESK in_hDesk,
	HWND  in_hWnd,
	BOOL  in_RemoveImmersive,
	UINT  in_ThreadID,
	INT  *out_Cnt
)
{
	UINT  lv_Max;
	UINT  lv_Cnt;
	UINT  lv_NtStatus;
	HWND *lv_List;

	static NtUserBuildHwndList lib_NtUserBuildHwndListW10 = NULL;
	static PH_INITONCE initOnce = PH_INITONCE_INIT;
	if (PhBeginInitOnce(&initOnce))
	{
		lib_NtUserBuildHwndListW10 = (NtUserBuildHwndList)PhGetModuleProcAddress(L"win32u.dll", "NtUserBuildHwndList");

		PhEndInitOnce(&initOnce);
	}

	// is api not supported?
	if (!lib_NtUserBuildHwndListW10)
		return NULL;

	// initial size of list
	lv_Max = 512;

	// retry to get list
	for (;;)
	{
		// allocate list
		if ((lv_List = (HWND*)malloc(lv_Max * sizeof(HWND))) == NULL)
			break;

		// call the api
		lv_NtStatus = lib_NtUserBuildHwndListW10(
			in_hDesk, in_hWnd,
			in_hWnd ? TRUE : FALSE,
			in_RemoveImmersive,
			in_ThreadID,
			lv_Max, lv_List, &lv_Cnt);

		// success?
		if (lv_NtStatus == NOERROR)
			break;

		// free allocated list
		free(lv_List);

		// clear
		lv_List = NULL;

		// other error then buffersize? or no increase in size?
		if (lv_NtStatus != STATUS_BUFFER_TOO_SMALL || lv_Cnt <= lv_Max)
			break;

		// update max plus some extra to take changes in number of windows into account
		lv_Max = lv_Cnt + 16;
	}

	// return the count
	*out_Cnt = lv_Cnt;

	// return the list, or NULL when failed
	return lv_List;
}

void CWinWnd__EnumAllWindows10(CWinWnd::WNDENUMPROCEX in_Proc, void* in_Param, HWND in_Parent)
{
	INT   lv_Cnt;
	HWND  lv_hWnd;
	HWND  lv_hFirstWnd;
	HWND  lv_hParent;
	HWND *lv_List;

	// first try api to get full window list including immersive/metro apps
	lv_List = BuildWindowList(0, in_Parent, FALSE, 0, &lv_Cnt);

	// success?
	if (lv_List)
	{
		// loop through list
		while (lv_Cnt-- > 0)
		{
			// get handle
			lv_hWnd = lv_List[lv_Cnt];

			// filter out the invalid entry (0x00000001) then call the callback
			if (IsWindow(lv_hWnd))
			{
				// call the callback
				in_Proc((quint64)lv_hWnd, /*in_Parent ? (quint64)GetParent(lv_hWnd) : 0,*/ in_Param);

				// enumerate all child windows
				if (in_Parent == NULL)
					CWinWnd__EnumAllWindows10(in_Proc, in_Param, lv_hWnd);
			}
		}

		// free the list
		free(lv_List);
	}
	else
	{
		// get desktop window, this is equivalent to specifying NULL as hwndParent
		lv_hParent = in_Parent ? in_Parent : GetDesktopWindow();

		// fallback to using FindWindowEx, get first top-level window
		lv_hFirstWnd = FindWindowEx(lv_hParent, 0, 0, 0);

		// init the enumeration
		lv_Cnt = 0;
		lv_hWnd = lv_hFirstWnd;

		// loop through windows found
		// - since 2012 the EnumWindows API in windows has a problem (on purpose by MS)
		//   that it does not return all windows (no metro apps, no start menu etc)
		// - luckally the FindWindowEx() still is clean and working
		while (lv_hWnd)
		{
			// call the callback
			in_Proc((quint64)lv_hWnd, /*(quint64)in_Parent,*/ in_Param);
			
			// enumerate child windows
			CWinWnd__EnumAllWindows10(in_Proc, in_Param, lv_hWnd);

			// get next window
			lv_hWnd = FindWindowEx(lv_hParent, lv_hWnd, 0, 0);

			// protect against changes in window hierachy during enumeration
			if (lv_hWnd == lv_hFirstWnd || lv_Cnt++ > 10000)
				break;
		}
	}
}

struct CWinWnd__WndEnumStruct
{
	CWinWnd::WNDENUMPROCEX Proc;
	void* Param;
	bool Child;
};

BOOL NTAPI CWinWnd__WndEnumProc(HWND hWnd, LPARAM lParam)
{
	CWinWnd__WndEnumStruct* pContext = (CWinWnd__WndEnumStruct*)lParam;
	if (!pContext->Child)
	{
		pContext->Proc((quint64)hWnd, /*0,*/ pContext->Param);

		CWinWnd__WndEnumStruct Context = *pContext;
		Context.Child = true;

		EnumChildWindows(hWnd, CWinWnd__WndEnumProc, (LPARAM)&Context);
	}
	else
		pContext->Proc((quint64)hWnd, /*(quint64)GetParent(hWnd),*/ pContext->Param);
	return TRUE;
}

void CWinWnd::EnumAllWindows(WNDENUMPROCEX in_Proc, void* in_Param)
{
	// on windows prior to windows 8 we can use the regular API
	if (WindowsVersion < WINDOWS_8)
	{
		CWinWnd__WndEnumStruct Context;
		Context.Proc = in_Proc;
		Context.Param = in_Param;
		Context.Child = false;

		EnumWindows(CWinWnd__WndEnumProc, (LPARAM)&Context);
	}
	else
		CWinWnd__EnumAllWindows10(in_Proc, in_Param, NULL);
}
