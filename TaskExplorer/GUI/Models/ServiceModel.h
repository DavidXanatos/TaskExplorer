#pragma once
#include <qwidget.h>
#include "Common/ListItemModel.h"
#include "../../API/ServiceInfo.h"

class CServiceModel : public CListItemModel
{
    Q_OBJECT

public:
    CServiceModel(QObject *parent = 0);
	~CServiceModel();

	void			Sync(QMap<QString, CServicePtr> ServiceList);
	
	CServicePtr		GetService(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eService = 0,
#ifdef WIN32
		eDisplayName,
		eType,
#endif
		eStatus,
		eStartType,
		ePID,
		eFileName,
		eErrorControl,
		eGroupe,
		eBinaryPath,
		eCount
	};

protected:
	struct SServiceNode: SListNode
	{
		SServiceNode(const QVariant& Id) : SListNode(Id) {}

		CServicePtr			pService;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SServiceNode(Id); }
};