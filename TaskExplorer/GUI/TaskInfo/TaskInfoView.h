#pragma once
#include <qwidget.h>
#include "..\..\API\Process.h"

#include "TaskOverview.h"
#include "TaskPerfMon.h"
#include "HandlesView.h"
#include "SocketsView.h"
#include "ThreadsView.h"
#include "ModulesView.h"
#include "MemoryView.h"
#include "TokensView.h"
#include "WindowsView.h"
#include "EnvironmentView.h"


class CTaskInfoView : public QWidget
{
	Q_OBJECT
public:
	CTaskInfoView();
	virtual ~CTaskInfoView();

public slots:
	void				ShowProcess(const CProcessPtr& pProcess);

private:
	QVBoxLayout*		m_pMainLayout;

	QTabWidget*			m_pTabs;

	CTaskOverview*		m_pTaskOverview;
	CTaskPerfMon*		m_pTaskPerfMon;
	CHandlesView*		m_pHandlesView;
	CSocketsView*		m_pSocketsView;
	CThreadsView*		m_pThreadsView;
	CModulesView*		m_pModulesView;
	CMemoryView*		m_pMemoryView;
	CTokensView*		m_pTokensView;
	CWindowsView*		m_pWindowsView;
	CEnvironmentView*	m_pEnvironmentView;
};

