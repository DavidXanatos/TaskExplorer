#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/PanelView.h"

class CPoolView;
class CDriversView;
class CNtObjectView;
class CAtomView;
class CRunObjView;

class CKernelView : public QWidget //CPanelView
{
	Q_OBJECT
public:
	CKernelView(QWidget *parent = 0);
	virtual ~CKernelView();

public slots:
	void					Refresh();

private slots:
	void					AddressFromSymbol(quint64 ProcessId, const QString& Symbol, quint64 Address);

protected:
	//virtual void				OnMenu(const QPoint& Point);
	//virtual QTreeView*			GetView()	{ return m_pStatsList; }
	//virtual QAbstractItemModel* GetModel()	{ return m_pStatsList->model(); }

private:

	QTabWidget*				m_pTabs;
	
	CPoolView*				m_pPoolView;
	CDriversView*			m_pDriversView;
	CNtObjectView*			m_pNtObjectView;
	CAtomView*				m_pAtomView;
	CRunObjView*			m_pRunObjView;

	QGridLayout*			m_pMainLayout;

	QWidget*				m_pInfoWidget;
	QHBoxLayout*			m_pInfoLayout;

	QWidget*				m_pInfoWidget1;
	QVBoxLayout*			m_pInfoLayout1;

	QWidget*				m_pInfoWidget2;
	QVBoxLayout*			m_pInfoLayout2;

	///////////////////

	QGroupBox*				m_pPPoolBox;
	QGridLayout*			m_pPPoolLayout;
	QLabel*					m_pPPoolWS;
	QLabel*					m_pPPoolVSize;
	QLabel*					m_pPPoolLimit;
	QLabel*					m_pPPoolAllocs;
	QLabel*					m_pPPoolFrees;

	QGroupBox*				m_pNPPoolBox;
	QGridLayout*			m_pNPPoolLayout;
	QLabel*					m_pNPPoolUsage;
	QLabel*					m_pNPPoolLimit;
	QLabel*					m_pNPPoolAllocs;
	QLabel*					m_pNPPoolFrees;


	quint64 m_MmSizeOfPagedPoolInBytes;
	quint64 m_MmMaximumNonPagedPoolInBytes;
};

