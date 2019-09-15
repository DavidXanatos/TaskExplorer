#pragma once
#include <qwidget.h>
#include "../../Common/PanelView.h"
#include "../StatsView.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#endif

class CProcessModel;
class QSortFilterProxyModel;

// Note: CJobView is a windows only class don't include it on a unix build

class CJobView : public CPanelView
{
	Q_OBJECT
public:
	CJobView(QWidget *parent = 0);
	virtual ~CJobView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					ShowJob(const CWinJobPtr& pJob);
	void					Refresh();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

	//void					OnMenu(const QPoint &point);

	void					OnPermissions();
	void					OnTerminate();
	void					OnAddProcess();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pProcessList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }

	QSharedPointer<CWinProcess>	m_pCurProcess;
	CWinJobPtr					m_pCurJob;

	QMap<quint64, CProcessPtr>	m_ProcessList;

private:
	enum EStackColumns
	{
		eName = 0,
		eType,
		eValue,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	//QLabel*					m_pJobNameLabel;
	QLineEdit*				m_pJobName;
	QPushButton*			m_pTerminate;

	QSplitter*				m_pSplitter;

	//QLabel*				m_pJobProcessesLabel;
	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QTreeViewEx*			m_pProcessList;

	QWidget*				m_pSubWidget;
	QGridLayout*			m_pSubLayout;

	QPushButton*			m_pAddProcess;

	QTabWidget*				m_pAdvancedTabs;

	CPanelWidgetEx* m_pLimits;

	//QLabel*					m_pJobStatsLabel;
	CStatsView*				m_pJobStats;

	QPushButton*			m_pPermissions;
};

