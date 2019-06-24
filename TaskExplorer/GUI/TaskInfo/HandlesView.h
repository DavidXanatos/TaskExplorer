#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "../../Common/PanelView.h"
#include "..\..\API\ProcessInfo.h"
#include "..\Models\HandleModel.h"
#include "..\..\Common\SortFilterProxyModel.h"

class CHandlesView : public CPanelView
{
	Q_OBJECT
public:
	CHandlesView(bool bAll = false, QWidget *parent = 0);
	virtual ~CHandlesView();

public slots:
	void					ShowHandles(const CProcessPtr& pProcess);
	void					ShowAllFiles();

private slots:
	//void					OnClicked(const QModelIndex& Index);
	void					OnCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

	//void					OnMenu(const QPoint &point);

	void					OnHandleAction();
	void					OnPermissions();

protected:
	virtual void				OnMenu(const QPoint& Point);
	virtual QTreeView*			GetView() 				{ return m_pHandleList; }
	virtual QAbstractItemModel* GetModel()				{ return m_pSortProxy; }
	//virtual QAbstractItemModel* GetModel()				{ return m_pHandleModel; }
	//virtual QModelIndex			MapToSource(const QModelIndex& Model) { return m_pSortProxy->mapToSource(Model); }

	CProcessPtr				m_pCurProcess;

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;

	QTreeViewEx*			m_pHandleList;
	CHandleModel*			m_pHandleModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QSplitter*				m_pSplitter;

	QScrollArea*			m_pDetailsArea;

	QWidget*				m_pDetailsWidget;
	QVBoxLayout*			m_pDetailsLayout;

	QWidget*				m_pGenericWidget;
	QFormLayout*			m_pGenericLayout;
	QLabel*					m_pName;
	QLabel*					m_pType;
	QLabel*					m_pReferences;
	QLabel*					m_pHandles;
	QLabel*					m_pQuotaPaged;
	QLabel*					m_pQuotaSize;
	QLabel*					m_pGrantedAccess;

	QWidget*				m_pCustomWidget;
	QStackedLayout*			m_pCustomLayout;

	QWidget*				m_pOtherWidget;
	QFormLayout*			m_pOtherLayout;

	QWidget*				m_pPortWidget;
	QFormLayout*			m_pPortLayout;
	QLabel*					m_pPortNumber;
	QLabel*					m_pPortContext;

	QWidget*				m_pFileWidget;
	QFormLayout*			m_pFileLayout;
	QLabel*					m_pFileMode;

	QWidget*				m_pSectionWidget;
	QFormLayout*			m_pSectionLayout;
	QLabel*					m_pSectionType;

	QWidget*				m_pMutantWidget;
	QFormLayout*			m_pMutantLayout;
	QLabel*					m_pMutantCount;
	QLabel*					m_pMutantAbandoned;
	QLabel*					m_pMutantOwner;

	QWidget*				m_pTaskWidget;
	QFormLayout*			m_pTaskLayout;
	QLabel*					m_pTaskCreated;
	QLabel*					m_pTaskExited;
	QLabel*					m_pTaskStatus;



	//QMenu*					m_pMenu;
	QAction*				m_pClose;
	QAction*				m_pProtect;
	QAction*				m_pInherit;

	QAction*				m_pTokenInfo;
	QMenu*					m_pSemaphore;
	QAction*				m_pSemaphoreAcquire;
	QAction*				m_pSemaphoreRelease;
	QMenu*					m_pEvent;
	QAction*				m_pEventSet;
	QAction*				m_pEventReset;
	QAction*				m_pEventPulse;
	QMenu*					m_pTask;
	QAction*				m_pPermissions;
};

