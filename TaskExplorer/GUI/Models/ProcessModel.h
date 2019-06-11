#pragma once
#include <qwidget.h>
#include "..\..\API\ProcessInfo.h"
#include "..\..\Common\TreeItemModel.h"

class CProcessModel : public CTreeItemModel
{
    Q_OBJECT

public:
    CProcessModel(QObject *parent = 0);
	~CProcessModel();

	void			SetUseDescr(bool bDescr)	{ m_bUseDescr = bDescr; }

	void			Sync(QMap<quint64, CProcessPtr> ProcessList);

	CProcessPtr		GetProcess(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	bool			testColumn(int column);
	void			setColumn(int column, bool set);
	QString			getColumn(int column) const;
	int				maxColumn() const;

	enum EColumns
	{
		eProcess = 0,
		ePID,
		eCPU,
		eIO_TotalRate,
		ePrivateBytes,
		eUserName,
#ifdef WIN32
		eDescription,

		eCompanyName,
		eVersion,
#endif
		eFileName,
		eCommandLine,
		ePeakPrivateBytes,
		eWorkingSet,
		ePeakWS,
		ePrivateWS,
		eSharedWS,
		eShareableWS,
		eVirtualSize,
		ePeakVirtualSize,
		ePageFaults,
		eSessionID,
		ePriorityClass,
		eBasePriority,

		eThreads,
		eHandles,
#ifdef WIN32
		eGDI_Handles,
		eUSER_Handles,
		eIntegrity,
#endif
		eIO_Priority,
		ePagePriority,
		eStartTime,
		eTotalCPU_Time,
		eKernelCPU_Time,
		eUserCPU_Time,
#ifdef WIN32
		eVerificationStatus,
		eVerifiedSigner,
#endif
		eASLR,
		eRelativeStartTime,
		eBits,
		eElevation,
#ifdef WIN32
		eWindowTitle,
		eWindowStatus,
#endif
		eCycles,
		eCyclesDelta,
		eCPU_History,
		ePrivateBytesHistory,
		eIO_History,
		eDEP,
		eVirtualized,
		eContextSwitches,
		eContextSwitchesDelta,
		ePageFaultsDelta,

		// IO
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

#ifdef WIN32
		eOS_Context,
#endif
		ePagedPool,
		ePeakPagedPool,
		eNonPagedPool,
		ePeakNonPagedPool,
		eMinimumWS,
		eMaximumWS,
		ePrivateBytesDelta,
		eSubsystem, // WSL or Wine
#ifdef WIN32
		ePackageName,
		eAppID,
		eDPI_Awareness,
#endif
		eCF_Guard,
		eTimeStamp,
		eFileModifiedTime,
		eFileSize,
		eSubprocesses,
#ifdef WIN32
		eJobObjectID,
		eProtection,
		eDesktop,
		eCritical,
#endif


		// Network IO
		eReceives,
		eSends,
		eReceiveBytes,
		eSendBytes,
		//case eTotalBytes,
		eReceivesDetla,
		eSendsDelta,
		eReceiveBytesDelta,
		eSendBytesDelta,
		//case eTotalBytesDelta,
		eReceiveRate,
		eSendRate,
		//case eTotalRate,

		// Disk IO
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
		//case eTotalRate,

		eHardFaults,
		eHardFaultsDelta,
		ePeakThreads,
		eGPU,
		eGPU_DedicatedBytes,
		eGPU_SharedBytes,
		eCount
	};

protected:
	struct SProcessNode: STreeNode
	{
		SProcessNode(const QVariant& Id) : STreeNode(Id) {}

		CProcessPtr			pProcess;
	};

	virtual STreeNode* MkNode(const QVariant& Id) { return new SProcessNode(Id); }

	QList<QVariant>  MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList);
	
	bool								m_bUseDescr;

	QVector<int>						m_Columns;
	bool								m_WasReset;

	virtual QVariant GetDefaultIcon() const;
};