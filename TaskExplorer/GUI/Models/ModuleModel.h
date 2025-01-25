#pragma once
#include <qwidget.h>
#include "../../API/ModuleInfo.h"
#include "../../../MiscHelpers/Common/TreeItemModel.h"
#ifdef WIN32
#include "../../API/Windows/WinModule.h"
#endif

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
		eMitigations,
		eImageCoherency,
		eTimeStamp,
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
#ifdef WIN32
		eOriginalName,
		eArchitecture,
		//eEnclaveType,
		//eEnclaveBaseAddress,
		//eEnclaveSize,
#endif

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

#ifdef WIN32
	void					Sync(const CWinModule* pModule, QList<QVariant> Path, QSet<quint64> &Added, QMap<QList<QVariant>, QList<STreeNode*> > &New, QHash<QVariant, STreeNode*> &Old);
#endif



	virtual QVariant		GetDefaultIcon() const;
};