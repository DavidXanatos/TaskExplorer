#include "stdafx.h"
#include "../../MiscHelpers/Common/Settings.h"
#include "PersistenceConfig.h"
#include <QStyledItemDelegate>
#ifdef WIN32
#include "../API/Windows/ProcessHacker/PhSvc.h"
#endif
#include "AffinityDialog.h"

class QStyledItemDelegateEx : public QStyledItemDelegate
{
public:
	QStyledItemDelegateEx(QObject* parent = 0) : QStyledItemDelegate(parent) {}

	QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QStringList Options = index.data(Qt::UserRole).toStringList();
		if (index.column() == 1 && !Options.isEmpty()) 
		{
			QComboBox *cb = new QComboBox(parent);
			cb->addItems(Options);
			cb->setFrame(false);
			return cb;
		}
		return QStyledItemDelegate::createEditor(parent, option, index);
	}
};

CPersistenceConfig::CPersistenceConfig(QWidget *parent)
	: QDialog(parent)
{
	m_pMainLayout = new QGridLayout(this);

	m_pPresets = new QTableWidget();
	//connect(m_pPresets, SIGNAL(cellChanged(int, int)), this, SLOT(OnCmdLine(int, int)));
	//m_pPresets->horizontalHeader()->hide();
	m_pPresets->verticalHeader()->hide();
	m_pPresets->setItemDelegate(new QStyledItemDelegateEx(this));

	m_pPresets->setRowCount(1);
	m_pPresets->setColumnCount(7);

	m_pMainLayout->addWidget(m_pPresets, 0, 0, 1, 1);

	m_pButtonBox = new QDialogButtonBox();
	m_pButtonBox->setOrientation(Qt::Horizontal);
	m_pButtonBox->setStandardButtons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
	m_pMainLayout->addWidget(m_pButtonBox, 2, 0, 1, 1);
 
	connect(m_pButtonBox,SIGNAL(accepted()),this,SLOT(accept()));
	connect(m_pButtonBox,SIGNAL(rejected()),this,SLOT(reject()));


	restoreGeometry(theConf->GetBlob("PersistenceWindow/Window_Geometry"));

	LoadPersistentList();

	m_pPresets->horizontalHeader()->setSectionsClickable(false);
	m_pPresets->horizontalHeader()->setSectionResizeMode(eCmdLine, QHeaderView::Stretch);
	//m_pPresets->horizontalHeader()->setSectionResizeMode(ePermission, QHeaderView::Fixed);
	//m_pPresets->horizontalHeader()->setSectionResizeMode(eCPUPriority, QHeaderView::Fixed);
	//m_pPresets->horizontalHeader()->setSectionResizeMode(eCPUAffinity, QHeaderView::Fixed);
	//m_pPresets->horizontalHeader()->setSectionResizeMode(eIOPriority, QHeaderView::Fixed);
	//m_pPresets->horizontalHeader()->setSectionResizeMode(ePagePriority, QHeaderView::Fixed);
	m_pPresets->horizontalHeader()->resizeSection(eRemovePreset, 16+1);
	m_pPresets->horizontalHeader()->setSectionResizeMode(eRemovePreset, QHeaderView::Fixed);
}

CPersistenceConfig::~CPersistenceConfig()
{
	theConf->SetBlob("PersistenceWindow/Window_Geometry", saveGeometry());
}

void CPersistenceConfig::closeEvent(QCloseEvent *e)
{
	this->deleteLater();
}

void CPersistenceConfig::accept()
{
	StorePersistentList();

	this->close();
}

void CPersistenceConfig::reject()
{
	this->close();
}

void CPersistenceConfig::LoadPersistent(const CPersistentPresetDataPtr& pPreset, int RowCounter)
{
	m_pPresets->verticalHeader()->resizeSection(RowCounter, m_ItemHeight);

	int Column = 0;
	/*QTableWidgetItem* pCmdLine = new QTableWidgetItem(pPreset->sCommandLine);
	pCmdLine->setData(Qt::UserRole, pPreset->sCommandLine);
	m_pPresets->setItem(RowCounter, Column, pCmdLine);*/
	QLineEdit* pCmdLine = new QLineEdit(pPreset->sPattern);
	QObject::connect(pCmdLine, SIGNAL(textChanged(QString)), this, SLOT(OnCmdLine()));
	m_pPresets->setCellWidget(RowCounter, Column, pCmdLine);
	Column++;

	QComboBox* pPerm = new QComboBox();
	pPerm->addItems(tr("Permited|Terminate").split("|"));
	pPerm->setCurrentIndex(pPreset->bTerminate ? 1 : 0);
	connect(pPerm, SIGNAL(activated(int)), this, SLOT(OnPermission(int)));
	m_pPresets->setCellWidget(RowCounter, Column, pPerm);
	//m_pPresets->verticalHeader()->resizeSection(RowCounter, pPerm->height()+1);
	//m_pPresets->horizontalHeader()->resizeSection(Column, pPerm->width()+1);
	Column++;

	QComboBox* pCPUPrio = new QComboBox();
	pCPUPrio->addItem(tr("Unconfigured")	, (long)-1);
#ifdef WIN32
	pCPUPrio->addItem(tr("Real time")		, (long)PROCESS_PRIORITY_CLASS_REALTIME);
	pCPUPrio->addItem(tr("High")			, (long)PROCESS_PRIORITY_CLASS_HIGH);
	pCPUPrio->addItem(tr("Above normal")	, (long)PROCESS_PRIORITY_CLASS_ABOVE_NORMAL);
	pCPUPrio->addItem(tr("Normal")			, (long)PROCESS_PRIORITY_CLASS_NORMAL);
	pCPUPrio->addItem(tr("Below normal")	, (long)PROCESS_PRIORITY_CLASS_BELOW_NORMAL);
	pCPUPrio->addItem(tr("Idle")			, (long)PROCESS_PRIORITY_CLASS_IDLE);
#else
	// linux-todo:
#endif
	if(pPreset->bPriority) pCPUPrio->setCurrentIndex(pCPUPrio->findData(pPreset->iPriority));
	connect(pCPUPrio, SIGNAL(activated(int)), this, SLOT(OnCPUPriority(int)));
	m_pPresets->setCellWidget(RowCounter, Column, pCPUPrio);
	//m_pPresets->horizontalHeader()->resizeSection(Column, pCPUPrio->width()+1);
	Column++;

	QPushButton* pAffi = new QPushButton(pPreset->bAffinity ? tr("Custom") : tr("Unconfigured"));
	m_pPresets->setCellWidget(RowCounter, Column, pAffi);
	connect(pAffi, SIGNAL(clicked(bool)), this, SLOT(OnCPUAffinity()));
	Column++;

	QComboBox* pIOPrio = new QComboBox();
	pIOPrio->addItem(tr("Unconfigured")		, (long)-1);
#ifdef WIN32
	pIOPrio->addItem(tr("Critical")			, (long)IoPriorityCritical);
	pIOPrio->addItem(tr("High")				, (long)IoPriorityHigh);
	pIOPrio->addItem(tr("Normal")			, (long)IoPriorityNormal);
	pIOPrio->addItem(tr("Low")				, (long)IoPriorityLow);
	pIOPrio->addItem(tr("Very low")			, (long)IoPriorityVeryLow);
#else
	// linux-todo:
#endif
	if(pPreset->bIOPriority) pIOPrio->setCurrentIndex(pIOPrio->findData(pPreset->iIOPriority));
	connect(pIOPrio, SIGNAL(activated(int)), this, SLOT(OnIOPriority(int)));
	m_pPresets->setCellWidget(RowCounter, Column, pIOPrio);
	//m_pPresets->horizontalHeader()->resizeSection(Column, pIOPrio->width()+1);
	Column++;

	QComboBox* pPagePrio = new QComboBox();
	pPagePrio->addItem(tr("Unconfigured")	, (long)-1);
#ifdef WIN32
	pPagePrio->addItem(tr("Normal")			, (long)MEMORY_PRIORITY_NORMAL);
	pPagePrio->addItem(tr("Below normal")	, (long)MEMORY_PRIORITY_BELOW_NORMAL);
	pPagePrio->addItem(tr("Medium")			, (long)MEMORY_PRIORITY_MEDIUM);
	pPagePrio->addItem(tr("Low")			, (long)MEMORY_PRIORITY_LOW);
	pPagePrio->addItem(tr("Very low")		, (long)MEMORY_PRIORITY_VERY_LOW);
	pPagePrio->addItem(tr("Lowest")			, (long)MEMORY_PRIORITY_LOWEST);
#else
	// linux-todo:
#endif
	if(pPreset->bPagePriority) pPagePrio->setCurrentIndex(pPagePrio->findData(pPreset->iPagePriority));
	connect(pPagePrio, SIGNAL(activated(int)), this, SLOT(OnPagePriority(int)));
	m_pPresets->setCellWidget(RowCounter, Column, pPagePrio);
	//m_pPresets->horizontalHeader()->resizeSection(Column, pPagePrio->width()+1);
	Column++;

	QPushButton* pButton = new QPushButton(tr("X"));
	pButton->setFixedWidth(32);
	connect(pButton, SIGNAL(clicked(bool)), this, SLOT(OnRemovePreset()));
	m_pPresets->setCellWidget(RowCounter, Column, pButton);
}

void CPersistenceConfig::LoadPersistentList()
{
	m_pPresets->clear();

	m_pPresets->setHorizontalHeaderLabels(tr("Path/Command Line (with wildcards)|Execution|CPU Priority|CPU Affinity|I/O Priority|Page Priority|").split("|"));

	QPushButton* pButton = new QPushButton(tr("Add new entry"));
	connect(pButton, SIGNAL(clicked(bool)), this, SLOT(OnAddPreset()));
	pButton->setMaximumWidth(150);
	m_pPresets->setCellWidget(0, 0, pButton);
	m_ItemHeight = pButton->height() + 1;
	m_pPresets->verticalHeader()->resizeSection(0, m_ItemHeight);

	for (int i = 1; i < eColumnCount; i++) {
		QTableWidgetItem* pItem = new QTableWidgetItem();
		pItem->setFlags(pItem->flags() & ~Qt::ItemIsEditable);
		m_pPresets->setItem(0, i, pItem);
	}

	m_PersistentPreset = theAPI->GetPersistentPresets();
	int RowCounter = 0;
	foreach(const CPersistentPresetDataPtr& pPreset, m_PersistentPreset)
	{
		m_pPresets->insertRow(RowCounter);
		LoadPersistent(pPreset, RowCounter++);
	}
}

CPersistentPresetDataPtr* CPersistenceConfig::FindPreset(int Column)
{
	int index = -1;
	for(int i=0; i < m_pPresets->rowCount(); i++)
	{
		if (m_pPresets->cellWidget(i, Column) == sender()) {
			index = i;
			break;
		}
	}
	if (index == -1 || index >= m_PersistentPreset.count())
		return NULL;
	return &m_PersistentPreset[index];
}

void CPersistenceConfig::OnCmdLine()
{
	QLineEdit* pCmdLine = (QLineEdit*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(eCmdLine);
	if(ppPreset)
		(*ppPreset)->sPattern = pCmdLine->text();
}

void CPersistenceConfig::OnPermission(int index)
{
	QComboBox* pPerm = (QComboBox*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(ePermission);
	if(ppPreset)
		(*ppPreset)->bTerminate = pPerm->currentIndex() == 1;
}

void CPersistenceConfig::OnCPUPriority(int index)
{
	QComboBox* pCPUPrio = (QComboBox*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(eCPUPriority);
	if (ppPreset) {
		long value = pCPUPrio->currentData().toInt();
		(*ppPreset)->bPriority = value != -1;
		(*ppPreset)->iPriority = value;
	}
}

void CPersistenceConfig::OnCPUAffinity()
{
	QPushButton* pAffi = (QPushButton*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(eCPUAffinity);
	if (!ppPreset)
		return;

	int iCount = theAPI->GetCpuCount();

	QVector<int> Affinity(64, 0);
	for (int j = 0; j < iCount; j++)
	{
		int curBit = ((*ppPreset)->uAffinity >> j) & 1ULL;
		Affinity[j] = (*ppPreset)->bAffinity ? curBit : 1ULL;
	}
	
	CAffinityDialog AffinityDialog(iCount);
	AffinityDialog.SetName((*ppPreset)->sPattern);
	AffinityDialog.SetAffinity(Affinity);
	if (!AffinityDialog.exec())
		return;
	
	Affinity = AffinityDialog.GetAffinity();
	
	(*ppPreset)->uAffinity = 0;
	(*ppPreset)->bAffinity = false;
	for (int j = 0; j < iCount; j++)
	{
		if (Affinity[j] == 1)
			(*ppPreset)->uAffinity |= (1ULL << j);
		else
			// (*ppPreset)->uAffinity &= ~(1ULL << j);
			(*ppPreset)->bAffinity = true; // if not all are set it means its set
	}

	pAffi->setText((*ppPreset)->bAffinity ? tr("Custom") : tr("Unconfigured"));
}

void CPersistenceConfig::OnIOPriority(int index)
{
	QComboBox* pIOPrio = (QComboBox*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(eIOPriority);
	if (*ppPreset) {
		long value = pIOPrio->currentData().toInt();
		(*ppPreset)->bIOPriority = value != -1;
		(*ppPreset)->iIOPriority = value;
	}
}

void CPersistenceConfig::OnPagePriority(int index)
{
	QComboBox* pPagePrio = (QComboBox*)sender();
	CPersistentPresetDataPtr* ppPreset = FindPreset(ePagePriority);
	if (*ppPreset) {
		long value = pPagePrio->currentData().toInt();
		(*ppPreset)->bPagePriority = value != -1;
		(*ppPreset)->iPagePriority = value;
	}
}

void CPersistenceConfig::OnRemovePreset()
{
	for(int i=0; i < m_pPresets->rowCount(); i++)
	{
		if (m_pPresets->cellWidget(i, eRemovePreset) == sender()) {
			m_pPresets->removeRow(i);
			if(i <= m_PersistentPreset.count())
				m_PersistentPreset.removeAt(i);
		}
	}
}

void CPersistenceConfig::OnAddPreset()
{
	int RowCounter = m_pPresets->rowCount()-1;

	m_pPresets->insertRow(RowCounter);
	m_PersistentPreset.append(CPersistentPresetDataPtr(new CPersistentPresetData()));
	LoadPersistent(m_PersistentPreset.last(), RowCounter++);
}

void CPersistenceConfig::StorePersistentList()
{
	theAPI->SetPersistentPresets(m_PersistentPreset);
}