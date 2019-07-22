#pragma once
#include <qwidget.h>
#include "Common/ListItemModel.h"
#include "../../API/ThreadInfo.h"

class CThreadModel : public CListItemModel
{
    Q_OBJECT

public:
    CThreadModel(QObject *parent = 0);
	~CThreadModel();

	void			Sync(QMap<quint64, CThreadPtr> ThreadList);

	CThreadPtr		GetThread(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eThread = 0,
		eCPU,
		eCPU_History,
		eCyclesDelta,
#ifdef WIN32
		eStartAddress,
#endif
		ePriority,
#ifdef WIN32
		eService,
		eName,
		eType,
#endif
		eCreated,
#ifdef WIN32
		eStartModule,
#endif
		eContextSwitches,
		eBasePriority,
		ePagePriority,
		eIOPriority,
		eCycles,
		eState,
		eKernelTime,
		eUserTime,
#ifdef WIN32
		eIdealProcessor,
		eCritical,
#endif
		eCount
	};

protected:
	struct SThreadNode: SListNode
	{
		SThreadNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CThreadPtr			pThread;

		int					iColor;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SThreadNode(Id); }
};