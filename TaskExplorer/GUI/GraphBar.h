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

private:
	void					FixPlotScale(CIncrementalPlot* pPlot);

	QGridLayout*			m_pMainLayout;

	CIncrementalPlot*		m_pMemoryPlot;
	CIncrementalPlot*		m_pObjectPlot;
	CIncrementalPlot*		m_pWindowsPlot;
	CIncrementalPlot*		m_pHandledPlot;
	//CIncrementalPlot*		m_pGpuPlot;
	CIncrementalPlot*		m_pMMapIoPlot;

	CIncrementalPlot*		m_pFileIoPlot;
	//CIncrementalPlot*		m_pDiskIoPlot;
	CIncrementalPlot*		m_pSambaPlot;
	CIncrementalPlot*		m_pNetworkPlot;
	CIncrementalPlot*		m_pRasPlot;
	CIncrementalPlot*		m_pCpuPlot;
};
