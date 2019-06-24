#include "stdafx.h"
#include "WinSvcWindow.h"

CWinSvcWindow::CWinSvcWindow(QWidget *parent)
	: QMainWindow(parent)
{
	ui.setupUi(this);
}

void CWinSvcWindow::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}