#pragma once
#include <qwidget.h>
#include "Common/ListItemModel.h"
#include "../../API/DNSEntry.h"

class CDnsModel : public CListItemModel
{
    Q_OBJECT

public:
    CDnsModel(QObject *parent = 0);
	~CDnsModel();

	//void			SetProcessFilter(const QList<QSharedPointer<QObject> >& Processes) { m_ProcessFilter = true; m_Processes = Processes; }

	void			Sync(QMultiMap<QString, CDnsCacheEntryPtr> List);
	
	CDnsCacheEntryPtr	GetDnsEntry(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		//eProcess = 0,
		eHostName,
		eType,
		eTTL,
		eTimeStamp,
		eCueryCount,
		eResolvedData,
		eCount
	};

protected:
	/*bool					m_ProcessFilter;
	QList<QSharedPointer<QObject> > m_Processes;*/

	struct SDnsNode: SListNode
	{
		SDnsNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CDnsCacheEntryPtr		pEntry;

		int					iColor;
	};

	virtual void SyncEntry(QList<SListNode*>& New, QHash<QVariant, SListNode*>& Old, const CDnsCacheEntryPtr& pEntry/*, const CDnsProcRecordPtr& pRecord = CDnsProcRecordPtr()*/);

	virtual SListNode* MkNode(const QVariant& Id) { return new SDnsNode(Id); }

	virtual QVariant GetDefaultIcon() const;
};
