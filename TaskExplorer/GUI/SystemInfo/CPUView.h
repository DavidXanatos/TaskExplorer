#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"

class CCPUView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CCPUView(QWidget *parent = 0);
	virtual ~CCPUView();

public slots:
	void					Refresh();
	void					UpdateGraphs();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	QVector<QPair<SSmoother, SSmoother> > m_CpuSmoother;

private:
	QGridLayout*			m_pMainLayout;

	QWidget*				m_pScrollWidget;
	QScrollArea*			m_pScrollArea;
	QGridLayout*			m_pScrollLayout;

	QLabel*					m_pCPUModel;
	CIncrementalPlot*		m_pCPUPlot;

	QWidget*				m_pInfoWidget;
	QHBoxLayout*			m_pInfoLayout;

	QWidget*				m_pInfoWidget1;
	QVBoxLayout*			m_pInfoLayout1;

	QWidget*				m_pInfoWidget2;
	QVBoxLayout*			m_pInfoLayout2;

	QGroupBox*				m_pCPUBox;
	QGridLayout*			m_pCPULayout;
	QLabel*					m_pCPUUsage;
	QLabel*					m_pCPUSpeed;
	QLabel*					m_pCPUNumaCount;
	QLabel*					m_pCPUCoreCount;

	QGroupBox*				m_pOtherBox;
	QGridLayout*			m_pOtherLayout;
	QLabel*					m_pSwitches;
	QLabel*					m_pInterrupts;
	QLabel*					m_pDPCs;
	QLabel*					m_pSysCalls;
};

