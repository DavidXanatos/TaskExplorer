#pragma once
#include <qwidget.h>

#include "TaskInfoView.h"


class CTaskInfoWindow : public QMainWindow
{
	Q_OBJECT
public:
	CTaskInfoWindow(const CProcessPtr& pProcess, quint64 ThreaId = 0, QWidget *parent = Q_NULLPTR);
	CTaskInfoWindow(QWidget* pSingleTab, const QString& TabName, QWidget *parent = Q_NULLPTR);
	virtual ~CTaskInfoWindow();

protected:
	virtual void		closeEvent(QCloseEvent *e);
	void				timerEvent(QTimerEvent* pEvent);
	
	int					m_uTimerID;

private:
	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pTaskInfo;

	QDialogButtonBox*	m_pButtonBox;
};

