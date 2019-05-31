#pragma once

class CSortFilterProxyModel: public QSortFilterProxyModel
{
public:
	CSortFilterProxyModel(bool bAlternate, QObject* parrent = 0) : QSortFilterProxyModel(parrent) {m_bAlternate = bAlternate;}

	bool filterAcceptsRow(int source_row, const QModelIndex & source_parent) const
	{
		// custom behaviour :
		if(!filterRegExp().isEmpty())
		{
			// get source-model index for current row
			QModelIndex source_index = sourceModel()->index(source_row, this->filterKeyColumn(), source_parent) ;
			if(source_index.isValid())
			{
				// if any of children matches the filter, then current index matches the filter as well
				int nb = sourceModel()->rowCount(source_index) ;
				for(int i = 0; i < nb; i++)
				{
					if(filterAcceptsRow(i, source_index))
						return true;
				}
				// check current index itself :
				QString key = sourceModel()->data(source_index, filterRole()).toString();
				return key.contains(filterRegExp()) ;
			}
		}
		// parent call for initial behaviour
		return QSortFilterProxyModel::filterAcceptsRow(source_row, source_parent) ;
	}

	QVariant data(const QModelIndex &index, int role) const
	{
		QVariant Data = QSortFilterProxyModel::data(index, role);
		if(m_bAlternate && role == Qt::BackgroundRole && !Data.isValid())
		{
			if (0 == index.row() % 2)
				return QColor(226, 237, 253);
			else
				return QColor(Qt::white);
		}
		return Data;
	}

protected:
	bool		m_bAlternate;
};