#pragma once
#include <qwidget.h>

#include "TreeViewEx.h"

class CSplitTreeView : public QWidget
{
	Q_OBJECT
public:
	CSplitTreeView(QAbstractItemModel* pModel, QWidget *parent = 0);
	virtual ~CSplitTreeView();

	QHeaderView *header() const { return m_pList->header(); }
	void setHeader(QHeaderView *header) { m_pList->setHeader(header); }

	void				SetTreeWidth(int Width);
	int					GetTreeWidth() const;

signals:
	void				TreeResized(int Width);


	void				clicked(const QModelIndex& Index);
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

	void				OnExpandTree(const QModelIndex& index);
	void				OnCollapseTree(const QModelIndex& index);

private:

	QHBoxLayout*			m_pMainLayout;

	bool					m_bTreeHidden;
	QSplitter*				m_pSplitter;
	QTreeView*				m_pTree;
	QTreeView*				m_pList;
	QAbstractItemModel*		m_pModel;
	COneColumnModel*		m_pOneModel;
};
