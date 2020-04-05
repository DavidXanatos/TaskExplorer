#pragma once
#include <qwidget.h>
#include "../../Common/TabPanel.h"

class CSystemView;
class CHandlesView;
class CSocketsView;
class CServicesView;
//class CDriversView;
#ifdef WIN32
class CKernelView;
#endif
class CDnsCacheView;
class CCPUView;
class CRAMView;
class CDiskView;
class CNetworkView;
class CGPUView;

class CSystemInfoView : public CTabPanel
{
	Q_OBJECT
public:
	CSystemInfoView(QWidget* patent = 0);
	virtual ~CSystemInfoView();

public slots:
	void				OnTab(int tabIndex);
	void				Refresh();
	void				UpdateGraphs();

protected:
	virtual void		InitializeTabs();

private:
	CSystemView*		m_pSystemView;
	CHandlesView*		m_pAllFilesView;
	CSocketsView*		m_pAllSocketsView;
	CDnsCacheView*		m_pDnsCacheView;
	CServicesView*		m_pServicesView;
	//CDriversView*		m_pDriversView;
#ifdef WIN32
	CKernelView*		m_pKernelView;
#endif
	CCPUView*			m_pCPUView;
	CRAMView*			m_pRAMView;
	CDiskView*			m_pDiskView;
	CNetworkView*		m_pNetworkView;
	CGPUView*			m_pGPUView;
};

