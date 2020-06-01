#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/ListItemModel.h"
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

#ifdef WIN32
	void			SetShowKernelServices(bool bShow) { m_ShowDriver = bShow; }
#endif

	enum EColumns
	{
		eService = 0,
		eStatus,
		ePID,
#ifdef WIN32
		eDisplayName,
		eType,
		eStartType,
#endif
		eFileName,
#ifdef WIN32
		eErrorControl,
		eGroupe,
		eDescription,
		eCompanyName,
		eVersion,
#endif
		eBinaryPath,
		eCount
	};

protected:
	struct SServiceNode: SListNode
	{
		SServiceNode(const QVariant& Id) : SListNode(Id) {}

		int					iColor;
		CServicePtr			pService;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SServiceNode(Id); }

	virtual QVariant GetDefaultIcon() const;

#ifdef WIN32
	bool				m_ShowDriver;
#endif
};