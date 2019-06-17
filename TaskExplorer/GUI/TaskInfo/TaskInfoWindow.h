#pragma once
#include <qwidget.h>

#include "TaskInfoView.h"


class CTaskInfoWindow : public QMainWindow
{
	Q_OBJECT
public:
	CTaskInfoWindow(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskInfoWindow();

public slots:
	void				ShowProcess(const CProcessPtr& pProcess);

protected:
	virtual void		closeEvent(QCloseEvent *e);
	void				timerEvent(QTimerEvent* pEvent);
	
	int					m_uTimerID;

private:
	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	CTaskInfoView*		m_pTaskInfo;

	QDialogButtonBox*	m_pButtonBox;
};

