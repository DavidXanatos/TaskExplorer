#include "stdafx.h"
#include "GUI\TaskExplorer.h"
#include <QtWidgets/QApplication>
//#include <vld.h>
#include <QThreadPool>
#include "SVC\TaskService.h"
#ifdef WIN32
#include "API/Windows/ProcessHacker.h"
#include "API/Windows/WinAdmin.h"
#include <codecvt>

bool SkipUacRun(bool test_only = false);
#endif

int main(int argc, char *argv[])
{
	qsrand(QTime::currentTime().msec());

	bool bSvc = false;
	QString svcName = TASK_SERVICE_NAME;
	int timeOut = 0;
	char* run_svc = NULL;
    for(int i = 1; i < argc; i++)
    {
		if (strcmp(argv[i], "-svc") == 0)
		{
			bSvc = true;
			if(++i < argc)
				svcName =  argv[i];
		}
		else if (strcmp(argv[i], "-timeout") == 0)
			timeOut = ++i < argc ? atoi(argv[i]) : 10000;
#ifdef _DEBUG
		else if (strcmp(argv[i], "-dbg_wait") == 0)
		{
			// add timeout?
			WaitForDebugger();
		}
#endif
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

	if (!bSvc && !IsElevated())
	{
		if (SkipUacRun())
			return 0;
	}

	if (run_svc)
	{
		if (CTaskService::RunService(run_svc)) {
			//_exit(EXIT_SUCCESS);
			return EXIT_SUCCESS; // 0
		}
		return EXIT_FAILURE; // 1
	}

	if (bSvc)		new QCoreApplication(argc, argv);
	else			new QApplication(argc, argv);

	theConf = new CSettings(QCoreApplication::applicationDirPath() + "/TaskExplorer.ini");

	QThreadPool::globalInstance()->setMaxThreadCount(theConf->GetInt("Options/MaxThreadPool", 10));

	int ret = 0;
	if (bSvc)
	{
		CTaskService Svc(1/*argc*/, argv, svcName, timeOut);
		ret = Svc.exec();
	}
	else
	{
		CTaskExplorer Wnd;
		ret = QApplication::exec();
	}

	delete theConf;
	theConf = NULL;

	// note: if ran as a service teh instance wil have already been delted, but delete NULL is ok
	delete QCoreApplication::instance();

	return ret;
}
