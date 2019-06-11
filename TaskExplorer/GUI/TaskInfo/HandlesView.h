#pragma once
#include <qwidget.h>
#include "../../Common/TreeViewEx.h"
#include "..\..\API\ProcessInfo.h"

class CHandleModel;
class QSortFilterProxyModel;

class CHandlesView : public QWidget
{
	Q_OBJECT
public:
	CHandlesView(bool bAll = false);
	virtual ~CHandlesView();

public slots:
	void					ShowHandles(const CProcessPtr& pProcess);
	void					ShowAllFiles();

protected:
	CProcessPtr				m_pCurProcess;

private slots:
	void					OnClicked(const QModelIndex& Index);

private:
	QVBoxLayout*			m_pMainLayout;

	QWidget*				m_pFilterWidget;

	QTreeViewEx*			m_pHandleList;
	CHandleModel*			m_pHandleModel;
	QSortFilterProxyModel*	m_pSortProxy;

	QSplitter*				m_pSplitter;

	QWidget*				m_pDetailsWidget;
	QHBoxLayout*			m_pDetailsLayout;

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
};

