#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/TreeWidgetEx.h"
#include "../../Common/PanelView.h"
#include "../../API/ProcessInfo.h"

class CStackView : public CPanelView
{
	Q_OBJECT
public:
	CStackView(QWidget *parent = 0);
	virtual ~CStackView();

public slots:
	void					Clear()			{ m_pStackList->clear(); }
	void					ShowStack(const CStackTracePtr& StackTrace);

	//void					OnMenu(const QPoint &point);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pStackList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pStackList->model(); }

private:
	enum EStackColumns
	{
		eStack = 0,
		eName,
		eStackAddress,
		eFrameAddress,
		eControlAddress,
		eReturnAddress,
		eStackParameter,
		eFileInfo,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	QTreeWidgetEx*			m_pStackList;

	//QMenu*					m_pMenu;
};
