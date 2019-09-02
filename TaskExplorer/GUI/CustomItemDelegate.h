#pragma once

class CCustomItemDelegate : public QStyledItemDelegate
{
public:
	explicit CCustomItemDelegate(int MaxHeight, QObject * parent = 0) : QStyledItemDelegate(parent) 
	{
		m_MaxHeight = MaxHeight;
		m_GridColor = Qt::transparent;  

		m_Style = QStyleFactory::create("windows");
		m_Style->setParent(this);
	}
 
	void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const
    {
		//QStyledItemDelegate::paint(painter, option, index);

		QStyleOptionViewItem opt = option;
		initStyleOption(&opt, index);

		//value = index.data(Qt::ForegroundRole);
		//if (value.canConvert<QBrush>())
		//	option->palette.setBrush(QPalette::Text, qvariant_cast<QBrush>(value));

		//opt->backgroundBrush = qvariant_cast<QBrush>(index.data(Qt::BackgroundRole));

		//QStyle* style = QApplication::style();
		m_Style->drawControl(QStyle::CE_ItemViewItem, &opt, painter);

		//

		if (m_GridColor != Qt::transparent)
		{
			painter->save();
			painter->setPen(m_GridColor);
			//painter->drawRect(option.rect);
			//painter->drawLine(option.rect.left(), option.rect.top(), option.rect.right(), option.rect.top());
			painter->drawLine(option.rect.right(), option.rect.top(), option.rect.right(), option.rect.bottom());
			//painter->drawLine(option.rect.left() + (m_Tree && index.column() == 0 ? 24 : 0), option.rect.bottom(), option.rect.right(), option.rect.bottom());
			painter->drawLine(option.rect.left(), option.rect.bottom(), option.rect.right(), option.rect.bottom());
			painter->restore();
		}
    }

	void SetGridColor(const QColor& GridColor) { m_GridColor = GridColor; }
	
	QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &index) const
	{
		QSize size = QStyledItemDelegate::sizeHint(option, index);
		size.setHeight(m_MaxHeight);
		return size;
	}

protected:
	int m_MaxHeight;
	QColor m_GridColor;
	QStyle* m_Style;
};