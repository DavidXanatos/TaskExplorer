#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/ListItemModel.h"
#include "../../API/HeapInfo.h"

class CHeapModel : public CListItemModel
{
    Q_OBJECT

public:
    CHeapModel(QObject *parent = 0);
	~CHeapModel();

	void			Sync(QMap<quint64, CHeapPtr> List);
	
	CHeapPtr		GetHeap(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eAddress = 0,
		eUsed,
		eCommited,
		eEntries,
		eFlags,
		eClass,
		eType,
		eCount
	};

protected:
	struct SHeapNode: SListNode
	{
		SHeapNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CHeapPtr			pHeap;

		int					iColor;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SHeapNode(Id); }
};