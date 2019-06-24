#include "stdafx.h"
#include "TaskService.h"
#include "..\Common\Common.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker/RunAs.h"

#include <shellapi.h>
#include <codecvt>

// Note: we want to restart early before initlaizzing PHlib so we need some custom basic functions 

bool IsElevated()
{
    bool fRet = false;
    HANDLE hToken = NULL;
    if( OpenProcessToken(GetCurrentProcess(),TOKEN_QUERY,&hToken))
	{
        TOKEN_ELEVATION Elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if(GetTokenInformation(hToken, TokenElevation, &Elevation, sizeof( Elevation ), &cbSize)) 
            fRet = Elevation.TokenIsElevated;
    }
    if(hToken) 
        CloseHandle(hToken);
    return fRet;
}

int RunElevated(const wstring& Params, bool bGetCode = false)
{
	wchar_t szPath[MAX_PATH];
	if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
		return -3;
	// Launch itself as admin
	SHELLEXECUTEINFO sei = { sizeof(sei) };
	sei.fMask = SEE_MASK_NOCLOSEPROCESS;
	sei.lpVerb = L"runas";
	sei.lpFile = szPath;
	sei.lpParameters = Params.c_str();
	sei.hwnd = NULL;
	sei.nShow = SW_NORMAL;
	if (!ShellExecuteEx(&sei))
	{
		DWORD dwError = GetLastError();
		if (dwError == ERROR_CANCELLED)
			return -2; // The user refused to allow privileges elevation.
	}
	else
	{
		if (bGetCode)
		{
			WaitForSingleObject(sei.hProcess, 10000);
			DWORD ExitCode = -4;
			BOOL success = GetExitCodeProcess(sei.hProcess, &ExitCode);
			NtClose(sei.hProcess);
			return success ? ExitCode : -4;
		}
		return 0;
	}
	return -1;
}

int RestartElevated(int &argc, char **argv)
{
	wstring Params;
	for (int i = 1; i < argc; i++)
	{
		if (i > 1)
			Params.append(L" ");
		Params.append(L"\"" + wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(argv[i]) + L"\"");
	}
	return RunElevated(Params);
}

// we need to adjust the pipe permissions in order to allow a user process to communicate with a service
#include <aclapi.h>
bool fixLocalServerPermissions(QLocalServer *server)
{
    QString pipeName = server->fullServerName();

    HANDLE h = CreateNamedPipeA(pipeName.toStdString().c_str(), PIPE_ACCESS_DUPLEX | WRITE_DAC,
            PIPE_TYPE_MESSAGE, PIPE_UNLIMITED_INSTANCES, 1024*16, 1024*16, 0, NULL);

    if (h == INVALID_HANDLE_VALUE)
        return false;

    bool status = SetSecurityInfo(h, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, NULL, NULL, NULL, NULL) == ERROR_SUCCESS;
    CloseHandle(h);

    return status;
}
#endif

QMutex CTaskService::m_TempMutex;
QString CTaskService::m_TempName;

 CTaskService::CTaskService(int argc, char **argv, const QString& svcName, int timeout)
	: QtServiceBase(argc, argv, svcName)
{
    setServiceDescription("Task Explorer worker service");
    //setServiceFlags(QtServiceBase::CanBeSuspended);

	m_pServer = NULL;

	m_TimeOut = timeout;
	m_LastActivity = GetCurTick();
	m_TimerId = startTimer(1000);
}

CTaskService::~CTaskService() 
{
	killTimer(m_TimerId);
}

void CTaskService::start()
{
	m_pServer = new QLocalServer(this);
	QObject::connect(m_pServer, SIGNAL(newConnection()), SLOT(receiveConnection()));
	m_pServer->listen(serviceName());
#ifdef WIN32
	fixLocalServerPermissions(m_pServer);
#endif
}

void CTaskService::stop()
{
	//QtServiceBase::stop(); // stop is empty anyways

	if (m_pServer) {
		m_pServer->close();
		m_pServer->deleteLater();
		m_pServer = NULL;
	}
}

void CTaskService::timerEvent(QTimerEvent *e)
{
	if (e->timerId() != m_TimerId) 
	{
		QObject::timerEvent(e);
		return;
	}

	if(m_TimeOut && GetCurTick() - m_LastActivity > m_TimeOut)
		QCoreApplication::instance()->quit();
}

bool CTaskService::SendVariant(QLocalSocket* pSocket, const QVariant& Data, int timeout)
{
	QByteArray Buff;
	QDataStream Stream(&Buff, QIODevice::ReadWrite);
	Stream << Data;

	quint32 len = Buff.size();
	pSocket->write((char*)&len, sizeof(quint32));
	pSocket->write(Buff);
    return pSocket->waitForBytesWritten(timeout);
}

QVariant CTaskService::RecvVariant(QLocalSocket* pSocket, int timeout)
{
	quint32 len = 0;
	if (ReadFromDevice(pSocket, (char*)&len, sizeof(quint32), timeout))
	{
		QByteArray Buff;
		Buff.resize(len);
		if (ReadFromDevice(pSocket, Buff.data(), len, timeout))
		{
			QVariant Data;
			QDataStream Stream(Buff);
			Stream >> Data;
			return Data;
		}
	}
	return QVariant();
}

void CTaskService::receiveConnection()
{
	CScoped<QLocalSocket> pSocket = m_pServer->nextPendingConnection();
    if (!pSocket)
        return;

	QVariant Request = RecvVariant(pSocket, 2000);
	if (!Request.isValid())
		return;
	
	QVariant Response = false;

	m_LastActivity = GetCurTick();
	if(Request.type() == QVariant::Map)
	{
		QVariantMap Data = Request.toMap();
		QString Command = Data["Command"].toString();
		QVariantMap Parameters = Data["Parameters"].toMap();
#ifdef WIN32
		if (Command == "SvcInvokeRunAsService")
			Response = PhSvcApiInvokeRunAsService(Parameters);
		else
			Response = "Unknown Command";
#endif
	}
	else if (Request.toString() == "refresh")
		Response = true;
	
	SendVariant(pSocket, Response, 2000);
    pSocket->waitForDisconnected(1000);
}

QVariant CTaskService::SendCommand(const QString& socketName, const QVariant &Command, int timeout)
{
    QLocalSocket Socket;

    bool Ok = false;
    for(int i = 0; i < 2; i++) 
	{
        Socket.connectToServer(socketName);
        Ok = Socket.waitForConnected(timeout/2);
        if (Ok)
            break;
		QThread::msleep(255);
    }

	if (!Ok || !SendVariant(&Socket, Command, timeout))
		return QVariant(); // invalid variant means communication failed

	return RecvVariant(&Socket, timeout);
}

// Note: we could do that using QtService, but it does not expose the required API directly only through start parameters
//			so we would start a process that creates the service, than we start a process that starts the service and than we start a process that deleted the service, 
//			thats to much starting of processes...

// ToDo: fork the QtService library to enable better controll for our usecase

QString CTaskService::RunService()
{
#ifdef WIN32
	QString ServiceName = TASK_SERVICE_NAME;

	// check if a persistent service is installed and running
	bool isRunning = false;
	bool isInstalled = false;
	SC_HANDLE serviceHandle = PhOpenService((wchar_t*)ServiceName.toStdWString().c_str(), SERVICE_QUERY_STATUS);
	if (serviceHandle)
	{
		isInstalled = true;
		SERVICE_STATUS serviceStatus;
		if (QueryServiceStatus(serviceHandle, &serviceStatus) && serviceStatus.dwCurrentState == SERVICE_RUNNING)
			isRunning = true;
		CloseServiceHandle(serviceHandle);
	}
	if (isRunning)
		return ServiceName;

	// if not, than chack if there is a temporary service reachable
	if (!isInstalled)
	{
		QString TempName = GetTempName();
		if (!TempName.isEmpty())
		{
			if (SendCommand(TempName, "refresh", 500).toBool() == true)
				return TempName;
		}
	}

	// if not, than start a new temporary service instance
	ServiceName += "_" + GetRand64Str();
	if (RunService(ServiceName))
	{
		QMutexLocker Locker(&m_TempMutex);
		m_TempName = ServiceName;
		return ServiceName;
	}
#endif
	return QString();
}

bool CTaskService::RunService(const QString& ServiceName)
{
#ifdef WIN32
	if (!IsElevated())
	{
		int Ret = RunElevated(L"-runsvc " + ServiceName.toStdWString(), true);
		if (Ret == EXIT_SUCCESS);
			return true;
		return false;
	}

	BOOLEAN success = FALSE;

	// Try to start the service if its installed
	SC_HANDLE serviceHandle = PhOpenService((wchar_t*)ServiceName.toStdWString().c_str(), SERVICE_QUERY_STATUS | SERVICE_START);
	if (serviceHandle)
	{
		SERVICE_STATUS serviceStatus;
        if (QueryServiceStatus(serviceHandle, &serviceStatus) && serviceStatus.dwCurrentState == SERVICE_RUNNING)
			success = TRUE;
		else if (StartService(serviceHandle, 0, NULL))
			success = TRUE;

		CloseServiceHandle(serviceHandle);
	}
	//else return NTSTATUS_FROM_WIN32(win32Result);

	// when that fails, try to install a temporary service and start it
	if (!success)
	{
		SC_HANDLE scManagerHandle;
		if (!(scManagerHandle = OpenSCManager(NULL, NULL, SC_MANAGER_CREATE_SERVICE)))
			return false; //PhGetLastWin32ErrorAsNtStatus();

		PPH_STRING applicationFileName;    
		if (!(applicationFileName = PhGetApplicationFileName()))
			return false;

		PPH_STRING commandLine;
#ifdef _DEBUG
		bool bDebug = false;
		if(bDebug)
			commandLine = PhFormatString(L"\"%s\" -svc \"%s\"%s", applicationFileName->Buffer, ServiceName.toStdWString().c_str(), L" -timeout 10000 -dbg_wait");
		else
#endif
			commandLine = PhFormatString(L"\"%s\" -svc \"%s\"%s", applicationFileName->Buffer, ServiceName.toStdWString().c_str(), L" -timeout 5000");

		SC_HANDLE serviceHandle = CreateService(scManagerHandle, ServiceName.toStdWString().c_str(), ServiceName.toStdWString().c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
			SERVICE_ERROR_IGNORE, commandLine->Buffer, NULL, NULL, NULL, L"LocalSystem", L"");
		//ULONG  win32Result = GetLastError();

		PhDereferenceObject(commandLine);
		PhDereferenceObject(applicationFileName);

		if (serviceHandle)
		{
			//PhSetDesktopWinStaAccess();

			if (StartService(serviceHandle, 0, NULL))
				success = TRUE;

			DeleteService(serviceHandle);

			CloseServiceHandle(serviceHandle);
		}
		//else return NTSTATUS_FROM_WIN32(win32Result);
	}

	if (success)
		return true;
#endif
	return false;
}