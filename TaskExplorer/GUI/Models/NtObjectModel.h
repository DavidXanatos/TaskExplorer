#pragma once
#include <qwidget.h>
#include "../../API/WndInfo.h"
#include "../../../MiscHelpers/Common/TreeItemModel.h"


class CNtObjectModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CNtObjectModel(QObject *parent = 0);
	~CNtObjectModel();

	virtual void	fetchMore(const QModelIndex &parent);
    virtual bool	canFetchMore(const QModelIndex &parent) const;
	virtual bool	hasChildren(const QModelIndex &parent = QModelIndex()) const;
	virtual int		columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eName = 0,
		eType,
		eCount
	};

public slots:
	void Refresh();

protected:
	struct SNtObjectNode: STreeNode
	{
		SNtObjectNode(const QVariant& Id) : STreeNode(Id), State(0) {}
		~SNtObjectNode() {}
		
		int				State;

		QString			ObjectPath;
	};

	virtual void FillNode(const struct SNtObjectInfo* pNtObject, SNtObjectNode* pChildNode);
	virtual void Refresh(SNtObjectNode* pNode, QMap<QList<QVariant>, QList<STreeNode*> >& New, QHash<QVariant, STreeNode*>& Old);

	virtual SNtObjectNode*	GetNode(const QModelIndex &index) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SNtObjectNode(Id); }

	virtual QVariant		GetDefaultIcon() const { return m_DefaultIcon; }

	QMap<QString, QVariant> m_Icons;
	QVariant				m_DefaultIcon;
};