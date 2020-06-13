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

bool SkipUacRun(bool test_only = false);
#endif

CSettings* theConf = NULL;

int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(L"TaskExplorer");
#endif

	qsrand(QTime::currentTime().msec());

	bool bSvc = false;
	bool bWrk = false;
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

			string command_line = ++i < argc ? argv[i] : "cmd.exe";
			create_process_as_trusted_installer(wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(command_line));
			return 0;
		}
#endif
    }

#ifdef WIN32
	if (!bSvc && !bWrk && !IsElevated())
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
		QtSingleApplication* pApp = new QtSingleApplication(argc, argv);
	}

	theConf = new CSettings("TaskExplorer");

	if (pApp && !theConf->GetBool("Options/AllowMultipleInstances", false) && pApp->sendMessage("ShowWnd"))
		return 0;

#ifdef Q_OS_WIN
#ifndef _DEBUG
    // Set the default priority.
    {
        PROCESS_PRIORITY_CLASS priorityClass;
        priorityClass.Foreground = FALSE;
        priorityClass.PriorityClass = PROCESS_PRIORITY_CLASS_ABOVE_NORMAL;
        PhSetProcessPriority(NtCurrentProcess(), priorityClass);

		PhSetProcessPagePriority(NtCurrentProcess(), MEMORY_PRIORITY_NORMAL);
		PhSetProcessIoPriority(NtCurrentProcess(), IoPriorityNormal);
    }
#endif
#endif // Q_OS_WIN

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
				QProcess::startDetached(BinaryPath, QCoreApplication::instance()->arguments());
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

	// note: if ran as a service teh instance wil have already been delted, but delete NULL is ok
	delete QCoreApplication::instance();

	delete theConf;
	theConf = NULL;

	return ret;
}
