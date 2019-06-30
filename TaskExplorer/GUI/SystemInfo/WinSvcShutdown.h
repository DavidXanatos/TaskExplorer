#pragma once

#include <QtWidgets/QMainWindow>
#include "ui_WinSvcShutdown.h"

class CWinSvcShutdown : public QDialog
{
	Q_OBJECT

public:
	CWinSvcShutdown(QWidget *parent = Q_NULLPTR);

	void SetRestartTime(quint64 RebootAfter);
	quint64 GetRestartTime();
	void SetMessageText(const QString& text);
	QString GetMessageText();


public slots:
	void OnDefault();

protected:
	void closeEvent(QCloseEvent *e);

private:
	Ui::WinSvcShutdown ui;
};
