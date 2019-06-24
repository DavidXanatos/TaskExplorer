#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_WinSvcWindow.h"

class CWinSvcWindow : public QMainWindow
{
	Q_OBJECT

public:
	CWinSvcWindow(QWidget *parent = Q_NULLPTR);

protected:
	void closeEvent(QCloseEvent *e);

private:
	Ui::WinSvcWindow ui;
};
