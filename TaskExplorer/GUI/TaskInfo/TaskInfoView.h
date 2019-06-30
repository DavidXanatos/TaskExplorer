#pragma once
#include <qwidget.h>
#include "..\..\Common\TabPanel.h"
#include "..\..\API\ProcessInfo.h"

class CProcessView;
class CHandlesView;
class CSocketsView;
class CThreadsView;
class CModulesView;
class CWindowsView;
//class CMemoryView;
//class CTokensView;
class CJobView;
class CServicesView;
class CEnvironmentView;


class CTaskInfoView : public CTabPanel
{
	Q_OBJECT
public:
	CTaskInfoView(bool bAsWindow = false, QWidget* patent = 0);
	virtual ~CTaskInfoView();

public slots:
	void				OnTab(int tabIndex);
	void				ShowProcess(const CProcessPtr& pProcess);
	void				Refresh();

protected:
	virtual void		InitializeTabs();

	CProcessPtr			m_pCurProcess;

private:
	CProcessView*		m_pProcessView;
	CHandlesView*		m_pHandlesView;
	CSocketsView*		m_pSocketsView;
	CThreadsView*		m_pThreadsView;
	CModulesView*		m_pModulesView;
	CWindowsView*		m_pWindowsView;
	//CMemoryView*		m_pMemoryView;
	//CTokensView*		m_pTokensView;
#ifdef WIN32
	CJobView*			m_pJobView;
	CServicesView*		m_pServiceView;
#endif
	CEnvironmentView*	m_pEnvironmentView;
};

