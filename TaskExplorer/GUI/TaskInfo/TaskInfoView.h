#pragma once
#include <qwidget.h>
#include "../../Common/TabPanel.h"
#include "../../API/ProcessInfo.h"

class CProcessView;
class CHandlesView;
class CSocketsView;
class CThreadsView;
class CModulesView;
class CWindowsView;
class CMemoryView;
class CTokenView;
class CJobView;
class CServicesView;
class CDotNetView;
class CGDIView;
class CEnvironmentView;


class CTaskInfoView : public CTabPanel
{
	Q_OBJECT
public:
	CTaskInfoView(bool bAsWindow = false, QWidget* patent = 0);
	virtual ~CTaskInfoView();

public slots:
	void				OnTab(int tabIndex);
	//void				ShowProcess(const CProcessPtr& pProcess);
	void				ShowProcesses(const QList<CProcessPtr>& Processes);
	void				SellectThread(quint64 ThreadId);
	void				Refresh();

protected:
	virtual void		InitializeTabs();

	QList<CProcessPtr>  m_Processes;

private:
	CProcessView*		m_pProcessView;
	CHandlesView*		m_pHandlesView;
	CSocketsView*		m_pSocketsView;
	CThreadsView*		m_pThreadsView;
	CModulesView*		m_pModulesView;
	CWindowsView*		m_pWindowsView;
	CMemoryView*		m_pMemoryView;
#ifdef WIN32
	CTokenView*			m_pTokenView;
	CJobView*			m_pJobView;
	CServicesView*		m_pServiceView;
	CDotNetView*		m_pDotNetView;
	CGDIView*			m_pGDIView;
#endif
	CEnvironmentView*	m_pEnvironmentView;
};

