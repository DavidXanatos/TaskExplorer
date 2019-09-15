#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"
#include "../../Common/PanelView.h"

class CDiskView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CDiskView(QWidget *parent = 0);
	virtual ~CDiskView();

public slots:
	void					Refresh();
	void					UpdateGraphs();
	void					ReConfigurePlots();

private slots:
	void					OnResetColumns();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	QSet<QString>			m_Disks;

private:
	int						m_PlotLimit;

	enum EColumns
	{
		eDiskName = 0,
		eActivityTime,
		eResponseTime,
		eQueueLength,

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

		eDevicePath,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pScrollWidget;
	QScrollArea*			m_pScrollArea;
	QGridLayout*			m_pScrollLayout;

	//QLabel*					m_pModel;

	QTabWidget*				m_pGraphTabs;

	CIncrementalPlot*		m_pDiskPlot;

	QWidget*				m_pWRPlotWidget;
	QVBoxLayout*			m_pWRPlotLayout;

	CIncrementalPlot*		m_pReadPlot;
	CIncrementalPlot*		m_pWritePlot;
	bool					m_bHasUnSupported;

	QWidget*				m_pIOPlotWidget;
	QVBoxLayout*			m_pIOPlotLayout;

	CIncrementalPlot*		m_pFileIOPlot;
	CIncrementalPlot*		m_pMMapIOPlot;

	CPanelWidgetEx* m_pDiskList;
};

