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

void PackDriver();
void UnPackDrivers();

int main(int argc, char *argv[])
{
#ifndef _DEBUG
	InitMiniDumpWriter(L"TaskExplorer");
#endif

	qsrand(QTime::currentTime().msec());

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

			string command_line = ++i < argc ? argv[i] : "cmd.exe";
			create_process_as_trusted_installer(wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(command_line));
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

	QtSingleApplication* pApp = NULL;
	if (bSvc || bWrk)	
		new QCoreApplication(argc, argv);
	else {
#ifdef Q_OS_WIN
		//SetProcessDPIAware();
#endif // Q_OS_WIN 
		QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling); 
		//QCoreApplication::setAttribute(Qt::AA_DisableHighDpiScaling);

		//new QApplication(argc, argv);
		pApp = new QtSingleApplication(argc, argv);
	}

	theConf = new CSettings("TaskExplorer");

#ifdef WIN32
	//InitConsole(false);
	if (pApp->arguments().contains("-rc4"))
	{
		PackDriver();
		return 0;
	}

	if(IsElevated())
		UnPackDrivers();
#endif

	if (pApp && !theConf->GetBool("Options/AllowMultipleInstances", false) && !bMulti && pApp->sendMessage("ShowWnd"))
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


bool TransformFile(const QString& InName, const QString& OutName, const QString& Key = "default_key")
{
	QFile InFile(InName);
	QFile OutFile(OutName);
	if (InFile.open(QIODevice::ReadOnly))
	{
		if (OutFile.open(QIODevice::WriteOnly))
		{
			rc4_sbox_s sbox;
			rc4_init(&sbox, Key.toLatin1());
			OutFile.write(rc4_transform(&sbox, InFile.readAll()));
			OutFile.flush();
			return true;
		}
	}
	return false;
}

bool TestFile(const QString& OutName)
{
	QThread::sleep(3);
	return QFile::exists(OutName);
}

void PackDriver()
{
	int pos = QCoreApplication::arguments().indexOf("-rc4");
	QString InName = QCoreApplication::arguments().at(pos + 1);
	QString OutName = InName;
	if (OutName.right(4) == ".rc4")
		OutName.truncate(OutName.length() - 4);
	else
		OutName.append(".rc4");

	if (TransformFile(InName, OutName))
		printf("success\r\n");
	else
		printf("failed\r\n");
}

bool NotifyCert()
{
	QString Caption = QObject::tr(
		"<h3>Important Notie</h3>"
	);
	QString Text = QObject::tr(
		"<p>TaskExplorer requires a driver to operate (xprocesshacker.sys), Windows however denies loading a driver file that has not been digitally signed.</p>"
		"<p>Luckily brave hackers have Leaked a few of these Certificates over the years, one of them was found by the author of this software and put to good use.</p>"
		"<p>Unfortunately, such certificates have been abused by malware authors resulting in many Anti Malware Fools being Lazy and flagging Everything signed with them Wrongfully as Malware. "
		"This Prejudice is Damaging the Open Source Ecosystem.</p>"
		"<p>Therefore, the required driver is provided in an obfuscated form and before use must be unpacked. "
		"<font color='red'>When doing so said Anti Viruses will complain and attempt to destroy the freshly created file.</font> "
		"Please be aware that this is a <u>false positive</u> caused by the necessary use of a compromised certificate. "
		"If this happens you will be notified and offered the option to repeat the unpacking operation, for the operation to succeed you will need to <u>configure an appropriate exemption</u>.</p>"

		"<p>If you want to proceed with the unpacking of the driver press YES.</p>"
	);
	QMessageBox *msgBox = new QMessageBox(NULL);
	msgBox->setAttribute(Qt::WA_DeleteOnClose);
	msgBox->setWindowTitle("TaskExplorer");
	msgBox->setText(Caption);
	msgBox->setInformativeText(Text);
	msgBox->setStandardButtons(QMessageBox::Yes);
	msgBox->addButton(QMessageBox::No);
	msgBox->setDefaultButton(QMessageBox::Yes);

	QIcon ico(QLatin1String(":/TaskExplorer.png"));
	msgBox->setIconPixmap(ico.pixmap(64, 64));

	return msgBox->exec() == QMessageBox::Yes;
}

void UnPackDrivers()
{
	bool notifyNotOk = false;
	QDir appDir(QApplication::applicationDirPath());
	foreach(const QString& FileName, appDir.entryList(QStringList("*.sys.rc4"), QDir::Files))
	{
		QString InName = QApplication::applicationDirPath() + "/" + FileName;
		QString OutName = InName.mid(0, InName.length() - 4);

		QFileInfo InInfo(InName);
		QFileInfo OutInfo(OutName);
		if (InInfo.size() != OutInfo.size() || InInfo.lastModified() > OutInfo.lastModified())
		{
			if (theConf->GetBool("Options/NotifyUnPack", true)) {
				if (!NotifyCert()) {
					notifyNotOk = true;
					break;
				}
				theConf->SetValue("Options/NotifyUnPack", false);
			}

		retry:
			if (!TransformFile(InName, OutName))
				QMessageBox::warning(NULL, "TaskExplorer", QObject::tr("Failed to decrypt %1 ensure app directory is writable.").arg(FileName));
			else if (!TestFile(OutName))
			{
				if (QMessageBox("TaskExplorer",
					QObject::tr("The decrypted file %1 seam to have been removed. Retry file extraction?").arg(FileName),
					QMessageBox::Information, QMessageBox::Yes | QMessageBox::Default, QMessageBox::Cancel, QMessageBox::NoButton).exec() == QMessageBox::Yes)
					goto retry;
				notifyNotOk = true;
			}
		}
	}
	if (notifyNotOk)
		QMessageBox::warning(NULL, "TaskExplorer", QObject::tr("Without the Driver TaskExplorer wont be able to run properly."));
}