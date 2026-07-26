#pragma once
#include <qwidget.h>
#include "../Common/IncrementalPlot.h"


class CGraphBar : public QWidget
{
	Q_OBJECT

public:
	CGraphBar();
	virtual ~CGraphBar();

public slots:
	void					UpdateGraphs();
	void					CustomizeGraphs();

	// Rebuilds the graph bar from the platform's default set; see
	// GetDefaultGraphs() for why this needs to be reachable.
	void					RestoreDefaultGraphs();

	void					ReConfigurePlots();
	void					SetDarkMode(bool bDark);

private slots:	
	void					OnMenu(const QPoint& Point);

	void					ClearGraphs();

	//void					OnEntered();
	//void					OnMoveed(QMouseEvent* event);
	//void					OnExited();
	void					OnToolTipRequested(QEvent* event);

signals:
	void					Resized(int Size);

private:
	void					FixPlotScale(CIncrementalPlot* pPlot);

	enum EGraph
	{
		eMemoryPlot = 0,
		eGpuMemPlot,
		eObjectPlot,
		eWindowsPlot,
		eHandledPlot,
		eDiskIoPlot,
		eMMapIoPlot,
		eFileIoPlot,
		eSambaPlot,
		eClientPlot,
		eServerPlot,
		eRasPlot,
		eNetworkPlot,
		eGpuPlot,
		eCpuPlot,
#ifndef WIN32
		// Pressure Stall Information; Linux only, there is no Windows analogue.
		ePressurePlot,
#endif
		eCount
	};


	/*struct SGraph
	{
		CIncrementalPlot*	pGraph;
		EGraphType			Type;
	};*/

	// The plots shown when nothing has been configured. Platform dependent.
	static QList<EGraph>	GetDefaultGraphs();

	// Persists the current layout to the settings.
	void					SaveGraphs();

	void					AddGraphs(QList<EGraph> Graphs, int Rows);
	void					AddGraph(EGraph Graph, int row, int column);
	void					DeleteGraphs();

	int						m_Rows;

	int						m_PlotLimit;

	struct SGraph
	{
		SGraph()
		{
			Type = eCount;
			pPlot = NULL;
		}

		EGraph Type;
		CIncrementalPlot* pPlot;
		QVariantMap Params;
	};
	QList<SGraph>			m_Graphs;

	QGridLayout*			m_pMainLayout;

	QPointer<CIncrementalPlot> m_pCurPlot;

	QMenu*					m_pMenu;
	QAction*				m_pResetPlot;
	QAction*				m_pResetAll;
	QAction*				m_pCustomize;
	QAction*				m_pRestoreDefaults;

	QWidget*				m_pLastTipGraph;
};
