#include "stdafx.h"
#include "GUI\TaskExplorer.h"
#include <QtWidgets/QApplication>
//#include <vld.h>
#include <QThreadPool>


int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	QThreadPool::globalInstance()->setMaxThreadCount(4);

	CTaskExplorer w;
	w.show();
	return a.exec();
}
