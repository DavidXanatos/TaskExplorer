#pragma once
#include <qwidget.h>

class CPanelView : public QWidget
{
	Q_OBJECT
public:
	CPanelView(QWidget *parent = 0);
	virtual ~CPanelView();

protected slots:
	virtual void				OnMenu(const QPoint& Point);

	virtual void				OnCopyCell();
	virtual void				OnCopyRow();
	virtual void				OnCopyPanel();


	virtual QTreeView*			GetView() = 0;
	virtual QAbstractItemModel* GetModel() = 0;
	virtual QModelIndex			MapToSource(const QModelIndex& Model) { return Model; }
	static QModelIndexList		MapToSource(QModelIndexList Indexes, QSortFilterProxyModel* pProxy)	{ 
		for (int i = 0; i < Indexes.count(); i++)
			Indexes[i] = pProxy->mapToSource(Indexes[i]);
		return Indexes;
	}

	virtual void				AddPanelItemsToMenu(bool bAddSeparator = true);

	virtual void				ForceColumn(int column, bool bSet = true) { if (bSet) m_ForcedColumns.insert(column); else m_ForcedColumns.remove(column); }

	virtual void				RecursiveCopyPanel(const QModelIndex& ModelIndex, QStringList& Rows, int Level = 0);

protected:
	QMenu*						m_pMenu;

	QAction*					m_pCopyCell;
	QAction*					m_pCopyRow;
	QAction*					m_pCopyPanel;

	bool						m_CopyAll;
	QSet<int>					m_ForcedColumns;
};

template <class T>
class CPanelWidget : public CPanelView
{
public:
	CPanelWidget(QWidget *parent = 0) : CPanelView(parent)
	{
		m_pMainLayout = new QVBoxLayout();
		m_pMainLayout->setMargin(0);
		this->setLayout(m_pMainLayout);

		m_pTreeList = new T();
		m_pTreeList->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(m_pTreeList, SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenu(const QPoint &)));
		m_pMainLayout->addWidget(m_pTreeList);
		
		AddPanelItemsToMenu(false);
	}

	virtual QTreeView*			GetView()	{ return m_pTreeList; }
	virtual QAbstractItemModel* GetModel()	{ return m_pTreeList->model(); }

protected:
	QVBoxLayout*			m_pMainLayout;

	T*						m_pTreeList;
};