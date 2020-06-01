#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"

class CRAMView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CRAMView(QWidget *parent = 0);
	virtual ~CRAMView();

public slots:
	void					Refresh();
	void					UpdateGraphs();
	void					ReConfigurePlots();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

private:
	int						m_PlotLimit;

	enum EColumns
	{
		eFileName = 0,
		eUsage,
		ePeakUsage,
		eTotalSize,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pScrollWidget;
	QScrollArea*			m_pScrollArea;
	QGridLayout*			m_pScrollLayout;

	QLabel*					m_pRAMSize;
	CIncrementalPlot*		m_pRAMPlot;

	QTabWidget*				m_pInfoTabs;

	QWidget*				m_pMemoryWidget;
	QGridLayout*			m_pMemoryLayout;

	QGroupBox*				m_pVMemBox;
	QGridLayout*			m_pVMemLayout;
	QLabel*					m_pVMemCurrent;
	QLabel*					m_pVMemPeak;
	QLabel*					m_pVMemLimit;
	QLabel*					m_pSwapSize;

	QGroupBox*				m_pRAMBox;
	QGridLayout*			m_pRAMLayout;
	QLabel*					m_pRAMUsed;
	QLabel*					m_pRAMTotal;
	QLabel*					m_pRAMReserved;
	QLabel*					m_pRAMCacheWS;
	QLabel*					m_pRAMKernelWS;
	QLabel*					m_pRAMDriverWS;


	QWidget*				m_pPageWidget;
	QGridLayout*			m_pPageLayout;

	QGroupBox*				m_pPagingBox;
	QGridLayout*			m_pPagingLayout;
	QLabel*					m_pPagingFault;
	QLabel*					m_pPagingReads;
	QLabel*					m_pPagingFileWrites;
	QLabel*					m_pMappedWrites;

	QGroupBox*				m_pSwapBox;
	QGridLayout*			m_pSwapLayout;

	CPanelWidgetEx*			m_pSwapList;

#ifdef WIN32
	QWidget*				m_pListsWidget;
	QGridLayout*			m_pListsLayout;

	QGroupBox*				m_pListBox;
	QGridLayout*			m_pListLayout;
	QLabel*					m_pZeroed;
	QLabel*					m_pFree;
	QLabel*					m_pModified;
	QLabel*					m_pModifiedNoWrite;
	QLabel*					m_pModifiedPaged;

	QGroupBox*				m_pStanbyBox;
	QGridLayout*			m_pStanbyLayout;
	QLabel*					m_pStandby;
	QLabel*					m_pPriority0;
	QLabel*					m_pPriority1;
	QLabel*					m_pPriority2;
	QLabel*					m_pPriority3;
	QLabel*					m_pPriority4;
	QLabel*					m_pPriority5;
	QLabel*					m_pPriority6;
	QLabel*					m_pPriority7;
#endif
};

