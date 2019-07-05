#pragma once
#include <qwidget.h>
#include "..\..\Common\PanelView.h"
#include "..\..\Common\TreeWidgetEx.h"
#include "..\..\API\ProcessInfo.h"

class CTokenView : public CPanelView
{
	Q_OBJECT
public:
	CTokenView(QWidget *parent = 0);
	virtual ~CTokenView();


public slots:
	void					ShowToken(const CProcessPtr& pProcess);

private slots:
	//void					OnMenu(const QPoint &point);

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_TokenList; }
	virtual QAbstractItemModel* GetModel()				{ return m_TokenList->model(); }

	CProcessPtr				m_pCurProcess;

private:
	enum EStackColumns
	{
		eName = 0,
		eStatus,
		eDescription,
		eCount
	};

	QGridLayout*			m_pMainLayout;

	QTreeWidgetEx*			m_TokenList;
};

