#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/IncrementalPlot.h"

class CPoolView;
class CDriversView;
class CNtObjectView;
class CAtomView;

class CKernelView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CKernelView(QWidget *parent = 0);
	virtual ~CKernelView();

public slots:
	void					Refresh();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

private:

	QTabWidget*				m_pTabs;
	
	CPoolView*				m_pPoolView;
	CDriversView*			m_pDriversView;
	CNtObjectView*			m_pNtObjectView;
	CAtomView*				m_pAtomView;

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pInfoWidget;
	QHBoxLayout*			m_pInfoLayout;

	QWidget*				m_pInfoWidget1;
	QVBoxLayout*			m_pInfoLayout1;

	QWidget*				m_pInfoWidget2;
	QVBoxLayout*			m_pInfoLayout2;

	///////////////////

	QGroupBox*				m_pPPoolBox;
	QGridLayout*			m_pPPoolLayout;
	QLabel*					m_pPPoolWS;
	QLabel*					m_pPPoolVSize;
	//QLabel*					m_pPPoolLimit;

	QGroupBox*				m_pNPPoolBox;
	QGridLayout*			m_pNPPoolLayout;
	QLabel*					m_pNPPoolUsage;
	//QLabel*					m_pNPPoolLimit;
};

