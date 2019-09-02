#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"
#include "../../Common/PanelView.h"
#include "../../Common/SmartGridWidget.h"

class CGPUView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CGPUView(QWidget *parent = 0);
	virtual ~CGPUView();

public slots:
	void					Refresh();
	void					UpdateGraphs();
	void					ReConfigurePlots();

private slots:
	void					OnMultiPlot(int State);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	QSet<QString>			m_Adapters;

private:
	int						m_PlotLimit;

	enum EColumns
	{
		eModelName = 0,
		eLocation,
		eDriverVersion,
		eHwID,

		eDedicatedUsage,
		eDedicatedLimit,
		eSharedUsage,
		eSharedLimit,

		eDeviceInterface,

		eCount
	};

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pScrollWidget;
	QScrollArea*			m_pScrollArea;
	QGridLayout*			m_pScrollLayout;

	QTabWidget*				m_pGraphTabs;

	QWidget*				m_pPlotWidget;
	QVBoxLayout*			m_pPlotLayout;

	//QLabel*					m_pGPUModel;
	CIncrementalPlot*		m_pGPUPlot;
	CIncrementalPlot*		m_pVRAMPlot;

	QCheckBox*				m_pMultiGraph;	

	struct SNodePlots
	{
		SNodePlots() : pStackedWidget(NULL), pStackedLayout(NULL), pPlot(NULL), pGrid(NULL) {}

		QWidget*				pStackedWidget;
		QStackedLayout*			pStackedLayout;
		CIncrementalPlot*		pPlot;
		CSmartGridWidget*		pGrid;
	};

	QMap<QString, SNodePlots>m_NodePlots;
	
	CPanelWidgetEx* m_pGPUList;
};

