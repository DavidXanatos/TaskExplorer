#pragma once

#include <QtWidgets/QMainWindow>
#include "GraphBar.h"
#include "ProcessTree.h"
#include "SystemInfo\SystemInfoView.h"
#include "TaskInfo\TaskInfoView.h"
#include "API\SystemAPI.h"

class CTaskExplorer : public QMainWindow
{
	Q_OBJECT

public:
	CTaskExplorer(QWidget *parent = Q_NULLPTR);
	virtual ~CTaskExplorer();

protected:
	void				timerEvent(QTimerEvent* pEvent);
	void				closeEvent(QCloseEvent *e);

	void				UpdateAll();

	//quint16				m_uTimerCounter;
	int					m_uTimerID;

private:
	//Ui::CTaskExplorerClass ui;

	QWidget*			m_pMainWidget;
	QVBoxLayout*		m_pMainLayout;

	CGraphBar*			m_pGraphBar;

	QSplitter*			m_pMainSplitter;

	CProcessTree*		m_pProcessTree;

	QSplitter*			m_pSubSplitter;

	CSystemInfoView*	m_pSystemInfo;

	CTaskInfoView*		m_pTaskInfo;
};

extern CSystemAPI*	theAPI;

extern QIcon g_ExeIcon;
extern QIcon g_DllIcon;