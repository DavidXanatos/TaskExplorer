#include "stdafx.h"
#include "GUI\TaskExplorer.h"
#include <QtWidgets/QApplication>
//#include <vld.h>
#include <QThreadPool>


int main(int argc, char *argv[])
{
	QApplication App(argc, argv);

	QThreadPool::globalInstance()->setMaxThreadCount(4);

	theConf = new CSettings(QApplication::applicationDirPath() + "/TaskExplorer.ini");

	CTaskExplorer* pWnd = new CTaskExplorer();
	pWnd->show();
	int ret = App.exec();
	delete pWnd;

	delete theConf;
	theConf = NULL;

	return ret;
}
