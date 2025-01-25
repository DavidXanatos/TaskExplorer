#pragma once

#include <QtWidgets/QMainWindow>
#include "../API/Windows/WinProcess.h"
#include "../../MiscHelpers/Common/PanelView.h"

class CWaitChainDialog : public QMainWindow
{
	Q_OBJECT

public:
	CWaitChainDialog(const CProcessPtr& pProcess, QWidget *parent = Q_NULLPTR);
	CWaitChainDialog(const CThreadPtr& pThread, QWidget *parent = Q_NULLPTR);
	~CWaitChainDialog();

public slots:
	//void accept();
	void reject();


protected slots:
	void				UpdateThreads();

protected:
	void				InitGUI();
	STATUS				InitWCT();
	void				UnInitWCT();
	//void				UpdateThread(void* ThreadId, QMap<quint64, QTreeWidgetItem*>& OldNodes);
	void				UpdateThread(ulong nodeInfoLength, struct _WAITCHAIN_NODE_INFO* nodeInfoArray, bool isDeadLocked, QMap<quint64, QTreeWidgetItem*>& OldNodes);

	bool Refresh();

	static QString		GetWCTObjType(int ObjectType);
	static QString		GetWCTObjStatus(int ObjectStatus);

	void closeEvent(QCloseEvent *e);
	void timerEvent(QTimerEvent *e);

	int					m_TimerId;

	QMap<quint64, QTreeWidgetItem*> m_FailtList;

	enum EColumns
	{
		eType = 0,
		eThreadId,
		eProcessId,
		eStatus,
		eContextSwitches,
		eWaitTime,
		eTimeout,
		eAlertable,
		eName,
		eCount
	};

	QMap<quint64, QTreeWidgetItem*>	m_pThreadNodes;

	QThread*			m_pWorker;

private:

	QWidget*			m_pMainWidget;
    QGridLayout*		m_pMainLayout;
	CPanelWidgetEx*		m_pWaitTree;
	QDialogButtonBox*	m_pButtonBox;
    
	struct SWaitChainTraversal* m;
};
