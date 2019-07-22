#pragma once
#include <qwidget.h>

#include "SystemInfoView.h"


class CSystemInfoWindow : public QMainWindow
{
	Q_OBJECT
public:
	CSystemInfoWindow(QWidget *parent = Q_NULLPTR);
	virtual ~CSystemInfoWindow();

protected:
	virtual void		closeEvent(QCloseEvent *e);
	void				timerEvent(QTimerEvent* pEvent);
	
	int					m_uTimerID;

private:
	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	QWidget*			m_pSystemInfo;

	QDialogButtonBox*	m_pButtonBox;
};

