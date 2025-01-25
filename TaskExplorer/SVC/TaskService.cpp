#include "stdafx.h"
#include "TaskService.h"
#include "../../MiscHelpers/Common/Common.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "../API/SystemAPI.h"
#ifdef WIN32
#include "../API/Windows/ProcessHacker/RunAs.h"
#include "../API/Windows/WinDumper.h"
#include "../API/Windows/SymbolProvider.h"
#include "../API/Windows/WinAdmin.h"
#include "../API/Windows/WinProcess.h"
#include "../API/Windows/WinThread.h"
#include "../API/Windows/WinSocket.h"

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

QMutex CTaskService::m_Mutex;
QString CTaskService::m_TempName;
QString CTaskService::m_TempSocket;
#ifdef WIN64
QString CTaskService::m_TempSocket32;
#endif

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
//#ifdef WIN32
//	InitPH(true);
//#endif
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
	
	m_LastActivity = GetCurTick();

	QString Command;
	QVariantMap Parameters;
	if (Request.type() == QVariant::Map)
	{
		QVariantMap Data = Request.toMap();
		Command = Data["Command"].toString();
		Parameters = Data["Parameters"].toMap();
	}
	else
		Command = Request.toString();
	

	QVariant Response = false;	
#ifdef WIN32
	if (Command == "RunAsService")
	{
		Response = SvcApiInvokeRunAsService(Parameters);
	}
	else if (Command == "GetProcessId")
	{
		Response = (quint64)NtCurrentProcessId();
	}
	else if (Command == "WriteMiniDumpProcess")
	{
		Response = SvcApiWriteMiniDumpProcess(Parameters);
	}
	else if (Command == "PredictAddressesFromClrData")
	{
		Response = SvcApiPredictAddressesFromClrData(Parameters);
	}
	else if (Command == "GetRuntimeNameByAddressClrProcess")
	{
		Response = SvcApiGetRuntimeNameByAddressClrProcess(Parameters);
	}
	else if (Command == "GetClrThreadAppDomain")
	{
		Response = SvcGetClrThreadAppDomain(Parameters);
	}
	else if (Command == "GetProcessUnloadedDlls")
	{
		Response = GetProcessUnloadedDlls(Parameters["ProcessId"].toULongLong());
	}
	else if (Command == "GetProcessHeaps")
	{
		Response = GetProcessHeaps(Parameters["ProcessId"].toULongLong());
	}
	else if (Command == "ExecTaskAction")
	{
		if(quint64 ThreadId = Parameters["ThreadId"].toULongLong())
			Response = ExecTaskAction(Parameters["ProcessId"].toULongLong(), ThreadId, Parameters["Action"].toString(), Parameters["Data"]);
		else
			Response = ExecTaskAction(Parameters["ProcessId"].toULongLong(), Parameters["Action"].toString(), Parameters["Data"]);
	}
	else if (Command == "ExecServiceAction")
	{
		Response = ExecServiceAction(Parameters["Name"].toString(), Parameters["Action"].toString(), Parameters["Data"]);
	}
	else if (Command == "ChangeServiceConfig")
	{
		QString ServiceName = Parameters["ServiceName"].toString();

		ULONG win32Result = 0;
		SC_HANDLE serviceHandle;
		if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_CHANGE_CONFIG, (wchar_t*)ServiceName.toStdWString().c_str())))
		{
			if (Parameters.contains("InfoLevel") && Parameters.contains("InfoData"))
			{
				DWORD InfoLevel = Parameters["InfoLevel"].toUInt();
				QByteArray Info = Parameters["InfoData"].toByteArray();

				if (!ChangeServiceConfig2(serviceHandle, InfoLevel, Info.data()))
				{
					win32Result = GetLastError();
				}
			}
			else if(Parameters.contains("ServiceType") && Parameters.contains("StartType") && Parameters.contains("ErrorControl"))
			{
				DWORD newServiceType = Parameters["ServiceType"].toULongLong();
				DWORD newStartType = Parameters["StartType"].toULongLong();
				DWORD newErrorControl = Parameters["ErrorControl"].toULongLong();

				std::wstring BinaryPathName = Parameters.value("BinaryPathName").toString().toStdWString();
				std::wstring LoadOrderGroup = Parameters.value("LoadOrderGroup").toString().toStdWString();
				
				DWORD newTagId = Parameters.value("LocalAdTagIddress").toUInt();
				
				std::wstring Dependencies;
				foreach(const QString& service, Parameters.value("Dependencies").toStringList())
				{
					Dependencies.append(service.toStdWString());
					Dependencies.push_back(L'\0');
				}
				Dependencies.push_back(L'\0');
				
				std::wstring ServiceStartName = Parameters.value("ServiceStartName").toString().toStdWString();
				std::wstring Password = Parameters.value("Password").toString().toStdWString();
				std::wstring DisplayName = Parameters.value("DisplayName").toString().toStdWString();


				if (!ChangeServiceConfig(
					serviceHandle,
					newServiceType,
					newStartType,
					newErrorControl,
					Parameters.contains("BinaryPathName") ? BinaryPathName.c_str() : NULL,
					Parameters.contains("LoadOrderGroup") ? LoadOrderGroup.c_str() : NULL,
					Parameters.contains("LocalAdTagIddress") ? &newTagId : NULL,
					Parameters.contains("Dependencies") ? Dependencies.c_str() : NULL,
					Parameters.contains("ServiceStartName") ? ServiceStartName.c_str() : NULL,
					Parameters.contains("Password") ? Password.c_str() : NULL,
					Parameters.contains("DisplayName") ? DisplayName.c_str() : NULL
				))
				{
					win32Result = GetLastError();
				}
			}
			else
			{
				win32Result = ERROR_INVALID_PARAMETER;
			}

			CloseServiceHandle(serviceHandle);
		}
		else
			win32Result = (quint32)GetLastError();

		Response = (quint32)win32Result;
	}
	else if (Command == "CloseSocket")
	{
		Response = SvcApiCloseSocket(Parameters);
	}
	else if (Command == "SendMessage")
	{
		HWND hWnd = (HWND)Parameters["hWnd"].toULongLong();
		UINT Msg = Parameters["Msg"].toULongLong();
		WPARAM wParam = Parameters["wParam"].toULongLong();
		LPARAM lParam = Parameters["lParam"].toULongLong();

		if (Parameters["Post"].toBool())
			Response = PostMessage(hWnd, Msg, wParam, lParam);
		else
			Response = SendMessage(hWnd, Msg, wParam, lParam);
	}
	else if (Command == "FreeMemory")
	{
		SYSTEM_MEMORY_LIST_COMMAND command = (SYSTEM_MEMORY_LIST_COMMAND)Parameters["Command"].toInt();
		Response = NtSetSystemInformation(SystemMemoryListInformation, &command, sizeof(SYSTEM_MEMORY_LIST_COMMAND));
	}
	else
#endif
	if (Request.toString() == "Quit")
		QTimer::singleShot(100, QCoreApplication::instance(), SLOT(quit()));
	else if (Request.toString() == "Refresh")
		Response = true;
	else
		Response = "Unknown Command";

	SendVariant(pSocket, Response, 2000);
    pSocket->waitForDisconnected(1000);
}

bool CTaskService::CheckStatus(long Status)
{
#ifdef WIN32
    if (!(Status == STATUS_ACCESS_DENIED || Status == STATUS_PRIVILEGE_NOT_HELD ||
      (NT_NTWIN32(Status) && WIN32_FROM_NTSTATUS(Status) == ERROR_ACCESS_DENIED)))
        return false;
#endif

    if (theAPI->RootAvaiable())
        return false;

	return theConf->GetBool("Options/AutoElevate", true);
}

bool CTaskService::TaskAction(quint64 ProcessId, quint64 ThreadId, const QString& Action, const QVariant& Data)
{
	QString SocketName = CTaskService::RunWorker();

	if (SocketName.isEmpty())
		return false;
	
	QVariantMap Parameters;
	Parameters["ProcessId"] = ProcessId;
	Parameters["ThreadId"] = ThreadId;
	Parameters["Action"] = Action;
	Parameters["Data"] = Data;

	QVariantMap Request;
	Request["Command"] = "ExecTaskAction";
	Request["Parameters"] = Parameters;

	QVariant Response = CTaskService::SendCommand(SocketName, Request);

	if (Response.type() == QVariant::Int)
		return Response.toInt() == 0;
	return false;	
}

long CTaskService::ExecTaskAction(quint64 ProcessId, const QString& Action, const QVariant& Data)
{
#ifdef WIN32
    NTSTATUS status = STATUS_INVALID_PARAMETER; // unknown action
	HANDLE processHandle = NULL;
	if (Action == "Terminate")
	{
		if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_TERMINATE, (HANDLE)ProcessId)))
		{
			// An exit status of 1 is used here for compatibility reasons:
			// 1. Both Task Manager and Process Explorer use 1.
			// 2. winlogon tries to restart explorer.exe if the exit status is not 1.

			status = PhTerminateProcess(processHandle, 1);
		}
	}
	else if (Action == "Suspend")
	{
		if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, (HANDLE)ProcessId)))
		{
			status = NtSuspendProcess(processHandle);
		}
	}
	else if (Action == "Resume")
	{
		if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SUSPEND_RESUME, (HANDLE)ProcessId)))
		{
			status = NtResumeProcess(processHandle);
		}
	}
	else if (Action.left(3) == "Set")
	{
		if ((HANDLE)ProcessId != SYSTEM_PROCESS_ID)
		{
			// Changing the priority of System can lead to a BSOD on some versions of Windows,
			// so disallow this.
			status = STATUS_UNSUCCESSFUL;
		}
		else if (NT_SUCCESS(status = PhOpenProcess(&processHandle, PROCESS_SET_INFORMATION, (HANDLE)ProcessId)))
		{
			if (Action == "SetPriorityBoost")
			{
				status = PhSetProcessPriorityBoost(processHandle, !Data.toBool());
			}
			else if (Action == "SetPriority")
			{
				status = PhSetProcessPriorityClass(processHandle, (UCHAR)Data.toUInt());
			}
			else if (Action == "SetPagePriority")
			{
				status = PhSetProcessPagePriority(processHandle, Data.toInt());
			}
			else if (Action == "SetIOPriority")
			{
				status = PhSetProcessIoPriority(processHandle, (IO_PRIORITY_HINT)Data.toInt());
			}
			else if (Action == "SetAffinityMask")
			{
				status = PhSetProcessAffinityMask(processHandle, Data.toULongLong());
			}
		}
	}
	if(processHandle)
		NtClose(processHandle);
    return status;
#else
    return 0;
#endif
}

long CTaskService::ExecTaskAction(quint64 ProcessId, quint64 ThreadId, const QString& Action, const QVariant& Data)
{
#ifdef WIN32
    NTSTATUS status = STATUS_INVALID_PARAMETER; // unknown action
	HANDLE threadHandle = NULL;
	if (Action == "Terminate")
	{
		if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_TERMINATE, (HANDLE)ThreadId)))
		{
			status = NtTerminateThread(threadHandle, STATUS_SUCCESS);
		}
	}
	else if (Action == "Suspend")
	{
		if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, (HANDLE)ThreadId)))
		{
			status = NtSuspendThread(threadHandle, NULL);
		}
	}
	else if (Action == "Resume")
	{
		if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SUSPEND_RESUME, (HANDLE)ThreadId)))
		{
			status = NtResumeThread(threadHandle, NULL);
		}
	}
	else if (Action.left(3) == "Set")
	{
		if (NT_SUCCESS(status = PhOpenThread(&threadHandle, THREAD_SET_INFORMATION, (HANDLE)ThreadId)))
		{
			if (Action == "SetPriorityBoost")
			{
				status = PhSetThreadPriorityBoost(threadHandle, !Data.toBool());
			}
			else if (Action == "SetPriority")
			{
				status = PhSetThreadBasePriority(threadHandle, Data.toInt());
			}
			else if (Action == "SetPagePriority")
			{
				status = PhSetThreadPagePriority(threadHandle, Data.toInt());
			}
			else if (Action == "SetIOPriority")
			{
				status = PhSetThreadIoPriority(threadHandle, (IO_PRIORITY_HINT)Data.toInt());
			}
			else if (Action == "SetAffinityMask")
			{
				status = PhSetThreadAffinityMask(threadHandle, Data.toULongLong());
			}
		}
	}
	if(threadHandle)
		NtClose(threadHandle);
    return status;
#else
    return 0;
#endif
}

bool CTaskService::ServiceAction(const QString& Name, const QString& Action, const QVariant& Data)
{
	QString SocketName = CTaskService::RunWorker();

	if (SocketName.isEmpty())
		return false;
	
	QVariantMap Parameters;
	Parameters["Name"] = Name;
	Parameters["Data"] = Data;
	Parameters["Action"] = Action;

	QVariantMap Request;
	Request["Command"] = "ServiceTaskAction";
	Request["Parameters"] = Parameters;

	QVariant Response = CTaskService::SendCommand(SocketName, Request);

	if (Response.type() == QVariant::Int)
		return Response.toInt() == 0;
	return false;	
}

long CTaskService::ExecServiceAction(const QString& Name, const QString& Action, const QVariant& Data)
{
#ifdef WIN32
    NTSTATUS status = 0;
	std::wstring SvcName = Name.toStdWString();
	SC_HANDLE serviceHandle = NULL;
	if (Action == "Start")
	{
		if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_START, (wchar_t*)SvcName.c_str())))
		{
			if (!StartService(serviceHandle, 0, NULL))
				status = PhGetLastWin32ErrorAsNtStatus();
		}
	}
	else if (Action == "Pause")
	{
		if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (wchar_t*)SvcName.c_str())))
		{
			SERVICE_STATUS serviceStatus;
			if (!ControlService(serviceHandle, SERVICE_CONTROL_PAUSE, &serviceStatus))
				status = PhGetLastWin32ErrorAsNtStatus();
		}
	}
	else if (Action == "Continue")
	{
		if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_PAUSE_CONTINUE, (wchar_t*)SvcName.c_str())))
		{
			SERVICE_STATUS serviceStatus;
			if (!ControlService(serviceHandle, SERVICE_CONTROL_CONTINUE, &serviceStatus))
				status = PhGetLastWin32ErrorAsNtStatus();
		}
	}
	else if (Action == "Stop")
	{
		if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_STOP, (wchar_t*)SvcName.c_str())))
		{
			SERVICE_STATUS serviceStatus;
			if (!ControlService(serviceHandle, SERVICE_CONTROL_STOP, &serviceStatus))
				status = PhGetLastWin32ErrorAsNtStatus();
		}
	}
	else if (Action == "Delete")
	{
		if (NT_SUCCESS(PhOpenService(&serviceHandle, DELETE, (wchar_t*)SvcName.c_str())))
		{
			if (!DeleteService(serviceHandle))
				status = PhGetLastWin32ErrorAsNtStatus();
		}
	}
	else
		status = STATUS_INVALID_PARAMETER;
	if(serviceHandle)
		CloseServiceHandle(serviceHandle);
    return status;
#else
    return 0;
#endif
}

QVariant CTaskService::SendCommand(const QString& socketName, const QVariant &Command, int timeout)
{
    QLocalSocket Socket;

    bool Ok = false;
    for(int i = 0; i < 2; i++) 
	{
        Socket.connectToServer(socketName);
        Ok = Socket.waitForConnected(timeout == -1 ? 30000 : timeout/2);
        if (Ok)
            break;
		QThread::msleep(255);
    }

	if (!Ok || !SendVariant(&Socket, Command, timeout))
		return QVariant(); // invalid variant means communication failed

	QVariant Response = RecvVariant(&Socket, timeout);
#ifdef _DEBUG
	ASSERT(Response.toString() != "Unknown Command");
#endif
	return Response;
}

#ifdef WIN64
QString CTaskService::RunWorker(bool bElevanted, bool b32Bit)
#else
QString CTaskService::RunWorker(bool bElevanted)
#endif
{
	QMutexLocker Locker(&m_Mutex);

#ifdef WIN32
	QString BinaryPath;
#ifdef WIN64
	if (b32Bit)
	{
		static char* relativeFileNames[] =
		{
			"\\x86\\TaskExplorer.exe",
			"\\..\\x86\\TaskExplorer.exe",
#ifdef DEBUG
			"\\..\\..\\Win32\\Debug\\TaskExplorer.exe"
#else
			"\\..\\..\\Win32\\Release\\TaskExplorer.exe"
#endif
		};

		QString AppDir = QApplication::applicationDirPath();

		for (int i = 0; i < RTL_NUMBER_OF(relativeFileNames); i++)
		{
			QString TestPath = QDir::cleanPath(AppDir + relativeFileNames[i]);
			if (QFile::exists(TestPath))
			{
				BinaryPath = TestPath.replace("/", "\\");
				break;
			}
		}

		if (BinaryPath.isEmpty())
			return QString();
	}
	else
#endif
	{
		wchar_t szPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
			return false;
		BinaryPath = QString::fromWCharArray(szPath);
	}

	QString SocketName;
	{
#ifdef WIN64
		if (b32Bit)
			SocketName = m_TempSocket32;
		else
#endif
			SocketName = m_TempSocket;
	}

	if (!SocketName.isEmpty())
	{
		if (SendCommand(SocketName, "Refresh", 500).toBool() == true)
			return SocketName;
	}

	SocketName = TASK_SERVICE_NAME;
	SocketName += "_" + GetRand64Str();

	std::wstring Arguments = L"-wrk \"" + SocketName.toStdWString() + L"\"";
#if 0
	Arguments.append(L" -timeout 50000");
	Arguments.append(L" -dbg_wait");
#else
	Arguments.append(L" -timeout 5000");
#endif

	HANDLE ProcessHandle = NULL;
	if (bElevanted && !PhGetOwnTokenAttributes().Elevated)
		PhShellProcessHackerEx(NULL, (wchar_t*)BinaryPath.toStdWString().c_str(), (wchar_t*)Arguments.c_str(), SW_HIDE, PH_SHELL_EXECUTE_ADMIN, 0, 0, &ProcessHandle);
	else
		PhShellProcessHackerEx(NULL, (wchar_t*)BinaryPath.toStdWString().c_str(), (wchar_t*)Arguments.c_str(), SW_HIDE, 0, 0, 0, &ProcessHandle);
	if (ProcessHandle == NULL)
		return QString();

#ifdef WIN64
	if (b32Bit)
		m_TempSocket32 = SocketName;
	else
#endif
		m_TempSocket = SocketName;

	return SocketName;
#else
	return QString(); // linux-todo:
#endif
}

void CTaskService::TerminateWorkers()
{
	QMutexLocker Locker(&m_Mutex);

	if (!m_TempName.isEmpty())
		Terminate(m_TempName);

	if (!m_TempSocket.isEmpty())
		Terminate(m_TempSocket);
#ifdef WIN64
	if (!m_TempSocket32.isEmpty())
		Terminate(m_TempSocket32);
#endif
}

// Note: we could do that using QtService, but it does not expose the required API directly only through start parameters
//			so we would start a process that creates the service, than we start a process that starts the service and than we start a process that deleted the service, 
//			thats to much starting of processes...

// ToDo: fork the QtService library to enable better controll for our usecase

QString CTaskService::RunService()
{
	QMutexLocker Locker(&m_Mutex);

#ifdef WIN32
	QString ServiceName = TASK_SERVICE_NAME;

	/*// check if a persistent service is installed and running
	bool isRunning = false;
	bool isInstalled = false;
	SC_HANDLE serviceHandle;
	if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_QUERY_STATUS, (wchar_t*)ServiceName.toStdWString().c_str())))
	{
		isInstalled = true;
		SERVICE_STATUS serviceStatus;
		if (QueryServiceStatus(serviceHandle, &serviceStatus) && serviceStatus.dwCurrentState == SERVICE_RUNNING)
			isRunning = true;
		CloseServiceHandle(serviceHandle);
	}
	if (isRunning)
		return ServiceName;*/

	{
		if (!m_TempName.isEmpty())
		{
			if (SendCommand(m_TempName, "Refresh", 500).toBool() == true)
				return m_TempName;
		}
	}

	// if not, than start a new temporary service instance
	ServiceName += "_" + GetRand64Str();
	if (RunService(ServiceName))
	{
		m_TempName = ServiceName;
		return ServiceName;
	}
#endif
	return QString();
}

bool CTaskService::RunService(const QString& ServiceName, QString BinaryPath)
{
#ifdef WIN32
	// Note: this function may be invoked without prior calling InitPH() !!!

	if (BinaryPath.isEmpty())
	{
		wchar_t szPath[MAX_PATH];
		if (!GetModuleFileName(NULL, szPath, ARRAYSIZE(szPath)))
			return false;
		BinaryPath = QString::fromWCharArray(szPath);
	}

	if (!IsElevated())
	{
		int Ret = RunElevated(BinaryPath.toStdWString(), L"-runsvc " + ServiceName.toStdWString(), true);
		if (Ret == EXIT_SUCCESS);
			return true;
		return false;
	}

	BOOLEAN success = FALSE;

	// Try to start the service if its installed
	SC_HANDLE serviceHandle; 
	if (NT_SUCCESS(PhOpenService(&serviceHandle, SERVICE_QUERY_STATUS | SERVICE_START, (wchar_t*)ServiceName.toStdWString().c_str()))) // this is safe to call without InitPH
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

		std::wstring CommandLine = L"\"" + BinaryPath.toStdWString() + L"\" -svc \"" + ServiceName.toStdWString() + L"\"";
#if 0
		CommandLine.append(L" -timeout 50000");
		CommandLine.append(L" -dbg_wait");
#else
		CommandLine.append(L" -timeout 5000");
#endif

		SC_HANDLE serviceHandle = CreateService(scManagerHandle, ServiceName.toStdWString().c_str(), ServiceName.toStdWString().c_str(), SERVICE_ALL_ACCESS, SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START,
			SERVICE_ERROR_IGNORE, CommandLine.c_str(), NULL, NULL, NULL, L"LocalSystem", L"");
		//ULONG  win32Result = GetLastError();

		if (serviceHandle)
		{
			//PhSetDesktopWinStaAccess(); // can this work without InitPH?

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
