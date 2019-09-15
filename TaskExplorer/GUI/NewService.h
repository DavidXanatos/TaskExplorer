#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_NewService.h"

class CNewService : public QMainWindow
{
	Q_OBJECT

public:
	CNewService(QWidget *parent = Q_NULLPTR);
	~CNewService();

public slots:
	void accept();
	void reject();

	void OnBrowse();

protected:
	void closeEvent(QCloseEvent *e);

private:
	Ui::NewService ui;
};
