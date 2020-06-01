#pragma once
#include <qwidget.h>
#include "../../../MiscHelpers/Common/TreeViewEx.h"
#include "../../../MiscHelpers/Common/TreeWidgetEx.h"
#include "../../../MiscHelpers/Common/PanelView.h"
#include "../../API/ProcessInfo.h"
#include "../Models/GDIModel.h"
#include "../../../MiscHelpers/Common/SortFilterProxyModel.h"


class CDebugView : public CPanelWidgetEx
{
	Q_OBJECT
public:
	CDebugView(QWidget *parent = 0);
	virtual ~CDebugView();

public slots:
	void					ShowProcesses(const QList<CProcessPtr>& Processes);
	void					Refresh();

private slots:
	void					OnResetColumns();

	//void					OnMenu(const QPoint &point);
	//void					OnItemDoubleClicked(const QModelIndex& Index);

protected:
	enum EColumns
	{
		eProcess = 0,
		eTimeStamp,
		eMessage,
		eCount
	};

	enum EView
	{
		eNone,
		eSingle,
		eMulti
	};

	virtual void			SwitchView(EView ViewMode);

	EView					m_ViewMode;

	QList<CProcessPtr>		m_Processes;
	QMap<quint64, quint32>	m_CounterMap;

private:


};

