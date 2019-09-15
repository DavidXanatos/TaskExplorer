#pragma once
#include <qwidget.h>
#include "../../API/ModuleInfo.h"
#include "../../Common/TreeItemModel.h"


class CModuleModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CModuleModel(QObject *parent = 0);
	~CModuleModel();

	QSet<quint64>	Sync(const QMap<quint64, CModulePtr>& ModuleList);

	CModulePtr		GetModule(const QModelIndex &index) const;

	int				columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eModule = 0,
		eModuleFile,
		eBaseAddress,
		eSize,
#ifdef WIN32
		eDescription,
		eCompanyName,
		eVersion,
#endif
		eFileName,
#ifdef WIN32
		eType,
		eLoadCount,
		eVerificationStatus,
		eVerifiedSigner,
		eASLR,
		eTimeStamp,
		eCFGuard,
		eLoadTime,
		eLoadReason,
#endif
		eFileModifiedTime,
		eFileSize,
#ifdef WIN32
		eEntryPoint,
		eService,
#endif
		eParentBaseAddress,
		eCount
	};

protected:
	struct SModuleNode: STreeNode
	{
		SModuleNode(const QVariant& Id) : STreeNode(Id) {}

		CModulePtr			pModule;
	};

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SModuleNode(Id); }

	QList<QVariant>			MakeModPath(const CModulePtr& pModule, const QMap<quint64, CModulePtr>& ModuleList);
	bool					TestModPath(const QList<QVariant>& Path, const CModulePtr& pModule, const QMap<quint64, CModulePtr>& ModuleList, int Index = 0);

	virtual QVariant		GetDefaultIcon() const;
};