#include "stdafx.h"
#include "GUI/TaskExplorer.h"
#include <QtWidgets/QApplication>
//#include <vld.h>
#include <QThreadPool>
#include "SVC/TaskService.h"
#ifdef WIN32
#include "API/Windows/ProcessHacker.h"
#include "API/Windows/WinAdmin.h"
#include <codecvt>
#include "../QtSingleApp/src/qtsingleapplication.h"
#include "../MiscHelpers/Common/qRC4.h"
#include "../../MiscHelpers/Common/CheckableMessageBox.h"
#include "..\ProcessHacker\kphlib\include\sistatus.h"

int SkipUacRun(bool test_only = false);
#endif

CSettings* theConf = NULL;


int main(int argc, char *argv[])
{
	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
	*wcsrchr(szPath, L'\\') = L'\0';

#ifndef _DEBUG
	InitMiniDumpWriter(L"TaskExplorer", szPath);
#endif

	srand(QTime::currentTime().msec());

#ifndef USE_TASK_HELPER	
	bool bSvc = false;
	bool bWrk = false;
	QString svcName = TASK_SERVICE_NAME;
	const char* run_svc = NULL;
#endif
	bool bMulti = false;
	bool bNoSkip = false;
	int timeOut = 0;
    for(int i = 1; i < argc; i++)
    {
#ifndef USE_TASK_HELPER	
		if (strcmp(argv[i], "-svc") == 0 || strcmp(argv[i], "-wrk") == 0)
		{
			bSvc = (strcmp(argv[i], "-svc") == 0);
			bWrk = (strcmp(argv[i], "-wrk") == 0);
			if(++i < argc)
				svcName =  argv[i];
		}
		else 
#endif
		if (strcmp(argv[i], "-kx") == 0)
			g_KphStartupMax = TRUE;
		else if (strcmp(argv[i], "-kh") == 0)
			g_KphStartupHigh = TRUE;
		else if (strcmp(argv[i], "-multi") == 0)
			bMulti = true;
		else if (strcmp(argv[i], "-no_elevate") == 0)
			bNoSkip = true;
		else if (strcmp(argv[i], "-timeout") == 0)
			timeOut = ++i < argc ? atoi(argv[i]) : 10000;
		else if (strcmp(argv[i], "-dbg_wait") == 0)
		{
			// add timeout?
			WaitForDebugger();
		}
#ifndef USE_TASK_HELPER	
		else if (strcmp(argv[i], "-runsvc") == 0)
		{
			run_svc = ++i < argc ? argv[i] : TASK_SERVICE_NAME;
		}
#endif
    }

#ifdef WIN32
	bool bTestElevated = false;
#ifndef USE_TASK_HELPER	
	if (!bSvc && !bWrk)
#endif
	if (!IsElevated() && !bNoSkip)
	{
		int ret = SkipUacRun(); // Warning: the started process will have lower priority!
		if (ret == 1)
			return 0;
		if (ret == -1) { // the driver does not allow us the know the state, so we wait a second and check if an other instance has came up
			bTestElevated = true;
			QThread::msleep(1000);
		}
	}
#endif

#ifndef USE_TASK_HELPER	
	if (run_svc)
	{
		if (CTaskService::RunService(run_svc)) {
			//_exit(EXIT_SUCCESS);
			return EXIT_SUCCESS; // 0
		}
		return EXIT_FAILURE; // 1
	}
#endif

	QString AppDir = QString::fromWCharArray(szPath);

	QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
	if (dirs.count() > 2) { // Note: last 2 are AppDir and AppDir/data
		QString OldPath;
		QString NewPath;
		if (dirs.count() > 3 && QFile::exists((OldPath = dirs[1] + "/TaskExplorer") + "/TaskExplorer.ini"))
			NewPath = dirs[1] + "/Xanasoft";
		else if (QFile::exists((OldPath = dirs[0] + "/TaskExplorer") + "/TaskExplorer.ini"))
			NewPath = dirs[0] + "/Xanasoft";

		if (!NewPath.isEmpty() && !QFile::exists(NewPath + "/TaskExplorer" + "/TaskExplorer.ini")){
			QDir().mkpath(NewPath);
			QDir().rename(OldPath, NewPath + "/TaskExplorer");
		}
	}
	theConf = new CSettings(AppDir, "TaskExplorer", "Xanasoft");

	InitPH();

#ifndef USE_TASK_HELPER
	if (bSvc) 
	{
		HANDLE tokenHandle; // Enable some required privileges.
		if (NT_SUCCESS(PhOpenProcessToken(NtCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &tokenHandle)))
		{
			PhSetTokenPrivilege2(tokenHandle, SE_ASSIGNPRIMARYTOKEN_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_INCREASE_QUOTA_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_BACKUP_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_RESTORE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			PhSetTokenPrivilege2(tokenHandle, SE_IMPERSONATE_PRIVILEGE, SE_PRIVILEGE_ENABLED);
			NtClose(tokenHandle);
		}
	}
#endif


	// this must be done before we create QApplication
	int DPI = theConf->GetInt("Options/DPIScaling", 1);
	if (DPI == 1) {
		//SetProcessDPIAware();
		//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		//SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		typedef DPI_AWARENESS_CONTEXT(WINAPI* P_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT dpiContext);
		P_SetThreadDpiAwarenessContext pSetThreadDpiAwarenessContext = (P_SetThreadDpiAwarenessContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
		if(pSetThreadDpiAwarenessContext) // not present on windows 7
			pSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
		else
			SetProcessDPIAware();
	}
	else if (DPI == 2) {
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); 
	}
	//else {
	//	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
	//}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

	//
	// Qt 6 uses the windows font cache which wants to access our process but our driver blocks it
	// that causes a lot of log entries, hence we disable the use of windows fonr cache.
	//
	qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("windows:nodirectwrite"));



	STATUS DrvStatus = OK;

#ifdef Q_OS_WIN
#ifndef _DEBUG
	// Set the default priority.
	{
		PhSetProcessPriorityClass(NtCurrentProcess(), PROCESS_PRIORITY_CLASS_ABOVE_NORMAL);

		PhSetProcessPagePriority(NtCurrentProcess(), MEMORY_PRIORITY_NORMAL);
		PhSetProcessIoPriority(NtCurrentProcess(), IoPriorityNormal);
	}
#endif
#endif // Q_OS_WIN


	QtSingleApplication* pApp = NULL;
#ifndef USE_TASK_HELPER
	if (bSvc || bWrk)
	{
		new QCoreApplication(argc, argv);
	}
	else if (!bSvc && !bWrk)
#endif
	{
		// this must be done before we create QApplication
		int DPI = theConf->GetInt("Options/DPIScaling", 1);
		if (DPI == 1) {
			//SetProcessDPIAware();
			//SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
			//SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
			typedef DPI_AWARENESS_CONTEXT(WINAPI* P_SetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT dpiContext);
			P_SetThreadDpiAwarenessContext pSetThreadDpiAwarenessContext = (P_SetThreadDpiAwarenessContext)GetProcAddress(GetModuleHandleW(L"user32.dll"), "SetThreadDpiAwarenessContext");
			if(pSetThreadDpiAwarenessContext) // not present on windows 7
				pSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_SYSTEM_AWARE);
			else
				SetProcessDPIAware();
		}
		else if (DPI == 2) {
			QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); 
		}
		//else {
		//	QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);
		//}

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		QApplication::setAttribute(Qt::AA_DisableWindowContextHelpButton);
#endif

		//new QApplication(argc, argv);
		pApp = new QtSingleApplication((IsElevated() || bTestElevated) ? "TaskExplorer" : "UTaskExplorer", argc, argv);

		if (theConf->GetBool("OptionsKSI/KsiEnable", true) && IsElevated() && !PhIsExecutingInWow64())
		{
			DrvStatus = InitKSI(AppDir);
		}
	}

	if (pApp)
	{
		if (bTestElevated && pApp->isClient())
			return 0;

		if (!theConf->GetBool("Options/AllowMultipleInstances", false) && !bMulti && pApp->sendMessage("ShowWnd"))
			return 0;
	}

	//DrvStatus = ERR(STATUS_UNKNOWN_REVISION);
	int DynDataUpdate = 0;
	while (DrvStatus.IsError() || DynDataUpdate == 2)
	{
		QString Message;
		QDialogButtonBox::StandardButtons buttons = QDialogButtonBox::Ok;
		if (DynDataUpdate == -1)
		{
			Message = CTaskExplorer::tr("Failed to update DynData, %1, Error: 0x%2 (%3).").arg(DrvStatus.GetText()).arg((quint32)DrvStatus.GetStatus(), 8, 16, QChar('0')).arg(CastPhString(PhGetNtMessage(DrvStatus.GetStatus())));
		}
		else if (DrvStatus.GetStatus() == STATUS_SI_DYNDATA_UNSUPPORTED_KERNEL || DrvStatus.GetStatus() == STATUS_UNKNOWN_REVISION || DynDataUpdate != 0) 
		{
			QString windowsVersion = QString("%1").arg(WindowsVersion); // todo
			QString kernelVersion = CastPhString(KsiGetKernelVersionString());

			Message = CTaskExplorer::tr("The current DynData for the KTaskExplorer driver does not yet supported on your windows kernel version.<br />"
				"You can check for <a href=\"https://github.com/DavidXanatos/TaskExplorer/releases\">TaskExplorer updates on github</a>, "
				"or grab the latest ksidyn.bin and ksidyn.sig from <a href=\"https://systeminformer.sourceforge.io/downloads\">the latest SystemInformer</a> "
				"and put them in the instalaltion directors next to KTaskExplorer.sys.<br />"
				"Instalation Directory: %4<br />"
				"<br />"
				"Operating System Details:<br />"
				"&nbsp;&nbsp;&nbsp;&nbsp;Windows %1<br />"
				"&nbsp;&nbsp;&nbsp;&nbsp;Windows Kernel %2<br />"
				"&nbsp;&nbsp;&nbsp;&nbsp;TaskExplorer %3<br />"
				"<br />").arg(windowsVersion).arg(kernelVersion).arg(CTaskExplorer::GetVersion()).arg(AppDir);

			if (DynDataUpdate == 1)
				Message += CTaskExplorer::tr("Update did not resolve the issue.");
			else {
				Message += CTaskExplorer::tr("Do you want to try to download updated DynData Yes, start without the driver No?");
				buttons = QDialogButtonBox::Yes | QDialogButtonBox::No | QDialogButtonBox::Cancel;
			}
		} 
		else {
			Message = CTaskExplorer::tr("Failed to load KTaskExplorer driver, %1, Error: 0x%2 (%3).").arg(DrvStatus.GetText()).arg((quint32)DrvStatus.GetStatus(), 8, 16, QChar('0')).arg(CastPhString(PhGetNtMessage(DrvStatus.GetStatus())));
		}

		bool State = false;
		int Ret = CCheckableMessageBox::question(NULL, "TaskExplorer", Message
			, CTaskExplorer::tr("Disable KTaskExplorer driver. Note: this will limit the aplications functionality!"), &State, 
			buttons, QDialogButtonBox::Ok, QMessageBox::Warning);

		if (Ret == QDialogButtonBox::Yes)
		{
			DrvStatus = TryUpdateDynData(AppDir);
			if (DrvStatus.IsError()) {
				QMessageBox::critical(NULL, "TaskExplorer", CTaskExplorer::tr("Failed to update DynData, %1.").arg(DrvStatus.GetText()));
				DynDataUpdate = -1;
			}
			else {
				DynDataUpdate = 1;
				CleanupKSI();
				DrvStatus = InitKSI(AppDir);
			}
			continue;
		}

		if (State)
			theConf->SetValue("OptionsKSI/KsiEnable", false);

		break;
	}

	QThreadPool::globalInstance()->setMaxThreadCount(theConf->GetInt("Options/MaxThreadPool", 10));

	int ret = 0;
#ifndef USE_TASK_HELPER
	// Old behavior: TaskExplorer handles service/worker modes
	if (bSvc || bWrk)
	{
		CTaskService Svc(1/*argc*/, argv, svcName, timeOut);
		if(bSvc)
			ret = Svc.exec();
		else
		{
			Svc.start();
			QCoreApplication::exec();
			Svc.stop();
		}
	}
	else
#endif
	if(pApp)
	{
#ifdef WIN32
#ifndef _WIN64
#ifndef _DEBUG
		if (PhIsExecutingInWow64())
		{
//			QString BinaryPath = "";
//
//			static char* relativeFileNames[] =
//			{
//				"\\x64\\TaskExplorer.exe",
//				"\\..\\x64\\TaskExplorer.exe",
//#ifdef DEBUG
//				"\\..\\..\\x64\\Debug\\TaskExplorer.exe"
//#else
//				"\\..\\..\\x64\\Release\\TaskExplorer.exe"
//#endif
//			};
//
//			QString AppDir = QApplication::applicationDirPath();
//
//			for (int i = 0; i < RTL_NUMBER_OF(relativeFileNames); i++)
//			{
//				QString TestPath = QDir::cleanPath(AppDir + relativeFileNames[i]);
//				if (QFile::exists(TestPath))
//				{
//					BinaryPath = TestPath.replace("/", "\\");
//					break;
//				}
//			}
//
//			if (!BinaryPath.isEmpty()) 
//			{
//				QStringList Args = QCoreApplication::instance()->arguments();
//				Args.removeFirst();
//				QProcess::startDetached(BinaryPath, Args);
//			}
//			else
			{
				QMessageBox::critical(NULL, "TaskExplorer", CTaskExplorer::tr(
					"You are attempting to run the 32-bit version of Task Explorer on 64-bit Windows. "
					"Most features will not work correctly.\n\n"
					"Please run the 64-bit version of Task Explorer instead."
				));
			}
			//QApplication::instance()->quit();
			return 0;
		}
#endif
#endif
#endif
		pApp->setQuitOnLastWindowClosed(false);

#if QT_VERSION > QT_VERSION_CHECK(6, 7, 0)
		if (pApp->style()->name() == "windows11" && !theConf->GetBool("Options/UseW11Style", false))
			pApp->setStyle("windowsvista");
#endif

		new CTaskExplorer();

		QObject::connect(pApp, SIGNAL(messageReceived(const QString&)), theGUI, SLOT(OnMessage(const QString&)));
		
		ret = pApp->exec();

		delete theGUI;

		CTaskService::TerminateWorkers();
	}

	CleanupKSI();

	// note: if ran as a service teh instance wil have already been delted, but delete NULL is ok
	delete pApp;

	delete theConf;
	theConf = NULL;

	return ret;
}
