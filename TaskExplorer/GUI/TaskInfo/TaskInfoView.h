#pragma once
#include <qwidget.h>
#include "..\..\Common\TabPanel.h"
#include "..\..\API\ProcessInfo.h"

#include "ProcessView.h"
#include "HandlesView.h"
#include "SocketsView.h"
#include "ThreadsView.h"
#include "ModulesView.h"
#include "MemoryView.h"
#include "JobView.h"
#include "TokensView.h"
#include "WindowsView.h"
#include "EnvironmentView.h"


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
	//CJobView*			m_pJobView;
	CEnvironmentView*	m_pEnvironmentView;
};

