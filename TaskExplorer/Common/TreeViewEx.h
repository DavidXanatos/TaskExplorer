#pragma once

#include <QStyledItemDelegate>

class QTreeViewEx: public QTreeView
{
	Q_OBJECT
public:
	QTreeViewEx(QWidget *parent = 0) : QTreeView(parent) 
	{
		header()->setContextMenuPolicy(Qt::CustomContextMenu);
		connect(header(), SIGNAL(customContextMenuRequested( const QPoint& )), this, SLOT(OnMenuRequested(const QPoint &)));

		m_pMenu = new QMenu(this);
	}

	/*void setColumnHiddenEx(int column, bool hide) {
		
		setColumnHidden(column, hide);
		if (hide)
			m_FixedColumns.insert(column);
		else
			m_FixedColumns.remove(column);

		foreach(QAction* pAction, m_Columns.keys())
			pAction->deleteLater();
		m_Columns.clear();
	}*/

	void setColumnFixed(int column, bool fixed) {
		if (fixed)
			m_FixedColumns.insert(column);
		else
			m_FixedColumns.remove(column);
	}

	QModelIndexList selectedRows() const
	{
		int Column = 0;
		QAbstractItemModel* pModel = model();
		for (int i = 0; i < pModel->columnCount(); i++)
		{
			if (!isColumnHidden(i))
			{
				Column = i;
				break;
			}
		}

		QModelIndexList IndexList;
		foreach(const QModelIndex& Index, selectedIndexes())
		{
			if (Index.column() == Column)
				IndexList.append(Index);
		}
		return IndexList;
	}

	template<class T>
	void StartUpdatingWidgets(T& OldMap, T& Map)
	{
		for(typename T::iterator I = Map.begin(); I != Map.end();)
		{
			if(I.value().first == NULL)
				I = Map.erase(I);
			else
			{
				OldMap.insert(I.key(), I.value());
				I++;
			}
		}
	}

	template<class T>
	void EndUpdatingWidgets(T& OldMap, T& Map)
	{
		for(typename T::iterator I = OldMap.begin(); I != OldMap.end(); I++)
		{
			Map.remove(I.key());
			if(I.value().second.isValid())
				setIndexWidget(I.value().second, NULL);
		}
	}

private slots:
	void				OnMenuRequested(const QPoint &point)
	{
		QAbstractItemModel* pModel = model();

		if(m_Columns.isEmpty())
		{
			for(int i=0; i < pModel->columnCount(); i++)
			{
				QString Label = pModel->headerData(i, Qt::Horizontal, Qt::DisplayRole).toString();
				if(Label.isEmpty() || m_FixedColumns.contains(i))
					continue;
				QAction* pAction = new QAction(Label, m_pMenu);
				pAction->setCheckable(true);
				pAction->setChecked(!isColumnHidden(i));
				connect(pAction, SIGNAL(triggered()), this, SLOT(OnMenu()));
				m_pMenu->addAction(pAction);
				m_Columns[pAction] = i;
			}
		}

		for(QMap<QAction*, int>::iterator I = m_Columns.begin(); I != m_Columns.end(); I++)
			I.key()->setChecked(!isColumnHidden(I.value()));

		m_pMenu->popup(QCursor::pos());	
	}

	void				OnMenu()
	{
		QAction* pAction = (QAction*)sender();
		int Column = m_Columns.value(pAction, -1);
		setColumnHidden(Column, !pAction->isChecked());
	}

protected:
	QMenu*				m_pMenu;
	QMap<QAction*, int>	m_Columns;
	QSet<int>			m_FixedColumns;
};

class QStyledItemDelegateMaxH : public QStyledItemDelegate
{
    Q_OBJECT
public:
    QStyledItemDelegateMaxH(int MaxHeight, QObject *parent = 0)
        : QStyledItemDelegate(parent) {m_MaxHeight = MaxHeight;}
	
    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(m_MaxHeight);
		return size;
	}

	int m_MaxHeight;
};

class CStyledGridItemDelegate : public QStyledItemDelegateMaxH
{
public:
	explicit CStyledGridItemDelegate(int MaxHeight, QObject * parent = 0) : CStyledGridItemDelegate(MaxHeight, false, parent) { }
	explicit CStyledGridItemDelegate(int MaxHeight, QColor Color, QObject * parent = 0) : CStyledGridItemDelegate(MaxHeight, Color, false, parent) { }
	explicit CStyledGridItemDelegate(int MaxHeight, bool Tree, QObject * parent = 0) : CStyledGridItemDelegate(MaxHeight, QColor(Qt::darkGray), false, parent) { }
	explicit CStyledGridItemDelegate(int MaxHeight, QColor Color, bool Tree, QObject * parent = 0) : QStyledItemDelegateMaxH(MaxHeight, parent) { m_Color = Color;  m_Tree = Tree; }
 
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
    {
		QStyledItemDelegate::paint(painter, option, index);

        painter->save();
        painter->setPen(m_Color);
        //painter->drawRect(option.rect);
		//painter->drawLine(option.rect.left(), option.rect.top(), option.rect.right(), option.rect.top());
		painter->drawLine(option.rect.right(), option.rect.top(), option.rect.right(), option.rect.bottom());
		painter->drawLine(option.rect.left() + (m_Tree && index.column() == 0 ? 24 : 0), option.rect.bottom(), option.rect.right(), option.rect.bottom());
        painter->restore();
    }

	bool m_Tree;
	QColor m_Color;
};