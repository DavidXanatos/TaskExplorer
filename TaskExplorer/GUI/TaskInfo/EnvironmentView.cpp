#include "stdafx.h"
#include "../TaskExplorer/GUI/TaskExplorer.h"
#include "EnvironmentView.h"
#include "../Common/KeyValueInputDialog.h"


CEnvironmentView::CEnvironmentView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// Variables List
	m_pVariablesList = new QTreeWidgetEx();
	m_pVariablesList->setItemDelegate(theGUI->GetItemDelegate());
	m_pVariablesList->setHeaderLabels(tr("Name|Type|Value").split("|"));

	m_pVariablesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pVariablesList->setSortingEnabled(false);

	m_pVariablesList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pVariablesList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pVariablesList, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(OnItemDoubleClicked(QTreeWidgetItem*, int)));
	m_pMainLayout->addWidget(m_pVariablesList);
	// 

	//m_pMenu = new QMenu();
	m_pEdit = m_pMenu->addAction(tr("Edit"), this, SLOT(OnEdit()));
	m_pAdd = m_pMenu->addAction(tr("Add"), this, SLOT(OnAdd()));
	m_pDelete = m_pMenu->addAction(tr("Delete"), this, SLOT(OnDelete()));
	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	m_pVariablesList->header()->restoreState(theConf->GetBlob(objectName() + "/EnvironmentView_Columns"));
}

CEnvironmentView::~CEnvironmentView()
{
	theConf->SetBlob(objectName() + "/EnvironmentView_Columns", m_pVariablesList->header()->saveState());
}

void CEnvironmentView::ShowProcess(const CProcessPtr& pProcess)
{
	m_pCurProcess = pProcess;

	Refresh();
}

void CEnvironmentView::Refresh()
{
	if (!m_pCurProcess)
		return;

	QMap<QString, CProcessInfo::SEnvVar> EnvVars = m_pCurProcess->GetEnvVariables();

	QMap<QString, QTreeWidgetItem*> OldVars;
	for(int i = 0; i < m_pVariablesList->topLevelItemCount(); ++i) 
	{
		QTreeWidgetItem* pItem = m_pVariablesList->topLevelItem(i);
		QString TypeName = pItem->data(eName, Qt::UserRole).toString();
		OldVars.insert(TypeName ,pItem);
	}

	foreach(const CProcessInfo::SEnvVar EnvVar, EnvVars)
	{
		QTreeWidgetItem* pItem = OldVars.take(EnvVar.GetTypeName());
		if(!pItem)
		{
			pItem = new QTreeWidgetItem();
			pItem->setText(eName, EnvVar.Name);
			pItem->setData(eName, Qt::UserRole, EnvVar.GetTypeName());
			m_pVariablesList->addTopLevelItem(pItem);
		}

		pItem->setText(eType, EnvVar.GetType());
		pItem->setText(eValue, EnvVar.Value);	
	}

	foreach(QTreeWidgetItem* pItem, OldVars)
		delete pItem;
}

void CEnvironmentView::OnMenu(const QPoint &point)
{
	QTreeWidgetItem* pItem = m_pVariablesList->currentItem();
	QString Name = pItem ? pItem->data(eName, Qt::UserRole).toString() : 0;

	m_pEdit->setEnabled(!Name.isEmpty());
	m_pDelete->setEnabled(!Name.isEmpty());

	CPanelView::OnMenu(point);
}

void CEnvironmentView::OnEdit()
{
	OnItemDoubleClicked(m_pVariablesList->currentItem(), 0);
}

void CEnvironmentView::OnAdd()
{
	OnItemDoubleClicked(NULL, 0);
}

void CEnvironmentView::OnItemDoubleClicked(QTreeWidgetItem* pItem, int Column)
{
	QString Name = pItem ? pItem->text(eName) : 0;
	QString Value = pItem ? pItem->text(eValue) : 0;

	CKeyValueInputDialog KVDailog(this);
	KVDailog.setWindowTitle("TaskExplorer");
    KVDailog.setText(tr("Enter Environment Variable"));
	KVDailog.setKeyLabel(tr("Name:"));
	KVDailog.setKey(Name);
	KVDailog.setKeyReadOnly(!Name.isEmpty());
	KVDailog.setValueLabel(tr("Value:"));
	KVDailog.setValue(Value);
	KVDailog.setStandardButtons(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
	KVDailog.exec();
	if (KVDailog.clickedStandardButton() != QDialogButtonBox::Ok)
		return;

	m_pCurProcess->EditEnvVariable(KVDailog.key(), KVDailog.value());
}

void CEnvironmentView::OnDelete()
{
	QTreeWidgetItem* pItem = m_pVariablesList->currentItem();
	QString Name = pItem ? pItem->text(eName) : 0;

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the environment variable %1").arg(Name), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	m_pCurProcess->DeleteEnvVariable(Name);
}
