#pragma once
#include <qwidget.h>
#include "../../MiscHelpers/Common/PanelView.h"

#include "../API/AbstractTask.h"

class CTaskView : public CPanelView
{
	Q_OBJECT
public:
	CTaskView(QWidget *parent = 0);
	virtual ~CTaskView();

private slots:
	void					OnTaskAction();

	void					OnAffinity();
	void					OnBoost();
	void					OnPriority();

protected:
	virtual QList<CTaskPtr>	GetSellectedTasks() = 0;

	virtual void			OnMenu(const QPoint& Point);

	virtual void			AddTaskItemsToMenu();

	enum EPriorityType
	{
		eInvalid = 0,
		eProcess,
		eThread,
		eIO,
		ePage
	};

	virtual void			AddPriorityItemsToMenu(EPriorityType Style);

	// Task Items
	QAction*				m_pTerminate;
	QAction*				m_pSuspend;
	QAction*				m_pResume;

	// Priority Items
	QAction*				m_pAffinity;
	QAction*				m_pBoost;

	QMenu*					m_pPriority;
	
	QMenu*					m_pIoPriority;
	
	QMenu*					m_pPagePriority;

private:
	struct SPriority
	{
		SPriority(EPriorityType type = eInvalid, int value = 0) {
			Type = type;
			Value = value;
		}
		EPriorityType Type;
		int Value;
	};

	void					AddPriority(EPriorityType Type, const QString& Name, int Value);

	QMap<QAction*, SPriority>	m_pPriorityLevels;
};
