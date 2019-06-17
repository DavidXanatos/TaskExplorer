#pragma once
#include <qwidget.h>
#include "..\..\API\WndInfo.h"
#include "..\..\Common\TreeItemModel.h"


class CWindowModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CWindowModel(QObject *parent = 0);
	~CWindowModel();

	void			Sync(const QMap<quint64, CWndPtr>& ModuleList);

	CWndPtr			GetWindow(const QModelIndex &index) const;

	int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eHandle = 0,
#ifdef WIN32
		eClass,
#endif
		eText,
		eThread,
#ifdef WIN32
		eModule,
#endif
		eCount
	};

protected:
	struct SWindowNode: STreeNode
	{
		SWindowNode(const QVariant& Id) : STreeNode(Id) {}

		CWndPtr			pWindow;
	};

	virtual STreeNode* MkNode(const QVariant& Id) { return new SWindowNode(Id); }

	QList<QVariant>  MakeWndPath(const CWndPtr& pWindow, const QMap<quint64, CWndPtr>& WindowList);

	virtual QVariant GetDefaultIcon() const;
};