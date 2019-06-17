#include "stdafx.h"
#include "PanelView.h"

CPanelView::CPanelView(QWidget *parent)
	:QWidget(parent)
{
	m_CopyAll = true;

	m_pMenu = new QMenu();
}

CPanelView::~CPanelView()
{
}

void CPanelView::AddPanelItemsToMenu(bool bAddSeparator)
{
	if(bAddSeparator)
		m_pMenu->addSeparator();
	m_pCopyCell = m_pMenu->addAction(tr("Copy Cell"), this, SLOT(OnCopyCell()));
	m_pCopyRow = m_pMenu->addAction(tr("Copy Row"), this, SLOT(OnCopyRow()));
	m_pCopyPanel = m_pMenu->addAction(tr("Copy Panel"), this, SLOT(OnCopyPanel()));
}

void CPanelView::OnMenu(const QPoint& Point)
{
	QModelIndex Index = GetView()->currentIndex();
	
	m_pCopyCell->setEnabled(Index.isValid());
	m_pCopyRow->setEnabled(Index.isValid());

	m_pMenu->popup(QCursor::pos());	
}

void CPanelView::OnCopyCell()
{
	QModelIndex Index = GetView()->currentIndex();
	QModelIndex ModelIndex = MapToSource(Index);
	QApplication::clipboard()->setText(GetModel()->data(ModelIndex, Qt::DisplayRole).toString());
}

void CPanelView::OnCopyRow()
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	int Column = 0;
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (!pView->isColumnHidden(i))
		{
			Column = i;
			break;
		}
	}

	QModelIndexList IndexList;
	foreach(const QModelIndex& Index, pView->selectionModel()->selectedIndexes())
	{
		if (Index.column() == Column)
			IndexList.append(Index);
	}

	QStringList Rows;

	foreach(const QModelIndex& Index, IndexList)
	{
		QStringList Cels;
		QModelIndex ModelIndex = MapToSource(Index);
		for (int i = 0; i < pModel->columnCount(); i++) {
			if (!m_CopyAll && !pView->isColumnHidden(i) && !m_ForcedColumns.contains(i))
				continue;
			QModelIndex CurIndex = pModel->index(Index.row(), i, Index.parent());
			Cels.append(pModel->data(CurIndex, Qt::DisplayRole).toString());
		}
		Rows.append(Cels.join(tr(", ")));
	}
	
	QApplication::clipboard()->setText(Rows.join(tr("\n")));
}

void CPanelView::RecursiveCopyPanel(const QModelIndex& ModelIndex, QStringList& Rows, int Level)
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	QStringList Cels;
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (!m_CopyAll && !pView->isColumnHidden(i) && !m_ForcedColumns.contains(i))
			continue;
		QModelIndex CellIndex = pModel->index(ModelIndex.row(), i, ModelIndex.parent());
		Cels.append(pModel->data(CellIndex, Qt::DisplayRole).toString());
	}
	Rows.append((Level ? QString(Level, '_') + " " : "") + Cels.join(tr(", ")));

	for (int i = 0; i < pModel->rowCount(ModelIndex); i++)
	{
		QModelIndex SubIndex = pModel->index(i, 0, ModelIndex);
		RecursiveCopyPanel(SubIndex, Rows, Level + 1);
	}
}

void CPanelView::OnCopyPanel()
{
	QAbstractItemModel* pModel = GetModel();
	QTreeView * pView = GetView();

	QStringList Rows;

	QStringList Headder;
	for (int i = 0; i < pModel->columnCount(); i++)
	{
		if (!pView->isColumnHidden(i))
			Headder.append(pModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString());
	}
	Rows.append(Headder.join(tr(", ")));
	Rows.append("");

	for (int i = 0; i < pModel->rowCount(); ++i)
	{
		QModelIndex ModelIndex = pModel->index(i, 0);
		RecursiveCopyPanel(ModelIndex, Rows);
	}

	QApplication::clipboard()->setText(Rows.join(tr("\n")));
}
