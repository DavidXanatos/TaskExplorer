#pragma once
#include <qwidget.h>
#include "../../API/MemoryInfo.h"
#include "../../../MiscHelpers/Common/TreeItemModel.h"


class CMemoryModel : public CTreeItemModel
{
    Q_OBJECT;

public:
    CMemoryModel(QObject *parent = 0);
	~CMemoryModel();

	void			Sync(const QMap<quint64, CMemoryPtr>& MemoryMap);
	bool			UpdateMemory(const CMemoryPtr& pMemory);

	CMemoryPtr		GetMemory(const QModelIndex &index) const;

	int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eBaseAddress = 0,
		eType,
		eSize,
		eProtection,
		eUse,
		eTotalWS,
		ePrivateWS,
		eShareableWS,
		eSharedWS,
		eLockedWS,
		eCommitted,
		ePrivate,
		eSigningLevel,
		eOriginalProtection,
		//eOriginalPages,
		eRegionType,
		ePriprity,
		eCount
	};

protected:
	struct SMemoryNode: STreeNode
	{
		SMemoryNode(const QVariant& Id) : STreeNode(Id), iColor(0) {}

		CMemoryPtr			pMemory;

		int					iColor;
	};

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SMemoryNode(Id); }

	QList<QVariant>			MakeMemPath(const CMemoryPtr& pMemory, const QMap<quint64, CMemoryPtr>& MemoryList);
	//bool					TestMemPath(const QList<QVariant>& Path, const CMemoryPtr& pMemory, const QMap<quint64, CMemoryPtr>& MemoryList, int Index = 0);

	void					UpdateMemory(const CMemoryPtr& pMemory, SMemoryNode* pNode, QModelIndex& Index);
};