#pragma once

class CCustomItemDelegate : public QStyledItemDelegate
{
public:
	explicit CCustomItemDelegate(int MaxHeight, QObject * parent = 0) : CCustomItemDelegate(MaxHeight, false, parent) { }
	explicit CCustomItemDelegate(int MaxHeight, QColor Color, QObject * parent = 0) : CCustomItemDelegate(MaxHeight, Color, false, parent) { }
	explicit CCustomItemDelegate(int MaxHeight, bool Tree, QObject * parent = 0) : CCustomItemDelegate(MaxHeight, QColor(Qt::darkGray), false, parent) { }
	explicit CCustomItemDelegate(int MaxHeight, QColor Color, bool Tree, QObject * parent = 0) : QStyledItemDelegate(parent) 
	{
		m_MaxHeight = MaxHeight;
		m_Color = Color;  
		m_Tree = Tree; 
		m_Grid = true;
	}
 
	void paint(QPainter * painter, const QStyleOptionViewItem & option, const QModelIndex & index ) const
    {
		//QStyledItemDelegate::paint(painter, option, index);

		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		//value = index.data(Qt::ForegroundRole);
		//if (value.canConvert<QBrush>())
		//	option->palette.setBrush(QPalette::Text, qvariant_cast<QBrush>(value));

		//opt->backgroundBrush = qvariant_cast<QBrush>(index.data(Qt::BackgroundRole));

		QStyle *style = QApplication::style();
		style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

		//

		if (m_Grid)
		{
			painter->save();
			painter->setPen(m_Color);
			//painter->drawRect(option.rect);
			//painter->drawLine(option.rect.left(), option.rect.top(), option.rect.right(), option.rect.top());
			painter->drawLine(option.rect.right(), option.rect.top(), option.rect.right(), option.rect.bottom());
			painter->drawLine(option.rect.left() + (m_Tree && index.column() == 0 ? 24 : 0), option.rect.bottom(), option.rect.right(), option.rect.bottom());
			painter->restore();
		}
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(m_MaxHeight);
		return size;
	}

	int m_MaxHeight;
	bool m_Grid;
	bool m_Tree;
	QColor m_Color;
};