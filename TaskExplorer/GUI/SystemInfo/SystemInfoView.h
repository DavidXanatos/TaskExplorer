#pragma once
#include <qwidget.h>

#include "SystemOverview.h"
#include "SystemPerfMon.h"
#include "DriversView.h"
#include "../TaskInfo/HandlesView.h"
#include "../TaskInfo/SocketsView.h"
#include "ServicesView.h"

class CSystemInfoView : public QWidget
{
	Q_OBJECT
public:
	CSystemInfoView();
	virtual ~CSystemInfoView();

public slots:
	void				OnTab(int tabIndex);
	void				Refresh();

private:
	QVBoxLayout*		m_pMainLayout;

	QTabWidget*			m_pTabs;

	CSystemOverview*	m_pSystemOverview;
	CSystemPerfMon*		m_pSystemPerfMon;
	CDriversView*		m_pDriversView;
	CHandlesView*		m_pAllFilesView;
	CSocketsView*		m_pAllSocketsView;
	CServicesView*		m_pServicesView;
};

