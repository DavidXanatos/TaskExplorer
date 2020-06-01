#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_DriverWindow.h"
#include "../API/Windows/WinProcess.h"
#include "../../MiscHelpers/Common/PanelView.h"

class CDriverWindow : public QMainWindow
{
	Q_OBJECT

public:
	CDriverWindow(QWidget *parent = Q_NULLPTR);
	~CDriverWindow();

public slots:
	void accept();
	void reject();

	void OnDriverFile();
	void Refresh();
	void OnConnect();
	void OnStop();

protected:
	void closeEvent(QCloseEvent *e);
	void timerEvent(QTimerEvent *e);

	int					m_TimerId;

	bool				m_HoldValues;

private:
	Ui::DriverWindow ui;
};
