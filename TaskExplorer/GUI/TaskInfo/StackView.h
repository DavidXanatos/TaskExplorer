#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"

class CFinder;

class CStackView : public CPanelView
{
	Q_OBJECT
public:
	CStackView(QWidget *parent = 0);
	virtual ~CStackView();

public slots:
	void					Clear()			{ m_pStackList->clear(); }
	void					Invalidate();
	void					ShowStack(const CStackTracePtr& StackTrace);

	//void					OnMenu(const QPoint &point);

	void					SetFilter(const QRegExp& Exp, bool bHighLight = false, int Col = -1); // -1 = any

protected:
	//virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView()	{ return m_pStackList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pStackList->model(); }

private:
	enum EStackColumns
	{
		eStack = 0,
		eSymbol,
		eStackAddress,
		eFrameAddress,
		eControlAddress,
		eReturnAddress,
		eStackParameter,
		eFileInfo,
		eCount
	};

	QVBoxLayout*			m_pMainLayout;

	bool					m_bIsInvalid;
	QTreeWidgetEx*			m_pStackList;

	CFinder*				m_pFinder;

	//QMenu*					m_pMenu;
};
