#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/ListItemModel.h"
#include "../../API/Windows/RpcEndpoint.h"

class CRpcModel : public CListItemModel
{
    Q_OBJECT

public:
    CRpcModel(QObject *parent = 0);
	~CRpcModel();

	void			Sync(const QMap<QString, CRpcEndpointPtr>& RpcList);
	
	CRpcEndpointPtr	GetRpcEndpoint(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eIfId = 0,
		eDescription,
		eBinding,
		eCount
	};

protected:
	struct SRpcEndpointNode: SListNode
	{
		SRpcEndpointNode(const QVariant& Id) : SListNode(Id) {}

		CRpcEndpointPtr	pRpcEndpoint;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SRpcEndpointNode(Id); }
};