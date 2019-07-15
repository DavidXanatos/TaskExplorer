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

	void					UpdateLengths();

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
		eCount
	};


	/*struct SGraph
	{
		CIncrementalPlot*	pGraph;
		EGraphType			Type;
	};*/

	void					AddGraphs(QList<EGraph> Graphs, int Rows);
	void					AddGraph(EGraph Graph, int row, int column);
	void					DeleteGraphs();

	int						m_Rows;
	typedef QPair<EGraph, CIncrementalPlot*> TGraphPair;
	QList<TGraphPair>		m_Graphs;

	QGridLayout*			m_pMainLayout;

	QPointer<CIncrementalPlot> m_pCurPlot;

	QMenu*					m_pMenu;
	QAction*				m_pResetPlot;
	QAction*				m_pResetAll;
	QAction*				m_pCustomize;

	QWidget*				m_pLastTipGraph;
};
