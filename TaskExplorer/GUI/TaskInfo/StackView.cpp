#include "stdafx.h"
#include "../TaskExplorer.h"
#include "StackView.h"
#include "../../Common/Common.h"


CStackView::CStackView(QWidget *parent)
	:CPanelView(parent)
{
	m_pMainLayout = new QVBoxLayout();
	m_pMainLayout->setMargin(0);
	this->setLayout(m_pMainLayout);

	// Stack List
	m_pStackList = new QTreeWidgetEx();
	m_pStackList->setItemDelegate(theGUI->GetItemDelegate());
	m_pStackList->setHeaderLabels(tr("#|Symbol|Stack address|Frame address|Control address|Return address|Stack parameters|File info").split("|"));

	m_pStackList->setSelectionMode(QAbstractItemView::ExtendedSelection);
	m_pStackList->setSortingEnabled(false);

	m_pStackList->setContextMenuPolicy(Qt::CustomContextMenu);
	connect(m_pStackList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));

	m_pMainLayout->addWidget(m_pStackList);
	// 

	m_bIsInvalid = false;

	//m_pMenu = new QMenu();
	AddPanelItemsToMenu(false);

	setObjectName(parent->parent()->objectName());
	m_pStackList->header()->restoreState(theConf->GetBlob(objectName() + "/StackView_Columns"));
}

CStackView::~CStackView()
{
	theConf->SetBlob(objectName() + "/StackView_Columns", m_pStackList->header()->saveState());
}

void CStackView::Invalidate()
{
	m_bIsInvalid = true;

	for (int i = 0; i < m_pStackList->topLevelItemCount(); i++)
	{
		for(int j=0; j < m_pStackList->columnCount(); j++)
			m_pStackList->topLevelItem(i)->setForeground(j, Qt::lightGray);
	}
}

void CStackView::ShowStack(const CStackTracePtr& StackTrace)
{
	int i = 0;
	for (; i < StackTrace->GetCount(); i++)
	{
		auto& StackFrame = StackTrace->GetFrame(i);
		
		QTreeWidgetItem* pItem;
		if (i >= m_pStackList->topLevelItemCount())
		{
			pItem = new QTreeWidgetItem();
			pItem->setData(eStack, Qt::UserRole, i);
			pItem->setText(eStack, QString::number(i));
			m_pStackList->addTopLevelItem(pItem);
		}
		else
			pItem = m_pStackList->topLevelItem(i);

		if (m_bIsInvalid)
		{
			for (int j = 0; j < m_pStackList->columnCount(); j++)
				m_pStackList->topLevelItem(i)->setForeground(j, Qt::black);
		}

		pItem->setText(eSymbol, StackFrame.Symbol);
		pItem->setText(eStackAddress, "0x" + QString::number(StackFrame.StackAddress, 16));
		pItem->setText(eFrameAddress, "0x" + QString::number(StackFrame.FrameAddress, 16));
		pItem->setText(eControlAddress, "0x" + QString::number(StackFrame.PcAddress, 16));
		pItem->setText(eReturnAddress, "0x" + QString::number(StackFrame.ReturnAddress, 16));
		pItem->setText(eStackParameter, tr("0x%1 0x%2 0x%3 0x%4").arg(StackFrame.Params[0], 0, 16).arg(StackFrame.Params[1], 0, 16).arg(StackFrame.Params[2], 0, 16).arg(StackFrame.Params[3], 0, 16));
		pItem->setText(eFileInfo, StackFrame.FileInfo);
	}

	for (; i < m_pStackList->topLevelItemCount(); )
		delete m_pStackList->topLevelItem(i);

	m_bIsInvalid = false;
}
