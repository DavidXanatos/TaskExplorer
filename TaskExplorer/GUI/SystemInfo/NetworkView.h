#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"
#include "../../Common/SettingsWidgets.h"
#include "../../Common/PanelView.h"


class CNetworkView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CNetworkView(QWidget *parent = 0);
	virtual ~CNetworkView();

public slots:
	void					Refresh();
	void					UpdateGraphs();
	void					ReConfigurePlots();

private slots:
	void					OnResetColumns();

	void					OnAdapterCheck(QTreeWidgetItem* item, int column);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	QSet<QString>			m_Adapters;

	bool					m_HoldValues;
	QStringList				m_HiddenAdapters;

private:
	int						m_PlotLimit;

	enum EColumns
	{
		eAdapter = 0,
		eLinkState,
		eLinkSpeed,

		eReadRate,
		eBytesReadDelta,
		eBytesRead,
		eReadsDelta,
		eReads,
		eWriteRate,
		eBytesWritenDelta,
		eBytesWriten,
		eWritesDelta,
		eWrites,

		eAddress,
		eGateway,
		eDNS,
		eDomain,

		eDeviceInterface,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pScrollWidget;
	QScrollArea*			m_pScrollArea;
	QGridLayout*			m_pScrollLayout;

	//QLabel*					m_pNICModel;
	CIncrementalPlot*		m_pDlPlot;
	CIncrementalPlot*		m_pUlPlot;

	CPanelWidgetEx* m_pNICList;

	QAction*				m_pShowDisconnected;
};

