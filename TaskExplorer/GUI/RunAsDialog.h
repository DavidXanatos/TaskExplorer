#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_RunAsDialog.h"

class CRunAsDialog : public QMainWindow
{
	Q_OBJECT

public:
	CRunAsDialog(quint64 PID = 0, QWidget *parent = Q_NULLPTR);
	~CRunAsDialog();

public slots:
	void accept();
	void reject();

	void OnBrowse();
	void OnUserName(const QString& userName);

protected:
	void closeEvent(QCloseEvent *e);

	quint64			m_PID;

private:
	Ui::RunAsDialog ui;
};
