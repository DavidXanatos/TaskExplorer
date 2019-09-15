#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../Common/TreeItemModel.h"

class CProcessModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CProcessModel(QObject *parent = 0);
	~CProcessModel();

	void			SetUseDescr(bool bDescr)	{ m_bUseDescr = bDescr; }

	QSet<quint64>	Sync(QMap<quint64, CProcessPtr> ProcessList);

	CProcessPtr		GetProcess(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QString			GetColumHeader(int column) const;

	enum EColumns
	{
		eProcess = 0,
		ePID,
		eStaus,
		eUserName,
#ifdef WIN32
		eServices,
#endif
		eNetUsage,
		eUpTime,
		eStartTime,
		eElevation,
		eCommandLine,

		// Graphs
		eCPU_History,
		eMEM_History,
		eIO_History,
		eNET_History,
		eGPU_History,
		eVMEM_History,

		// CPU
		eCPU,
		ePriorityClass,
		eBasePriority,
		eTotalCPU_Time,
		eKernelCPU_Time,
		eUserCPU_Time,
		eContextSwitches,
		eContextSwitchesDelta,
		eCycles,
		eCyclesDelta,

		// Memory
		ePrivateBytes,
		ePeakPrivateBytes,
		ePagePriority,
		eWorkingSet,
		ePeakWS,
		ePrivateWS,
		eSharedWS,
		eShareableWS,
		eVirtualSize,
		ePeakVirtualSize,
		ePageFaults,
		ePageFaultsDelta,
		eHardFaults,
		eHardFaultsDelta,
		ePagedPool,
		ePeakPagedPool,
		eNonPagedPool,
		ePeakNonPagedPool,
		eMinimumWS,
		eMaximumWS,
		ePrivateBytesDelta,

		// GPU
		eGPU_Usage,
		eGPU_Shared,
		eGPU_Dedicated,
		eGPU_Adapter,

		// objects
		eHandles,
		ePeakHandles,
#ifdef WIN32
		eWND_Handles,
		eGDI_Handles,
		eUSER_Handles,
#endif
		eThreads,
		ePeakThreads,

		//Protection
#ifdef WIN32
		eIntegrity,
		eVerificationStatus,
		eVerifiedSigner,
		eASLR,
		eDEP,
		eCF_Guard,
		eProtection,
		eCritical,
#endif

		// IO
		eIO_TotalRate,
		eIO_Priority,
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


		// file Info
		eFileName,
#ifdef WIN32
		eDescription,
		eCompanyName,
		eVersion,
#endif
		eTimeStamp,
		eFileModifiedTime,
		eFileSize,


		// Other
		eSubsystem, // WSL or Wine
		eVirtualized,
		eArch,
#ifdef WIN32
		eOS_Context,

		eWindowTitle,
		eWindowStatus,

		ePackageName,
		eAppID,
		eDPI_Awareness,

		eJobObjectID,
		eDesktop,

		eRunningTime,
		eSuspendedTime,
		eHangCount,
		eGhostCount,
#endif
		eSessionID,


		// Network IO
		eNet_TotalRate,
		eReceives,
		eSends,
		eReceiveBytes,
		eSendBytes,
		//case eTotalBytes,
		eReceivesDelta,
		eSendsDelta,
		eReceiveBytesDelta,
		eSendBytesDelta,
		//case eTotalBytesDelta,
		eReceiveRate,
		eSendRate,

		// Disk IO
		eDisk_TotalRate,
		eReads,
		eWrites,
		eReadBytes,
		eWriteBytes,
		//case eTotalBytes,
		eReadsDelta,
		eWritesDelta,
		eReadBytesDelta,
		eWriteBytesDelta,
		//case eTotalBytesDelta,
		eReadRate,
		eWriteRate,

		eCount
	};

protected:
	struct SProcessNode: STreeNode
	{
		SProcessNode(const QVariant& Id) : STreeNode(Id), iColor(0) { }

		CProcessPtr			pProcess;

		int					iColor;

		QSet<int>			Bold;
	};

	virtual QVariant		NodeData(STreeNode* pNode, int role, int section) const;

	virtual STreeNode*		MkNode(const QVariant& Id) { return new SProcessNode(Id); }
		
	QList<QVariant>			MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList);
	bool					TestProcPath(const QList<QVariant>& Path, const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList, int Index = 0);
	
	bool					m_bUseDescr;

	virtual QVariant GetDefaultIcon() const;
};