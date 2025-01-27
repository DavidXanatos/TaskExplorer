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

bool SkipUacRun(bool test_only = false);
#endif

CSettings* theConf = NULL;


int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(L"TaskExplorer", L"");
#endif

	srand(QTime::currentTime().msec());

	bool bSvc = false;
	bool bWrk = false;
	bool bMulti = false;
	bool bNoSkip = false;
	QString svcName = TASK_SERVICE_NAME;
	int timeOut = 0;
    const char* run_svc = NULL;
    for(int i = 1; i < argc; i++)
    {
		if (strcmp(argv[i], "-svc") == 0 || strcmp(argv[i], "-wrk") == 0)
		{
			bSvc = (strcmp(argv[i], "-svc") == 0);
			bWrk = (strcmp(argv[i], "-wrk") == 0);
			if(++i < argc)
				svcName =  argv[i];
		}
		else if (strcmp(argv[i], "-kx") == 0)
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
		else if (strcmp(argv[i], "-runsvc") == 0)
		{
			run_svc = ++i < argc ? argv[i] : TASK_SERVICE_NAME;
		}
#ifdef WIN32
		else if (strcmp(argv[i], "-runasti") == 0)
		{
			if (!IsElevated() && !SkipUacRun()) 
			{
				return RestartElevated(argc, argv);
			}

			std::string command_line = ++i < argc ? argv[i] : "cmd.exe";
			create_process_as_trusted_installer(std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(command_line));
			return 0;
		}
#endif
    }

#ifdef WIN32
	if (!bSvc && !bWrk && !IsElevated() && !bNoSkip)
	{
		if (SkipUacRun()) // Warning: the started process will have lower priority!
			return 0;
	}
#endif

	if (run_svc)
	{
		if (CTaskService::RunService(run_svc)) {
			//_exit(EXIT_SUCCESS);
			return EXIT_SUCCESS; // 0
		}
		return EXIT_FAILURE; // 1
	}

	wchar_t szPath[MAX_PATH];
	GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath));
	*wcsrchr(szPath, L'\\') = L'\0';
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
	theConf = new CSettings(AppDir, "Xanasoft", "TaskExplorer");

	InitPH(bSvc);

	STATUS DrvStatus = OK;
	if (theConf->GetBool("OptionsKSI/KsiEnable", true) && IsElevated() && !PhIsExecutingInWow64())
	{
		DrvStatus = InitKSI(AppDir);
	}

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
	if (bSvc || bWrk)	
		new QCoreApplication(argc, argv);
	else {
#ifdef Q_OS_WIN
		SetProcessDPIAware();
#endif // Q_OS_WIN 
		//QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); 
		//QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

		//new QApplication(argc, argv);
		pApp = new QtSingleApplication(IsElevated() ? "TaskExplorer" : "UTaskExplorer", argc, argv);
	}

	if (pApp && !theConf->GetBool("Options/AllowMultipleInstances", false) && !bMulti && pApp->sendMessage("ShowWnd"))
		return 0;

	if (DrvStatus.IsError()) {
		if (!CTaskExplorer::CheckErrors(QList<STATUS>() << DrvStatus))
			return -1;
	}

	QThreadPool::globalInstance()->setMaxThreadCount(theConf->GetInt("Options/MaxThreadPool", 10));

	int ret = 0;
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
	{
#ifdef WIN32
#ifndef WIN64
#ifndef _DEBUG
		if (PhIsExecutingInWow64())
		{
			QString BinaryPath = "";

			static char* relativeFileNames[] =
			{
				"\\x64\\TaskExplorer.exe",
				"\\..\\x64\\TaskExplorer.exe",
#ifdef DEBUG
				"\\..\\..\\x64\\Debug\\TaskExplorer.exe"
#else
				"\\..\\..\\x64\\Release\\TaskExplorer.exe"
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

			if (!BinaryPath.isEmpty()) 
			{
				QStringList Args = QCoreApplication::instance()->arguments();
				Args.removeFirst();
				QProcess::startDetached(BinaryPath, Args);
			}
			else
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

		new CTaskExplorer();
		QObject::connect(QtSingleApplication::instance(), SIGNAL(messageReceived(const QString&)), theGUI, SLOT(OnMessage(const QString&)));
		
		ret = QApplication::exec();

		delete theGUI;

		CTaskService::TerminateWorkers();
	}

	CleanupKSI();

	// note: if ran as a service teh instance wil have already been delted, but delete NULL is ok
	delete QCoreApplication::instance();

	delete theConf;
	theConf = NULL;

	return ret;
}
