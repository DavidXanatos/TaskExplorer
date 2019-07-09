#pragma once
#include <qwidget.h>
#include "..\..\API\ProcessInfo.h"
#include "..\..\Common\PanelView.h"
#include "..\..\Common\TreeWidgetEx.h"
#include "..\StatsView.h"

class CSystemView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CSystemView(QWidget *parent = 0);
	virtual ~CSystemView();

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QScrollArea*			m_pScrollArea;

	QWidget*				m_pInfoWidget;
	QVBoxLayout*			m_pInfoLayout;

	QGroupBox*				m_pSystemBox;
	QGridLayout*			m_pSystemLayout;
	
	//QLabel*					m_pCpuModelLabel;
	QLabel*					m_pCpuModel;
	//QLabel*					m_pSystemMemoryLabel;
	QLabel*					m_pSystemMemory;

	//QTreeWidgetEx*			m_pStatsList;
	CStatsView*				m_pStatsView;
};

