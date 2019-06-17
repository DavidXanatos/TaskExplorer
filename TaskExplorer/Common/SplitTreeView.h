#pragma once
#include <qwidget.h>

#include "TreeViewEx.h"

class CSplitTreeView : public QWidget
{
	Q_OBJECT
public:
	CSplitTreeView(QAbstractItemModel* pModel, QWidget *parent = 0);
	virtual ~CSplitTreeView();

	QTreeView*			GetView() { return m_pList; }

	void				SetTreeWidth(int Width);
	int					GetTreeWidth() const;

	QModelIndex			currentIndex() const { return m_pList->currentIndex(); }
	QModelIndexList		selectedRows() const;

	QSplitter*			GetSplitter() { return m_pSplitter; }

signals:
	void				TreeResized(int Width);

	void				MenuRequested(const QPoint &);

	void				clicked(const QModelIndex& Index);
	void				doubleClicked(const QModelIndex& Index);
	void				currentChanged(const QModelIndex &current, const QModelIndex &previous);
	// todo add more relevant signals

public slots:
	void				hideColumn(int column) { m_pList->hideColumn(column); }
    void				showColumn(int column) { m_pList->showColumn(column); }
	void				expand(const QModelIndex &index);
	void				collapse(const QModelIndex &index);
    void				resizeColumnToContents(int column) { m_pList->resizeColumnToContents(column); }
    void				sortByColumn(int column) { m_pList->sortByColumn(column); }
    void				expandAll() { m_pTree->expandAll(); }
    void				collapseAll() { m_pTree->collapseAll(); }
    void				expandToDepth(int depth) { m_pTree->expandToDepth(depth); }

private slots:
	void				OnSplitterMoved(int pos, int index);

	void				OnClickedTree(const QModelIndex& Index);
	void				OnDoubleClickedTree(const QModelIndex& Index);

	void				OnExpandTree(const QModelIndex& index);
	void				OnCollapseTree(const QModelIndex& index);

	void				OnTreeSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);
	void				OnListSelectionChanged(const QItemSelection& Selected, const QItemSelection& Deselected);

	void				OnTreeCurrentChanged(const QModelIndex &current, const QModelIndex &previous);
	void				OnListCurrentChanged(const QModelIndex &current, const QModelIndex &previous);

private:

	QHBoxLayout*			m_pMainLayout;

	bool					m_bTreeHidden;
	QSplitter*				m_pSplitter;
	QTreeView*				m_pTree;
	QTreeView*				m_pList;
	QAbstractItemModel*		m_pModel;
	COneColumnModel*		m_pOneModel;

	bool					m_LockSellection;
};
