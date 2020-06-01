#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"

class CStatsView;
#ifdef WIN32
class CPoolView;
class CDriversView;
class CNtObjectView;
class CAtomView;
class CRunObjView;
#endif

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

private:
	QVBoxLayout*			m_pMainLayout;

	QScrollArea*			m_pScrollArea;

	QWidget*				m_pInfoWidget;
	QVBoxLayout*			m_pInfoLayout;

	QGroupBox*				m_pSystemBox;
	QGridLayout*			m_pSystemLayout;
	QLabel*					m_pIcon;
	QLabel*					m_pSystemName;
	QLabel*					m_pSystemType;
	QLabel*					m_pSystemVersion;
	QLabel*					m_pSystemBuild;

	//QLabel*					m_pUpTime;
	//QLabel*					m_pHostName;
	//QLabel*					m_pUserName;
	//QLineEdit*				m_pSystemDir;

	QTabWidget*				m_pTabs;
	
	CStatsView*				m_pStatsView;
#ifdef WIN32
	CPoolView*				m_pPoolView;
	CDriversView*			m_pDriversView;
	CNtObjectView*			m_pNtObjectView;
	CAtomView*				m_pAtomView;
	CRunObjView*			m_pRunObjView;
#endif
};

