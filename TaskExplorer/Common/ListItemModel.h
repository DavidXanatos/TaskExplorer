#pragma once
#include <qwidget.h>

class CListItemModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CListItemModel(QObject *parent = 0);
	virtual ~CListItemModel();

	void			Sync(QList<QVariantMap> List);
	QModelIndex		FindIndex(const QVariant& ID);
	void			Clear();

	QVariant		Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    virtual QVariant		data(const QModelIndex &index, int role) const;
    virtual Qt::ItemFlags	flags(const QModelIndex &index) const;
    virtual QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    virtual QModelIndex		parent(const QModelIndex &index) const;
    virtual int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    virtual int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    virtual QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

protected:
	struct SListNode
	{
		SListNode(const QVariant& Id)
		{
			ID = Id;

			IsBold = false;
		}
		virtual ~SListNode() 
		{
		}

		QVariant			ID;

		QVariant			Icon;
		bool				IsBold;
		struct SValue
		{
			QVariant Raw;
			QVariant Formated;
		};
		QVector<SValue>		Values;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SListNode(Id); }

	void Sync(QList<SListNode*>& New, QMap<QVariant, SListNode*>& Old);

	virtual QVariant GetDefaultIcon() const { return QVariant(); }

	QList<SListNode*>			m_List;
	QMap<QVariant, SListNode*>	m_Map;
};