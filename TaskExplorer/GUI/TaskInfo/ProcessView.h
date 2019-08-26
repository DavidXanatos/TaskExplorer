#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../StatsView.h"

class CProcessModel;
class QSortFilterProxyModel;

class CProcessView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CProcessView(QWidget *parent = 0);
	virtual ~CProcessView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	QList<CProcessPtr>		m_Processes;

private:
	QVBoxLayout*			m_pMainLayout;

	QScrollArea*			m_pScrollArea;

	QWidget*				m_pInfoWidget;
	QVBoxLayout*			m_pInfoLayout;

	QWidget*				m_pStackedWidget;
	QStackedLayout*			m_pStackedLayout;

	QWidget*				m_pOneProcWidget;
	QVBoxLayout*			m_pOneProcLayout;

	QGroupBox*				m_pFileBox;
	QGridLayout*			m_pFileLayout;
	QLabel*					m_pIcon;
	QLabel*					m_pProcessName;
	QLabel*					m_pCompanyName;
	QLabel*					m_pProcessVersion;
	QLineEdit*				m_pFilePath;

	QGroupBox*				m_pProcessBox;
	QGridLayout*			m_pProcessLayout;
	QLineEdit*				m_pCmdLine;
	QLineEdit*				m_pCurDir;
	QLineEdit*				m_pUserName;
	QLineEdit*				m_pProcessId;
	QLineEdit*				m_pStartedBy;

	QWidget*				m_pMultiProcWidget;
	QVBoxLayout*			m_pMultiProcLayout;

	CProcessModel*			m_pProcessModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QTreeViewEx*			m_pProcessList;

#ifdef WIN32
	QLineEdit*				m_pPEBAddress;
	QLabel*					m_ImageType;
	QLineEdit*				m_pMitigation;
	QLabel*					m_Protecetion;
#endif

	CStatsView*				m_pStatsView;
};

