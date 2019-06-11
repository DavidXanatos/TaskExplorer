#include "stdafx.h"
#include "../TaskExplorer.h"
#include "ProcessModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinProcess.h"
#endif



CProcessModel::CProcessModel(QObject *parent)
:CTreeItemModel(parent)
{
	m_bUseIcons = true;
	m_bUseDescr = true;

	m_Columns.append(eProcess);
	m_WasReset = false;
}

CProcessModel::~CProcessModel()
{
}

QList<QVariant> CProcessModel::MakeProcPath(const CProcessPtr& pProcess, const QMap<quint64, CProcessPtr>& ProcessList)
{
	QList<QVariant> list;

	quint64 ParentPID = pProcess->GetParentID();
	CProcessPtr pParent = ProcessList.value(ParentPID);

	if (!pParent.isNull())
	{
#ifdef WIN32
		if (!qobject_cast<CWinProcess*>(pProcess.data())->ValidateParent(pParent))
			return list;
#endif

		list = MakeProcPath(pParent, ProcessList);
		list.append(ParentPID);
	}

	return list;
}

void CProcessModel::Sync(QMap<quint64, CProcessPtr> ProcessList)
{
	QMap<QList<QVariant>, QList<STreeNode*> > New;
	QMap<QVariant, STreeNode*> Old = m_Map;

	foreach (const CProcessPtr& pProcess, ProcessList)
	{
		QVariant ID = pProcess->GetID();

		QModelIndex Index;
		QList<QVariant> Path;
		if(m_bTree)
			Path = MakeProcPath(pProcess, ProcessList);
		
		SProcessNode* pNode = static_cast<SProcessNode*>(Old.value(ID));
		if(!pNode || pNode->Path != Path)
		{
			pNode = static_cast<SProcessNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->Path = Path;
			pNode->pProcess = pProcess;
			New[Path].append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Index = Find(m_Root, pNode);
		}

		//if(Index.isValid()) // this is to slow, be more precise
		//	emit dataChanged(createIndex(Index.row(), 0, pNode), createIndex(Index.row(), columnCount()-1, pNode));

		if (m_WasReset) {
			pNode->Values.clear();
			pNode->Values.resize(m_Columns.size());
		}

		int Col = 0;
		bool State = false;
		bool Changed = false;

		// Note: icons are loaded asynchroniusly
		if (!pNode->Icon.isValid())
		{
			QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
			if (!Icon.isNull()) {
				Changed = true; // set change for first column
				pNode->Icon = Icon;
			}
		}

#ifdef WIN32
		CWinProcess* pWinProc = qobject_cast<CWinProcess*>(pProcess.data());
#endif

		SProcStats Stats = pProcess->GetStats();

		for(int section = eProcess; section < columnCount(); section++)
		{
			QVariant Value;
			//switch(section)
			switch(m_Columns[section])
			{
				case eProcess:		
#ifdef WIN32
					if (m_bUseDescr)
					{
						QString Descr = pWinProc->GetModuleInfo()->GetFileInfo("Description");
						if (!Descr.isEmpty())
						{
							Value =  Descr + " (" + pProcess->GetName() + ")";
							break;
						}
					}
#endif
											Value = pProcess->GetName(); break;
				case ePID:					Value = (qint64)pProcess->GetID(); break;
				case eCPU:					Value = pProcess->GetCpuUsage(); break;
				case eIO_TotalRate:			Value = Stats.Io.ReadRate.Get() + Stats.Io.WriteRate.Get() + Stats.Io.OtherRate.Get(); break;
				case ePrivateBytes:			Value = pProcess->GetWorkingSetPrivateSize(); break;
				case eUserName:				Value = pProcess->GetUserName(); break;
#ifdef WIN32
				case eDescription:			Value = pWinProc->GetModuleInfo()->GetFileInfo("Description"); break;

				/*case eCompanyName:			Value = break;
				case eVersion:				Value = break;*/
#endif
				case eFileName:				Value = pProcess->GetFileName(); break;
				/*case eCommandLine:			Value = break;
				case ePeakPrivateBytes:		Value = break;
				case eWorkingSet:			Value = break;
				case ePeakWS:				Value = break;
				case ePrivateWS:			Value = break;
				case eSharedWS:				Value = break;
				case eShareableWS:			Value = break;
				case eVirtualSize:			Value = break;
				case ePeakVirtualSize:		Value = break;
				case ePageFaults:			Value = break;*/
				case eSessionID:			Value = pProcess->GetSessionID(); break;
				case ePriorityClass:		Value = (quint32)pProcess->GetPriorityClass(); break;
				//case eBasePriority:			Value = break;

				case eThreads:				Value = (quint32)pProcess->GetNumberOfThreads(); break;
				case eHandles:				Value = (quint32)pProcess->GetNumberOfHandles(); break;
#ifdef WIN32
				case eGDI_Handles:			Value = (quint32)pWinProc->GetGdiHandles(); break;
				case eUSER_Handles:			Value = (quint32)pWinProc->GetUserHandles(); break;
				case eIntegrity:			Value = pWinProc->IntegrityLevel(); break;
#endif
				/*case eIO_Priority:			Value = break;
				case ePagePriority:			Value = break;*/
				case eStartTime:			Value = pProcess->GetCreateTime(); break;
				/*case eTotalCPU_Time:		Value = break;
				case eKernelCPU_Time:		Value = break;
				case eUserCPU_Time:			Value = break;
#ifdef WIN32
				case eVerificationStatus:	Value = break;
				case eVerifiedSigner:		Value = break;
#endif
				case eASLR:					Value = break;
				case eRelativeStartTime:	Value = break;
				case eBits:					Value = break;
				case eElevation:			Value = break;
#ifdef WIN32
				case eWindowTitle:			Value = break;
				case eWindowStatus:			Value = break;
#endif
				case eCycles:				Value = break;
				case eCyclesDelta:			Value = break;
				case eCPU_History:			Value = break;
				case ePrivateBytesHistory:	Value = break;
				case eIO_History:			Value = break;
				case eDEP:					Value = break;
				case eVirtualized:			Value = break;
				case eContextSwitches:		Value = break;
				case eContextSwitchesDelta:	Value = break;
				case ePageFaultsDelta:		Value = break;
				*/
				// IO
				case eIO_Reads:				Value = Stats.Io.ReadCount; break;
				case eIO_Writes:			Value = Stats.Io.WriteCount; break;
				case eIO_Other:				Value = Stats.Io.OtherCount; break;
				case eIO_ReadBytes:			Value = Stats.Io.ReadRaw; break;
				case eIO_WriteBytes:		Value = Stats.Io.WriteRaw; break;
				case eIO_OtherBytes:		Value = Stats.Io.OtherRaw; break;
				//case eIO_TotalBytes:		Value = ; break;
				case eIO_ReadsDelta:		Value = Stats.Io.ReadDelta.Delta; break;
				case eIO_WritesDelta:		Value = Stats.Io.WriteDelta.Delta; break;
				case eIO_OtherDelta:		Value = Stats.Io.OtherDelta.Delta; break;
				//case eIO_TotalDelta:		Value = ; break;
				case eIO_ReadBytesDelta:	Value = Stats.Io.ReadRawDelta.Delta; break;
				case eIO_WriteBytesDelta:	Value = Stats.Io.WriteRawDelta.Delta; break;
				case eIO_OtherBytesDelta:	Value = Stats.Io.WriteRawDelta.Delta; break;
				//case eIO_TotalBytesDelta:	Value = ; break;
				case eIO_ReadRate:			Value = Stats.Io.ReadRate.Get(); break;
				case eIO_WriteRate:			Value = Stats.Io.WriteRate.Get(); break;
				case eIO_OtherRate:			Value = Stats.Io.OtherRate.Get(); break;
				//case eIO_TotalRate:		Value = ; break;

#ifdef WIN32
				case eOS_Context:			Value = (quint32)pWinProc->GetOsContextVersion(); break;
#endif
				/*case ePagedPool:			Value = break;
				case ePeakPagedPool:		Value = break;
				case eNonPagedPool:			Value = break;
				case ePeakNonPagedPool:		Value = break;
				case eMinimumWS:			Value = break;
				case eMaximumWS:			Value = break;
				case ePrivateBytesDelta:	Value = break;
				case eSubsystem:			Value = break; // WSL or Wine
#ifdef WIN32
				case ePackageName:			Value = break;
				case eAppID:				Value = break;
				case eDPI_Awareness:		Value = break;
#endif
				case eCF_Guard:				Value = break;
				case eTimeStamp:			Value = break;
				case eFileModifiedTime:		Value = break;
				case eFileSize:				Value = break;
				case eSubprocesses:			Value = break;
#ifdef WIN32
				case eJobObjectID:			Value = break;
				case eProtection:			Value = break;
				case eDesktop:				Value = break;
				case eCritical:				Value = break;
#endif
*/
				// Network IO
				case eReceives:				Value = Stats.Net.ReceiveCount; break; 
				case eSends:				Value = Stats.Net.SendCount; break; 
				case eReceiveBytes:			Value = Stats.Net.ReceiveRaw; break; 
				case eSendBytes:			Value = Stats.Net.SendRaw; break; 
				//case eTotalBytes:			Value = ; break; 
				case eReceivesDetla:		Value = Stats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:			Value = Stats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:	Value = Stats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:		Value = Stats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = ; break; 
				case eReceiveRate:			Value = Stats.Net.ReceiveRate.Get(); break; 
				case eSendRate:				Value = Stats.Net.SendRate.Get(); break; 
				//case eTotalRate:			Value = ; break; 

				// Disk IO
				case eReads:				Value = Stats.Disk.ReadCount; break;
				case eWrites:				Value = Stats.Disk.WriteCount; break;
				case eReadBytes:			Value = Stats.Disk.ReadRaw; break;
				case eWriteBytes:			Value = Stats.Disk.WriteRaw; break;
				//case eTotalBytes:			Value = ; break;
				case eReadsDelta:			Value = Stats.Disk.ReadDelta.Delta; break;
				case eWritesDelta:			Value = Stats.Disk.WriteDelta.Delta; break;
				case eReadBytesDelta:		Value = Stats.Disk.ReadRawDelta.Delta; break;
				case eWriteBytesDelta:		Value = Stats.Disk.WriteRawDelta.Delta; break;
				//case eTotalBytesDelta:	Value = ; break;
				case eReadRate:				Value = Stats.Disk.ReadRate.Get(); break;
				case eWriteRate:			Value = Stats.Disk.WriteRate.Get(); break;
				//case eTotalRate:			Value = ; break;

				/*case eHardFaults:			Value = break;
				case eHardFaultsDelta:		Value = break;
				case ePeakThreads:			Value = break;
				case eGPU:					Value = break;
				case eGPU_DedicatedBytes:	Value = break;
				case eGPU_SharedBytes:		Value = break;*/
			}

			SProcessNode::SValue& ColValue = pNode->Values[section];

			bool Changed = false;
			if (ColValue.Raw != Value)
			{
				Changed = true;
				ColValue.Raw = Value;

				//switch(section)
				switch(m_Columns[section])
				{
					case ePID:				if (Value.toLongLong() < 0) ColValue.Formated = ""; break;

					case eCPU:				ColValue.Formated = QString::number(pProcess->GetCpuUsage()*100, 10,2); break;

					case ePrivateBytes:		ColValue.Formated = FormatSize(pProcess->GetWorkingSetPrivateSize()); break;
					case eStartTime:		ColValue.Formated = pProcess->GetCreateTime().toString("dd.MM.yyyy hh:mm:ss"); break;
					case ePriorityClass:	ColValue.Formated = pProcess->GetPriorityString(); break;
#ifdef WIN32
					case eIntegrity:		ColValue.Formated = pWinProc->GetIntegrityString(); break;
#endif

#ifdef WIN32
					case eOS_Context:		ColValue.Formated = pWinProc->GetOsContextString(); break;
#endif

					case eIO_ReadBytes:
					case eIO_WriteBytes:
					case eIO_OtherBytes:
					case eIO_ReadBytesDelta:
					case eIO_WriteBytesDelta:
					case eIO_OtherBytesDelta:

					case eReceiveBytes:
					case eSendBytes:
					case eReceiveBytesDelta:
					case eSendBytesDelta:

					case eReadBytes:
					case eWriteBytes:
					case eReadBytesDelta:
					case eWriteBytesDelta:
												ColValue.Formated = FormatSize(Value.toULongLong()); break; 

					case eIO_TotalRate:
					case eIO_ReadRate:
					case eIO_WriteRate:
					case eIO_OtherRate:
					case eReceiveRate:
					case eSendRate:
					case eReadRate:
					case eWriteRate:
												ColValue.Formated = FormatSize(Value.toULongLong()) + "/s"; break; 
				}
			}

			if(State != Changed)
			{
				if(State && Index.isValid())
					emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), section-1, pNode));
				State = Changed;
				Col = section;
			}
			Changed = false;
		}
		if(State && Index.isValid())
			emit dataChanged(createIndex(Index.row(), Col, pNode), createIndex(Index.row(), columnCount()-1, pNode));

	}

	m_WasReset = false;

	CTreeItemModel::Sync(New, Old);
}

CProcessPtr CProcessModel::GetProcess(const QModelIndex &index) const
{
	if (!index.isValid())
        return CProcessPtr();

	SProcessNode* pNode = static_cast<SProcessNode*>(index.internalPointer());
	ASSERT(pNode);

	return pNode->pProcess;
}

int CProcessModel::columnCount(const QModelIndex &parent) const
{
	//return eCount;
	return m_Columns.size();
}

QVariant CProcessModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		if(m_Columns.size() > section)
			return getColumn(m_Columns[section]);
	}
    return QVariant();
}

bool CProcessModel::testColumn(int column)
{
	return m_Columns.contains(column);
}

void CProcessModel::setColumn(int column, bool set)
{
	emit beginResetModel();
	if (!set)
	{
		//emit beginRemoveColumns();
		m_Columns.removeAll(column);
		//emit endRemoveColumns();
	}
	else if (!m_Columns.contains(column)) 
	{
		//emit beginInsertColumns();
		m_Columns.append(column);
		qSort(m_Columns.begin(), m_Columns.end(), [](const int& a, const int& b) { return a < b; });
		//emit endInsertColumns();
	}
	m_WasReset = true;
	emit endResetModel();
}

int CProcessModel::maxColumn() const
{
	return eCount;
}

QString CProcessModel::getColumn(int section) const
{
	switch(section)
	{
		case eProcess:				return tr("Process");
		case ePID:					return tr("PID");
		case eCPU:					return tr("CPU");
		case eIO_TotalRate:			return tr("I/O total rate");
		case ePrivateBytes:			return tr("Private bytes");
		case eUserName:				return tr("User name");
#ifdef WIN32
		case eDescription:			return tr("Description");

		case eCompanyName:			return tr("Company name");
		case eVersion:				return tr("Version");
#endif
		case eFileName:				return tr("File name");
		case eCommandLine:			return tr("Command line");
		case ePeakPrivateBytes:		return tr("Peak private bytes");
		case eWorkingSet:			return tr("Working set");
		case ePeakWS:				return tr("Peak working set");
		case ePrivateWS:			return tr("Private WS");
		case eSharedWS:				return tr("Shared WS");
		case eShareableWS:			return tr("Shareable WS");
		case eVirtualSize:			return tr("Virtual size");
		case ePeakVirtualSize:		return tr("Peak virtual size");
		case ePageFaults:			return tr("Page faults");
		case eSessionID:			return tr("Session ID");
		case ePriorityClass:		return tr("Priority class");
		case eBasePriority:			return tr("Base priority");

		case eThreads:				return tr("Threads");
		case eHandles:				return tr("Handles");
#ifdef WIN32
		case eGDI_Handles:			return tr("GDI handles");
		case eUSER_Handles:			return tr("USER handles");
		case eIntegrity:			return tr("Integrity");
#endif
		case eIO_Priority:			return tr("I/O priority");
		case ePagePriority:			return tr("Page priority");
		case eStartTime:			return tr("Start time");
		case eTotalCPU_Time:		return tr("Total CPU time");
		case eKernelCPU_Time:		return tr("Kernel CPU time");
		case eUserCPU_Time:			return tr("User CPU time");
#ifdef WIN32
		case eVerificationStatus:	return tr("Verification status");
		case eVerifiedSigner:		return tr("Verified signer");
#endif
		case eASLR:					return tr("ASLR");
		case eRelativeStartTime:	return tr("Relative start time");
		case eBits:					return tr("Bits");
		case eElevation:			return tr("Elevation");
#ifdef WIN32
		case eWindowTitle:			return tr("Window title");
		case eWindowStatus:			return tr("Window status");
#endif
		case eCycles:				return tr("Cycles");
		case eCyclesDelta:			return tr("Cycles delta");
		case eCPU_History:			return tr("CPU history");
		case ePrivateBytesHistory:	return tr("Private bytes history");
		case eIO_History:			return tr("I/O history");
		case eDEP:					return tr("DEP");
		case eVirtualized:			return tr("Virtualized");
		case eContextSwitches:		return tr("Context switches");
		case eContextSwitchesDelta:	return tr("Context switches delta");
		case ePageFaultsDelta:		return tr("Page faults delta");

		// IO
		case eIO_Reads:				return tr("I/O reads");
		case eIO_Writes:			return tr("I/O writes");
		case eIO_Other:				return tr("I/O other");
		case eIO_ReadBytes:			return tr("I/O read bytes");
		case eIO_WriteBytes:		return tr("I/O write bytes");
		case eIO_OtherBytes:		return tr("I/O other bytes");
		//case eIO_TotalBytes:		return tr("I/O total bytes");
		case eIO_ReadsDelta:		return tr("I/O reads delta");
		case eIO_WritesDelta:		return tr("I/O writes delta");
		case eIO_OtherDelta:		return tr("I/O other delta");
		//case eIO_TotalDelta:		return tr("I/O total delta");
		case eIO_ReadBytesDelta:	return tr("I/O read bytes delta");
		case eIO_WriteBytesDelta:	return tr("I/O write bytes delta");
		case eIO_OtherBytesDelta:	return tr("I/O other bytes delta");
		//case eIO_TotalBytesDelta:	return tr("I/O total bytes delta");
		case eIO_ReadRate:			return tr("I/O read rate");
		case eIO_WriteRate:			return tr("I/O write rate");
		case eIO_OtherRate:			return tr("I/O other rate");
		//case eIO_TotalRate:		return tr("I/O total rate");

#ifdef WIN32
		case eOS_Context:			return tr("OS context");
#endif
		case ePagedPool:			return tr("Paged pool");
		case ePeakPagedPool:		return tr("Peak paged pool");
		case eNonPagedPool:			return tr("Non-paged pool");
		case ePeakNonPagedPool:		return tr("Peak non-paged pool");
		case eMinimumWS:			return tr("Minimum working set");
		case eMaximumWS:			return tr("Maximum working set");
		case ePrivateBytesDelta:	return tr("Private bytes delta");
		case eSubsystem:			return tr("Subsystem"); // WSL or Wine
#ifdef WIN32
		case ePackageName:			return tr("Package name");
		case eAppID:				return tr("App ID");
		case eDPI_Awareness:		return tr("DPI awareness");
#endif
		case eCF_Guard:				return tr("CF Guard");
		case eTimeStamp:			return tr("Time stamp");
		case eFileModifiedTime:		return tr("File modified time");
		case eFileSize:				return tr("File size");
		case eSubprocesses:			return tr("Subprocesses");
#ifdef WIN32
		case eJobObjectID:			return tr("Job Object ID");
		case eProtection:			return tr("Protection");
		case eDesktop:				return tr("Desktop");
		case eCritical:				return tr("Critical");
#endif


		// Network IO
		case eReceives:				return tr("Network receives");
		case eSends:				return tr("Network sends");
		case eReceiveBytes:			return tr("Network receive bytes");
		case eSendBytes:			return tr("Network send bytes");
		//case eTotalBytes:			return tr("Network Total bytes");
		case eReceivesDetla:		return tr("Network receives detla");
		case eSendsDelta:			return tr("Network sends delta");
		case eReceiveBytesDelta:	return tr("Network receive bytes delta");
		case eSendBytesDelta:		return tr("Network send bytes delta");
		//case eTotalBytesDelta:	return tr("Network total bytes delta");
		case eReceiveRate:			return tr("Network receive rate");
		case eSendRate:				return tr("Network send rate");
		//case eTotalRate:			return tr("Network total rate");

		// Disk IO
		case eReads:				return tr("Disk reads");
		case eWrites:				return tr("Disk writes");
		case eReadBytes:			return tr("Disk read bytes");
		case eWriteBytes:			return tr("Disk write bytes");
		//case eTotalBytes:			return tr("Disk total bytes");
		case eReadsDelta:			return tr("Disk reads delta");
		case eWritesDelta:			return tr("Disk writes delta");
		case eReadBytesDelta:		return tr("Disk read bytes delta");
		case eWriteBytesDelta:		return tr("Disk write bytes delta");
		//case eTotalBytesDelta:		return tr("Disk total bytes delta");
		case eReadRate:				return tr("Disk read rate");
		case eWriteRate:			return tr("Disk write rate");
		//case eTotalRate:			return tr("Disk total rate");

		case eHardFaults:			return tr("Hard faults");
		case eHardFaultsDelta:		return tr("Hard faults delta");
		case ePeakThreads:			return tr("Peak threads");
		case eGPU:					return tr("GPU");
		case eGPU_DedicatedBytes:	return tr("GPU dedicated bytes");
		case eGPU_SharedBytes:		return tr("GPU shared bytes");
	}
	return "";
}

QVariant CProcessModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}