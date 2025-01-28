#include "stdafx.h"
#include "WinSvcShutdown.h"
#ifdef WIN32
#include "../../API/Windows/ProcessHacker/PhSvc.h"
#endif


CWinSvcShutdown::CWinSvcShutdown(QWidget *parent)
	: QDialog(parent)
{
	ui.setupUi(this);

	connect(ui.defaultMsg, SIGNAL(pressed()), this, SLOT(OnDefault()));
	connect(ui.buttonBox, SIGNAL(accepted()), this, SLOT(accepted()));
	connect(ui.buttonBox, SIGNAL(rejected()), this, SLOT(rejected()));
}

void CWinSvcShutdown::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CWinSvcShutdown::OnDefault()
{
	ui.msgText->setPlainText(tr("Your computer is connected an other computer. A service on this computer has ended unexpectedly. "
		"The remote computer will restart automatically, and then you can reestablish the connection."));
}

void CWinSvcShutdown::SetRestartTime(quint64 RebootAfter)
{
	ui.afterMin->setText(QString::number(RebootAfter / (1000 * 60)));
}

quint64 CWinSvcShutdown::GetRestartTime()
{
	return ui.afterMin->text().toULongLong() * (1000 * 60);
}

void CWinSvcShutdown::SetMessageText(const QString& text)
{
	ui.sendMsg->setChecked(text.length() > 0);
	ui.msgText->setPlainText(text);
}

QString CWinSvcShutdown::GetMessageText()
{
	if (!ui.sendMsg->isChecked())
		return QString();
	return ui.msgText->toPlainText();
}