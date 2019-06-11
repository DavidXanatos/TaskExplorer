#pragma once
#include <qwidget.h>
#include "../Common/IncrementalPlot.h"

/*
	Process Explorer Layout:
	+-----------------------+-----------------------+-----------------------+-----------------------+-----------------------+
	|	Memory All In One	|	USER/GDI Obj		|	Windows				|	Handles				|	MMap IO				|
	+-----------------------+-----------------------+-----------------------+-----------------------+-----------------------+
	|	File IO				|	Samba Share C/S		|	TCP/IP				|	GPU Usage			|	CPUs All In One		|
	+-----------------------+-----------------------+-----------------------+-----------------------+-----------------------+
*/

class CGraphBar : public QWidget
{
	Q_OBJECT

public:
	CGraphBar();
	virtual ~CGraphBar();

public slots:
	void					UpdateGraphs();

private:
	QGridLayout*			m_pMainLayout;

	CIncrementalPlot*		m_pMemoryPlot;
	CIncrementalPlot*		m_pObjectPlot;
	CIncrementalPlot*		m_pWindowsPlot;
	CIncrementalPlot*		m_pHandledPlot;
	CIncrementalPlot*		m_pMMapIoPlot;

	CIncrementalPlot*		m_pFileIoPlot;
	CIncrementalPlot*		m_pSambaPlot;
	CIncrementalPlot*		m_pNetworkPlot;
	CIncrementalPlot*		m_pGpuPlot;
	CIncrementalPlot*		m_pCpuPlot;
};
