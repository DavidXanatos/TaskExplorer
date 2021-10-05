#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TabPanel.h"

class CSystemView;
class CHandlesView;
class CSocketsView;
class CServicesView;
#ifdef WIN32
class CRpcView;
#endif
//class CDriversView;
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
	CSystemInfoView(bool bAsWindow = false, QWidget* patent = 0);
	virtual ~CSystemInfoView();

	enum ETabs
	{
		eSystemView = 0,
		eCPUView,
		eRAMView,
		eGPUView,
		eDiskView,
		eAllFilesView,
		eNetworkView,
		eAllSocketsView,
		eDnsCacheView,
		//eDriversView,
		eKernelView,
		eServicesView,
		eTabCount
	};

public slots:
	void				OnTab(int tabIndex);
	void				Refresh();
	void				UpdateGraphs();

protected:
	virtual void		InitializeTabs();

	bool				m_bAsWindow;

private:
	CSystemView*		m_pSystemView;
	CHandlesView*		m_pAllFilesView;
	CSocketsView*		m_pAllSocketsView;
	CDnsCacheView*		m_pDnsCacheView;
	CServicesView*		m_pServicesView;
#ifdef WIN32
	CRpcView*			m_pRpcView;
#endif
	//CDriversView*		m_pDriversView;
	CCPUView*			m_pCPUView;
	CRAMView*			m_pRAMView;
	CDiskView*			m_pDiskView;
	CNetworkView*		m_pNetworkView;
	CGPUView*			m_pGPUView;
};

