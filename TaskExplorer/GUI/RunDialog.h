#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_RunDialog.h"

class CRunDialog : public QMainWindow
{
	Q_OBJECT

public:
	CRunDialog(QWidget *parent = Q_NULLPTR);
	~CRunDialog();

public slots:
	void accept();
	void reject();

	void OnBrowse();
	void OnInjectDll();
	void OnDllPath();

protected:
	void closeEvent(QCloseEvent *e);
	bool event(QEvent* event);

private:
	Ui::RunDialog ui;
};
