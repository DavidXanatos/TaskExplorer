#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../API/DriverInfo.h"
#include "../Models/DnsModel.h"

class QSortFilterProxyModel;

class CDnsCacheView : public CPanelView
{
	Q_OBJECT

public:
	CDnsCacheView(bool bAll, QWidget *parent = 0);
	virtual ~CDnsCacheView();

	//void					OnMenu(const QPoint &point);

public slots:
	//void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

private slots:
	void					OnResetColumns();
	void					OnColumnsChanged();

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pDnsList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pSortProxy; }

	/*enum EView
	{
		eNone,
		eSingle,
		eMulti
	};

	virtual void			SwitchView(EView ViewMode);

	EView					m_ViewMode;*/

	QMultiMap<QString, CDnsCacheEntryPtr> m_DnsCacheList;

private slots:
	void					OnDnsCacheUpdated();

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pDnsList;
	CDnsModel*				m_pDnsModel;
	QSortFilterProxyModel*	m_pSortProxy;
};

