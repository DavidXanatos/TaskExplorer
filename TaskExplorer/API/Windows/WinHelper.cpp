#include "stdafx.h"
#include "WinHelper.h"
#include <QDebug>

#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
#include <QtWin>
#else
#include <windows.h>
#endif

#include <shellapi.h>
#include <Shlwapi.h>
#include <Shlobj.h>


/*QVariantMap ResolveShortcut(const QString& LinkPath)
{
    QVariantMap Link;

    HRESULT hRes = E_FAIL;
    IShellLink* psl = NULL;

    // buffer that receives the null-terminated string
    // for the drive and path
    TCHAR szPath[0x1000];
    // structure that receives the information about the shortcut
    WIN32_FIND_DATA wfd;

    // Get a pointer to the IShellLink interface
    hRes = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (void**)&psl);

    if (SUCCEEDED(hRes))
    {
        // Get a pointer to the IPersistFile interface
        IPersistFile*  ppf     = NULL;
        psl->QueryInterface(IID_IPersistFile, (void **) &ppf);

        // Open the shortcut file and initialize it from its contents
        hRes = ppf->Load(LinkPath.toStdWString().c_str(), STGM_READ);
        if (SUCCEEDED(hRes))
        {
            hRes = psl->Resolve(NULL, SLR_NO_UI | SLR_NOSEARCH | SLR_NOUPDATE);
            if (SUCCEEDED(hRes))
            {
                // Get the path to the shortcut target
                hRes = psl->GetPath(szPath, ARRAYSIZE(szPath), &wfd, SLGP_RAWPATH);
                if (hRes == S_OK)
                    Link["Path"] = QString::fromWCharArray(szPath);
                else
                {
                    PIDLIST_ABSOLUTE pidl;
                    hRes = psl->GetIDList(&pidl);

                    if (SUCCEEDED(hRes)) 
                    {
                        LPWSTR url = nullptr;
                        SHGetNameFromIDList(pidl, SIGDN_URL, &url);
                    
                        if (url) 
                        {
                            QUrl Url = QString::fromWCharArray(url);

                            if (Url.isLocalFile())
                                Link["Path"] = Url.path().mid(1).replace("/", "\\");
                            else
                                Link["Path"] = Url.toString();
                        
                            CoTaskMemFree(url);
                        }

                        CoTaskMemFree(pidl);
                    }
                }

                hRes = psl->GetArguments(szPath, ARRAYSIZE(szPath));
                if (!FAILED(hRes))
                    Link["Arguments"] = QString::fromWCharArray(szPath);

                hRes = psl->GetWorkingDirectory(szPath, ARRAYSIZE(szPath));
                if (!FAILED(hRes))
				    Link["WorkingDir"] = QString::fromWCharArray(szPath);

				int IconIndex;
                hRes = psl->GetIconLocation(szPath, ARRAYSIZE(szPath), &IconIndex);
                if (FAILED(hRes))
                    return Link;
				Link["IconPath"] = QString::fromWCharArray(szPath);
				Link["IconIndex"] = IconIndex;

                // Get the description of the target
                hRes = psl->GetDescription(szPath, ARRAYSIZE(szPath));
                if (FAILED(hRes))
                    return Link;
                Link["Info"] = QString::fromWCharArray(szPath);
            }
        }
    }

    return Link;
}*/

QPixmap LoadWindowsIcon(const QString& Path, quint32 Index)
{
	std::wstring path = QString(Path).replace("/", "\\").toStdWString();
	HICON icon = ExtractIconW(NULL, path.c_str(), Index);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QPixmap Icon = QtWin::fromHICON(icon);
#else
	QPixmap Icon = QPixmap::fromImage(QImage::fromHICON(icon));
#endif
	DestroyIcon(icon);
	return Icon;
}

bool PickWindowsIcon(QWidget* pParent, QString& Path, quint32& Index)
{
	wchar_t iconPath[MAX_PATH] = { 0 };
	Path.toWCharArray(iconPath);
	BOOL Ret = PickIconDlg((HWND)pParent->window()->winId(), iconPath, MAX_PATH, (int*)&Index);
	Path = QString::fromWCharArray(iconPath);
	return !!Ret;
}

QIcon LoadWindowsIconEx(const QString &Path, quint32 Index) 
{
    std::wstring path = QString(Path).replace("/", "\\").toStdWString();
    QIcon icon;

    HICON hIconL = NULL;
    HICON hIconS = NULL;
    if (ExtractIconExW(path.c_str(), Index, &hIconL, &hIconS, 1) == 2)
    {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        icon.addPixmap(QtWin::fromHICON(hIconL));
        icon.addPixmap(QtWin::fromHICON(hIconS));
#else
        QImage imgL = QImage::fromHICON(hIconL);
        icon.addPixmap(QPixmap::fromImage(imgL));
        QImage imgS = QImage::fromHICON(hIconS);
        icon.addPixmap(QPixmap::fromImage(imgS));
#endif
        DestroyIcon(hIconL);
        DestroyIcon(hIconS);
    }

    return icon;
}

bool WindowsMoveFile(const QString& From, const QString& To)
{
    std::wstring from = QString(From).replace("/", "\\").toStdWString();
    from.append(L"\0", 1);
    std::wstring to = QString(To).replace("/", "\\").toStdWString();
    to.append(L"\0", 1);

    SHFILEOPSTRUCTW SHFileOp;
    memset(&SHFileOp, 0, sizeof(SHFILEOPSTRUCT));
    SHFileOp.hwnd = NULL;
    SHFileOp.wFunc = To.isEmpty() ? FO_DELETE : FO_MOVE;
    SHFileOp.pFrom = from.c_str();
    SHFileOp.pTo = to.c_str();
    SHFileOp.fFlags = NULL;    

    //The Copying Function
    return SHFileOperationW(&SHFileOp) == 0;
}

bool OpenFileProperties(const QString& Path) 
{
    std::wstring path = Path.toStdWString();

    // Initialize the SHELLEXECUTEINFO structure
    SHELLEXECUTEINFOW sei = { 0 };
    sei.cbSize = sizeof(SHELLEXECUTEINFOW);
    sei.fMask = SEE_MASK_INVOKEIDLIST;
    sei.hwnd = NULL; // Parent window handle, can be NULL
    sei.lpVerb = L"properties"; // Action to perform
    sei.lpFile = path.c_str(); // File path
    sei.nShow = SW_SHOW; // Show the dialog

    // Execute the command
    return ShellExecuteExW(&sei) == 0;
}

bool OpenFileFolder(const QString& Path)
{
    std::wstring command = L"/select,\"" + Path.toStdWString() + L"\"";

    // Execute the command
    return ShellExecuteW(NULL, L"open", L"explorer.exe", command.c_str(), NULL, SW_SHOWNORMAL) == 0;
}

#include <netlistmgr.h>

bool CheckInternet()
{
    bool bRet = false;

    HRESULT hr = CoInitialize(NULL);
    if (SUCCEEDED(hr))
    {
        INetworkListManager* pNetworkListManager = nullptr;
        hr = CoCreateInstance(CLSID_NetworkListManager, NULL, CLSCTX_ALL, IID_INetworkListManager, (void**)&pNetworkListManager);
        if (SUCCEEDED(hr)) 
        {
            NLM_CONNECTIVITY connectivity = NLM_CONNECTIVITY_DISCONNECTED;
            hr = pNetworkListManager->GetConnectivity(&connectivity);
            if (SUCCEEDED(hr)) {
                if (connectivity & NLM_CONNECTIVITY_IPV4_INTERNET || connectivity & NLM_CONNECTIVITY_IPV6_INTERNET) {
                    bRet = true;
                }
            }

            pNetworkListManager->Release();
        }
        CoUninitialize();
    }

    return bRet;
}

QVariantList EnumNICs()
{
    QVariantList NICs;

    ULONG bufferSize = 0;
    GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, NULL, &bufferSize);
    std::vector<byte> buffer;
    buffer.resize(bufferSize * 10 / 8);
    IP_ADAPTER_ADDRESSES* adapters = (IP_ADAPTER_ADDRESSES*)buffer.data();
    if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, NULL, adapters, &bufferSize) == NO_ERROR)
    {
        for (IP_ADAPTER_ADDRESSES* adapter = adapters; adapter != NULL; adapter = adapter->Next)
        {
            QVariantMap Data;
            Data["Adapter"] = QString::fromWCharArray(adapter->FriendlyName);
            Data["Device"] = QString::fromLatin1(adapter->AdapterName);
            Data["Index"] = (quint32)adapter->IfIndex;
            Data["MAC"] = QByteArray((char*)adapter->PhysicalAddress, adapter->PhysicalAddressLength);

            QStringList Ip4;
            QStringList Ip6;
            for (IP_ADAPTER_UNICAST_ADDRESS* unicast = adapter->FirstUnicastAddress; unicast != NULL; unicast = unicast->Next)
            {
                char addrStr[INET6_ADDRSTRLEN] = { 0 };

                if (unicast->Address.lpSockaddr->sa_family == AF_INET) {
                    struct sockaddr_in* sa4 = (struct sockaddr_in*)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET, &(sa4->sin_addr), addrStr, sizeof(addrStr));
                    Ip4.append(QString::fromLatin1(addrStr));
                }
                else if (unicast->Address.lpSockaddr->sa_family == AF_INET6) {
                    struct sockaddr_in6* sa6 = (struct sockaddr_in6*)unicast->Address.lpSockaddr;
                    inet_ntop(AF_INET6, &(sa6->sin6_addr), addrStr, sizeof(addrStr));
                    Ip6.append(QString::fromLatin1(addrStr));
                }
            }
            Data["Ip4"] = Ip4;
            Data["Ip6"] = Ip6;

            NICs.append(Data);
        }
    }

    return NICs;
}





static bool IsPlatformFullScreenMode() 
{
    // SHQueryUserNotificationState is only available for Vista and above.
#if defined(NTDDI_VERSION) && (NTDDI_VERSION >= NTDDI_VISTA)

    OSVERSIONINFO osvi;
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
    if (GetVersionEx(&osvi) && osvi.dwMajorVersion < 6)
        return false;


    typedef HRESULT(WINAPI* SHQueryUserNotificationStatePtr)(
        QUERY_USER_NOTIFICATION_STATE* state);

    HMODULE shell32_base = ::GetModuleHandleW(L"shell32.dll");
    if (!shell32_base) {
        return false;
    }
    SHQueryUserNotificationStatePtr query_user_notification_state_ptr =
        reinterpret_cast<SHQueryUserNotificationStatePtr>
        (::GetProcAddress(shell32_base, "SHQueryUserNotificationState"));
    if (!query_user_notification_state_ptr) {
        return false;
    }

    QUERY_USER_NOTIFICATION_STATE state;
    if (FAILED((*query_user_notification_state_ptr)(&state)))
        return false;
    return state == QUNS_RUNNING_D3D_FULL_SCREEN ||
        state == QUNS_PRESENTATION_MODE;
#else
    return false;
#endif
}

static bool IsFullScreenWindowMode() 
{
    // Get the foreground window which the user is currently working on.
    HWND wnd = ::GetForegroundWindow();
    if (!wnd)
        return false;

    // Get the monitor where the window is located.
    RECT wnd_rect;
    if (!::GetWindowRect(wnd, &wnd_rect))
        return false;
    HMONITOR monitor = ::MonitorFromRect(&wnd_rect, MONITOR_DEFAULTTONULL);
    if (!monitor)
        return false;
    MONITORINFO monitor_info = { sizeof(monitor_info) };
    if (!::GetMonitorInfo(monitor, &monitor_info))
        return false;

    // It should be the main monitor.
    if (!(monitor_info.dwFlags & MONITORINFOF_PRIMARY))
        return false;

    // The window should be at least as large as the monitor.
    if (!::IntersectRect(&wnd_rect, &wnd_rect, &monitor_info.rcMonitor))
        return false;
    if (!::EqualRect(&wnd_rect, &monitor_info.rcMonitor))
        return false;

    // At last, the window style should not have WS_DLGFRAME and WS_THICKFRAME and
    // its extended style should not have WS_EX_WINDOWEDGE and WS_EX_TOOLWINDOW.
    LONG style = ::GetWindowLong(wnd, GWL_STYLE);
    LONG ext_style = ::GetWindowLong(wnd, GWL_EXSTYLE);
    return !((style & (WS_DLGFRAME | WS_THICKFRAME)) ||
        (ext_style & (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW)));
}

static bool IsFullScreenConsoleMode() 
{
    // We detect this by attaching the current process to the console of the
    // foreground window and then checking if it is in full screen mode.
    DWORD pid = 0;
    ::GetWindowThreadProcessId(::GetForegroundWindow(), &pid);
    if (!pid)
        return false;

    if (!::AttachConsole(pid))
        return false;

    DWORD modes = 0;
    ::GetConsoleDisplayMode(&modes);
    ::FreeConsole();

    return (modes & (CONSOLE_FULLSCREEN | CONSOLE_FULLSCREEN_HARDWARE)) != 0;
}

bool IsFullScreenMode()
{
    return IsPlatformFullScreenMode()
        //|| IsFullScreenConsoleMode()
        || IsFullScreenWindowMode();
}