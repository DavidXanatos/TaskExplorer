#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/ListItemModel.h"
#include "../../API/Windows/WinPoolEntry.h"

class CPoolModel : public CListItemModel
{
    Q_OBJECT

public:
    CPoolModel(QObject *parent = 0);
	~CPoolModel();

	void			Sync(QMap<quint64, CPoolEntryPtr> DriverList);
	
	CPoolEntryPtr	GetPoolEntry(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
    	eTagName = 0,
    	eDriver,
    	eDescription,
    	ePagedAllocs,
    	ePagedFrees,
    	ePagedCurrent,
    	ePagedBytes,
    	eNonPagedAllocs,
    	eNonPagedFrees,
    	eNonPagedCurrent,
    	eNonPagedBytes,
		eCount
	};

protected:
	struct SPoolEntryNode: SListNode
	{
		SPoolEntryNode(const QVariant& Id) : SListNode(Id) {}

		CPoolEntryPtr			pPoolEntry;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SPoolEntryNode(Id); }
};