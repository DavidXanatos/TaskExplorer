#pragma once
#include <qwidget.h>
#include "../../API/StringInfo.h"
#include "../../Common/ListItemModel.h"


class CStringModel : public CListItemModel
{
    Q_OBJECT;

public:
    CStringModel(QObject *parent = 0);
	~CStringModel();

	void			Sync(const QMap<quint64, CStringInfoPtr>& StringMap);

	CStringInfoPtr	GetString(const QModelIndex &index) const;

	int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eProcess = 0,
		eAddress,
		eBaseAddress,
		eLength,
		eResult,
		eCount
	};

protected:
	struct SStringNode: SListNode
	{
		SStringNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CStringInfoPtr			pString;

		int					iColor;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SStringNode(Id); }
	
	virtual QVariant GetDefaultIcon() const;
};