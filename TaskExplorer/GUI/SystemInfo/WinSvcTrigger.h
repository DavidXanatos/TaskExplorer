#pragma once

#include <QtWidgets/QDialog>
#include "ui_WinSvcTrigger.h"

class CWinSvcTrigger : public QDialog
{
	Q_OBJECT

public:
	CWinSvcTrigger(QWidget *parent = Q_NULLPTR);
	~CWinSvcTrigger();

	void InitInfo();
	void SetInfo(void* pInfo);
	void* GetInfo() { ASSERT(m_pInfo != NULL); return m_pInfo; }

public slots:
	void accept();
	void reject();

	void OnData(QTreeWidgetItem *item, int column);
	void OnNewTrigger();
	void OnEditTrigger();
	void OnDeleteTrigger();

	void FixServiceTriggerControls();

protected:
	void SetInfo();
	void closeEvent(QCloseEvent *e);

	void*	m_pInfo;

	QString	m_LastCustomSubType;
	quint32	m_LastSelectedType;
	bool m_NoFixServiceTriggerControls;

private:

	Ui::WinSvcTrigger ui;
};
