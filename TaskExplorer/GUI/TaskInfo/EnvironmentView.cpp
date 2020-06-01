#include "stdafx.h"
#include "../TaskExplorer.h"
#include "EnvironmentView.h"
#include "../../../MiscHelpers/Common/KeyValueInputDialog.h"
#include "../../../MiscHelpers/Common/Finder.h"


CEnvironmentView::CEnvironmentView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// Variables List
	m_pVariablesModel = new CSimpleListModel();
	m_pVariablesModel->setHeaderLabels(tr("Name|Type|Value").split("|"));

	m_pSortProxy = new CSortFilterProxyModel(false, this);
	m_pSortProxy->setSortRole(Qt::EditRole);
    m_pSortProxy->setSourceModel(m_pVariablesModel);
	m_pSortProxy->setDynamicSortFilter(true);

	m_pVariablesList = new QTreeViewEx();
	m_pVariablesList->setItemDelegate(theGUI->GetItemDelegate());
	
	m_pVariablesList->setModel(m_pSortProxy);

	m_pVariablesList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pVariablesList->setSortingEnabled(true);

	m_pVariablesList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pVariablesList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	connect(m_pVariablesList, SIGNAL(doubleClicked(const QModelIndex&)), this, SLOT(OnItemDoubleClicked(const QModelIndex&)));

	//m_pVariablesList->setColumnReset(2);
	//connect(m_pVariablesList, SIGNAL(ResetColumns()), this, SLOT(OnResetColumns()));
	connect(m_pVariablesList, SIGNAL(ColumnChanged(int, bool)), this, SLOT(OnColumnsChanged()));

	m_pMainLayout->addWidget(m_pVariablesList);
	// 

	m_pMainLayout->addWidget(new CFinder(m_pSortProxy, this));

	//m_pMenu = new QMenu();
	m_pEdit = m_pMenu->addAction(tr("Edit"), this, SLOT(OnEdit()));
	m_pAdd = m_pMenu->addAction(tr("Add"), this, SLOT(OnAdd()));
	m_pDelete = m_pMenu->addAction(tr("Delete"), this, SLOT(OnDelete()));
	
	AddPanelItemsToMenu();

	setObjectName(parent->objectName());
	QByteArray Columns = theConf->GetBlob(objectName() + "/EnvironmentView_Columns");
	if (Columns.isEmpty())
		m_pVariablesList->OnResetColumns();
	else
		m_pVariablesList->restoreState(Columns);
}

CEnvironmentView::~CEnvironmentView()
{
	theConf->SetBlob(objectName() + "/EnvironmentView_Columns", m_pVariablesList->saveState());
}

void CEnvironmentView::OnColumnsChanged()
{
}

void CEnvironmentView::ShowProcesses(const QList<CProcessPtr>& Processes)
{
	CProcessPtr pProcess;
	if (Processes.count() > 1)
	{
		setEnabled(false);
	}
	else if(!Processes.isEmpty())
	{
		setEnabled(true);
		pProcess = Processes.first();
	}

	m_pCurProcess = pProcess;

	Refresh();
}

void CEnvironmentView::Refresh()
{
	if (!m_pCurProcess)
		return;

	QMap<QString, CProcessInfo::SEnvVar> EnvVars = m_pCurProcess->GetEnvVariables();

	m_Variables.clear();

	foreach(const CProcessInfo::SEnvVar EnvVar, EnvVars)
	{
		QVariantMap Item;
		Item["ID"] = EnvVar.GetTypeName();
		
		QVariantMap Values;
		Values.insert(QString::number(eName), EnvVar.Name);
		Values.insert(QString::number(eType), EnvVar.GetType());
		Values.insert(QString::number(eValue), EnvVar.Value);

		Item["Values"] = Values;
		m_Variables.append(Item);
	}
	m_pVariablesModel->Sync(m_Variables);
}

void CEnvironmentView::OnMenu(const QPoint &point)
{
	QModelIndex Index = m_pVariablesList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	QString Name = ModelIndex.isValid() ? m_pVariablesModel->Data(ModelIndex, Qt::EditRole, eName).toString() : "";

	m_pEdit->setEnabled(!Name.isEmpty());
	m_pDelete->setEnabled(!Name.isEmpty());

	CPanelView::OnMenu(point);
}

void CEnvironmentView::OnEdit()
{
	QModelIndex Index = m_pVariablesList->currentIndex();
	OnItemDoubleClicked(Index);
}

void CEnvironmentView::OnAdd()
{
	OnItemDoubleClicked(QModelIndex());
}

void CEnvironmentView::OnItemDoubleClicked(const QModelIndex& Index)
{
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);

	QString Name = ModelIndex.isValid() ? m_pVariablesModel->Data(ModelIndex, Qt::EditRole, eName).toString() : "";
	QString Value = ModelIndex.isValid() ? m_pVariablesModel->Data(ModelIndex, Qt::EditRole, eValue).toString() : "";

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
	QModelIndex Index = m_pVariablesList->currentIndex();
	QModelIndex ModelIndex = m_pSortProxy->mapToSource(Index);
	if (!ModelIndex.isValid())
		return;
	QString Name = m_pVariablesModel->Data(ModelIndex, Qt::EditRole, eName).toString();

	if(QMessageBox("TaskExplorer", tr("Do you want to delete the environment variable %1").arg(Name), QMessageBox::Question, QMessageBox::Yes, QMessageBox::No | QMessageBox::Default | QMessageBox::Escape, QMessageBox::NoButton).exec() != QMessageBox::Yes)
		return;

	m_pCurProcess->DeleteEnvVariable(Name);
}
