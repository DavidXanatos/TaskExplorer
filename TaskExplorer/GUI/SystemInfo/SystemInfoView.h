#pragma once
#include <qwidget.h>
#include "..\..\Common\TabPanel.h"

#include "SystemView.h"
#include "DriversView.h"
#include "../TaskInfo/HandlesView.h"
#include "../TaskInfo/SocketsView.h"
#include "ServicesView.h"

class CSystemInfoView : public CTabPanel
{
	Q_OBJECT
public:
	CSystemInfoView(QWidget* patent = 0);
	virtual ~CSystemInfoView();

public slots:
	void				OnTab(int tabIndex);
	void				Refresh();

protected:
	virtual void		InitializeTabs();

private:
	CSystemView*		m_pSystemView;
	CDriversView*		m_pDriversView;
	CHandlesView*		m_pAllFilesView;
	CSocketsView*		m_pAllSocketsView;
	CServicesView*		m_pServicesView;
};

