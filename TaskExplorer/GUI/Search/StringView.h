#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../../API/SocketInfo.h"
#include "../Models/StringModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"

class CStringView : public CPanelView
{
	Q_OBJECT
public:
	CStringView(bool bGlobal, QWidget *parent = 0);
	virtual ~CStringView();

public slots:
	void					ShowStrings(const QMap<quint64, CStringInfoPtr>& String);

private slots:
	void					OnColumnsChanged();

	void					OnDoubleClicked();

	//void					OnMenu(const QPoint &point);
	void					OnSaveString();

protected:
	virtual void				OnMenu(const QPoint& Point);

	virtual QTreeView*			GetView() 				{ return m_pStringList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pSocketModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }
	
	CProcessPtr				m_pCurProcess;

	QMap<quint64, CStringInfoPtr> m_String;

private:

	QVBoxLayout*			m_pMainLayout;

	QTreeViewEx*			m_pStringList;
	CStringModel*			m_pStringModel;
	CSortFilterProxyModel*	m_pSortProxy;

	//QMenu*					m_pMenu;
	QAction*				m_pMenuEdit;
	QAction*				m_pMenuSave;
};

