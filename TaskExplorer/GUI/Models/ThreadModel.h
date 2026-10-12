#pragma once
#include <qwidget.h>
#include "../MiscHelpers/Common/ListItemModel.h"
#include "../../API/ThreadInfo.h"

class CThreadModel : public CListItemModel
{
    Q_OBJECT

public:
    CThreadModel(QObject *parent = 0);
	~CThreadModel();

	void			Sync(QMap<quint64, CThreadPtr> ThreadList);

	void			SetExtThreadId(bool bSet) { m_bExtThreadId = bSet; }

	CThreadPtr		GetThread(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	//
	// Ordered in contiguous blocks so the header column menu can select each
	// group by a first..last range (see CThreadsView::OnHeaderMenu), the way the
	// process view does. Keep each block together; the menu names only the first
	// and last entry of a group. Reordering, or inserting anywhere but the end,
	// invalidates saved column layouts - bump ColumnLayoutVersion in ThreadsView.
	//
	enum EColumns
	{
		// common - shown at the top level of the menu
		eThread = 0,
		eCPU,
		eCPU_History,
		eStartAddress,
		eService,
		eName,
		eType,
		eState,
		eCreated,

		// CPU
		eContextSwitches,
		eContextSwitchesDelta,
		eCycles,
		eCyclesDelta,
		eKernelTime,
		eUserTime,

		// Scheduling
		ePriority,
		eBasePriority,
		eBasePriorityActual,
		ePagePriority,
		eIOPriority,
		eIdealProcessor,
		ePriorityBoost,

		// I/O
		eIO_Reads,
		eIO_Writes,
		eIO_Other,
		eIO_ReadBytes,
		eIO_WriteBytes,
		eIO_OtherBytes,
		//eIO_TotalBytes,
		eIO_ReadsDelta,
		eIO_WritesDelta,
		eIO_OtherDelta,
		//eIO_TotalDelta,
		eIO_ReadBytesDelta,
		eIO_WriteBytesDelta,
		eIO_OtherBytesDelta,
		//eIO_TotalBytesDelta,
		eIO_ReadRate,
		eIO_WriteRate,
		eIO_OtherRate,
		//eIO_TotalRate,

		// Other - misc, COM/.NET and platform, folded together
		eStartModule,
		eWaitTime,
		eStackUsage,
		ePendingIRP,
		eLastSystemCall,
		eLastStatusCode,
		eImpersonation,
		eCOM_Apartment,
		eCOM_Flags,
		eAppDomain,
		eFiber,
		eCritical,
		eTID_LXSS,
		ePowerThrottling,
		//eContainerID,
		eRPC_Usage,

		eCount
	};

protected:
	struct SThreadNode: SListNode
	{
		SThreadNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CThreadPtr			pThread;

		int					iColor;
	};

	bool m_bExtThreadId;

	virtual SListNode* MkNode(const QVariant& Id) { return new SThreadNode(Id); }

	virtual QVariant GetDefaultIcon() const;
};