#pragma once
#include <qwidget.h>
#include "../../API/ProcessInfo.h"
#include "../../../MiscHelpers/Common/TreeItemModel.h"

class CProcessModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CProcessModel(QObject *parent = 0);
	~CProcessModel();

	void			SetUseDescr(int iDescr)	{ m_iUseDescr = iDescr; }

	QSet<quint64>	Sync(QMap<quint64, CProcessPtr> ProcessList);

	CProcessPtr		GetProcess(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	QString			GetColumHeader(int column) const;

	enum EColumns
	{
		eProcess = 0,
		ePID,
		//ePID_LXSS,
		eParentPID,
		eConsolePID,
		eSequenceNumber, 
		eStartKey, 
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
		eWorkingSet,
		ePeakWS,
		ePrivateWS,
		eVirtualSize,
		ePeakVirtualSize,
		ePageFaults,
		ePageFaultsDelta,
		eHardFaults,
		eHardFaultsDelta,
#ifdef WIN32
		eTLS,
		ePagedPool,
		ePeakPagedPool,
		eNonPagedPool,
		ePeakNonPagedPool,
#endif
		eSharedWS,
		eShareableWS,
#ifdef WIN32
		eShareableCommit,
#endif
		eMinimumWS,
		eMaximumWS,
		ePrivateBytesDelta,

		// GPU
		eGPU_Usage,
		eGPU_Shared,
		eGPU_Dedicated,
		eGPU_Adapter,

		// Priority
		ePriorityClass,
		eBasePriority,
#ifdef WIN32
		ePriorityBoost,
#endif
		ePagePriority,
		eIO_Priority,

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
		eMitigations,
		eImageCoherency,
		eProtection,
		eCritical,
#endif

		// IO
		eIO_TotalRate,
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

		ePowerThrottling, 
		eRunningTime,
		eSuspendedTime,
		eHangCount,
		eGhostCount,

		eErrorMode,
		eCodePage, 
		eReferences,
		eGrantedAccess,
#endif
		eDebugTotal,
		//eDebugDelte,
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
	
	int						m_iUseDescr;

	virtual QVariant GetDefaultIcon() const;
};