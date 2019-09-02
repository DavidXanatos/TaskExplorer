#include "stdafx.h"
#include "../TaskExplorer.h"
#include "SocketModel.h"
#include "../../Common/Common.h"
#ifdef WIN32
#include "../../API/Windows/WinSocket.h"
#include "../../API/Windows/WindowsAPI.h"
#endif

CSocketModel::CSocketModel(QObject *parent)
:CListItemModel(parent)
{
	m_ProcessFilter = false;
}

CSocketModel::~CSocketModel()
{
}

void CSocketModel::Sync(QMultiMap<quint64, CSocketPtr> SocketList)
{
	QList<SListNode*> New;
	QHash<QVariant, SListNode*> Old = m_Map;
	
	bool IsMonitoringETW = ((CWindowsAPI*)theAPI)->IsMonitoringETW();
	bool bClearZeros = theConf->GetBool("Options/ClearZeros", true);

	foreach (const CSocketPtr& pSocket, SocketList)
	{
		if (m_ProcessFilter)
		{
			if (!m_Processes.contains(pSocket->GetProcess()))
				continue;
		}

		QVariant ID = (quint64)pSocket.data();

		int Row = -1;
		SSocketNode* pNode = static_cast<SSocketNode*>(Old[ID]);
		if(!pNode)
		{
			pNode = static_cast<SSocketNode*>(MkNode(ID));
			pNode->Values.resize(columnCount());
			pNode->pSocket = pSocket;
			New.append(pNode);
		}
		else
		{
			Old[ID] = NULL;
			Row = GetRow(pNode);
		}

#ifdef WIN32
		CWinSocket* pWinSock = qobject_cast<CWinSocket*>(pSocket.data());
#endif

		int Col = 0;
		bool State = false;
		int Changed = 0;

		// Note: icons are loaded asynchroniusly
		if (m_bUseIcons && !pNode->Icon.isValid() && m_Columns.contains(eProcess))
		{
			CProcessPtr pProcess = pNode->pSocket->GetProcess();
			if (!pProcess.isNull())
			{
				QPixmap Icon = pProcess->GetModuleInfo()->GetFileIcon();
				if (!Icon.isNull()) {
					Changed = 1; // set change for first column
					pNode->Icon = Icon;
				}
			}
		}

		int RowColor = CTaskExplorer::eNone;
		if (pSocket->IsMarkedForRemoval())		RowColor = CTaskExplorer::eToBeRemoved;
		else if (pSocket->IsNewlyCreated())		RowColor = CTaskExplorer::eAdded;
		
		if (pNode->iColor != RowColor) {
			pNode->iColor = RowColor;
			pNode->Color = CTaskExplorer::GetListColor(RowColor);
			Changed = 2;
		}


		SSockStats Stats = pSocket->GetStats();

		for(int section = eProcess; section < columnCount(); section++)
		{
			if (!m_Columns.contains(section))
				continue; // ignore columns which are hidden

			QVariant Value;
			switch(section)
			{
				case eProcess:			Value = pSocket->GetProcessName(); break;
				case eProtocol:			Value = (quint32)pSocket->GetProtocolType(); break; 
				case eState:			Value = (quint32)pSocket->GetState(); break; 
				case eLocalAddress:		Value = pSocket->GetLocalAddress().toString(); break;
				case eLocalPort:		Value = pSocket->GetLocalPort(); break; 
				case eRemoteAddress:	Value = pSocket->GetRemoteAddress().toString(); break; 
				case eRemotePort:		Value = pSocket->GetRemotePort(); break; 
#ifdef WIN32
				case eOwnerService:		Value = pWinSock->GetOwnerServiceName(); break; 
#endif
				case eTimeStamp:		Value = pSocket->GetCreateTimeStamp(); break;
				//case eLocalHostname:	Value = ; break; 
				//case eRemoteHostname:	Value = pSocket->GetRemoteHostName(); break; 

				case eReceives:			Value = Stats.Net.ReceiveCount; break; 
				case eSends:			Value = Stats.Net.SendCount; break; 
				case eReceiveBytes:		Value = Stats.Net.ReceiveRaw; break; 
				case eSendBytes:		Value = Stats.Net.SendRaw; break; 
				//case eTotalBytes:		Value = ; break; 
				case eReceivesDelta:	Value = Stats.Net.ReceiveDelta.Delta; break; 
				case eSendsDelta:		Value = Stats.Net.SendDelta.Delta; break; 
				case eReceiveBytesDelta:Value = Stats.Net.ReceiveRawDelta.Delta; break; 
				case eSendBytesDelta:	Value = Stats.Net.SendRawDelta.Delta; break; 
				//case eTotalBytesDelta:	Value = ; break; 
				case eReceiveRate:		Value = Stats.Net.ReceiveRate.Get(); break; 
				case eSendRate:			Value = Stats.Net.SendRate.Get(); break; 
				//case eTotalRate:		Value = ; break; 
#ifdef WIN32
				case eFirewallStatus:	Value = pWinSock->GetFirewallStatus(); break; 
#endif
			}

			SSocketNode::SValue& ColValue = pNode->Values[section];

#ifdef WIN32
			// Note: all rate and transfer values are not available without ETW being enabled
			if (!IsMonitoringETW && section >= eReceives && section < eFirewallStatus)
				Value = tr("N/A");
#endif

			if (ColValue.Raw != Value)
			{
				if(Changed == 0)
					Changed = 1;
				ColValue.Raw = Value;

				switch (section)
				{
					case eProcess:			{
												quint64 ProcessId = pSocket->GetProcessId();
												if (ProcessId)
													ColValue.Formated = tr("%1 (%2)").arg(pSocket->GetProcessName()).arg(ProcessId);
												break;
											}

					case eProtocol:				ColValue.Formated = pSocket->GetProtocolString(); break; 
					case eState:				ColValue.Formated = pSocket->GetStateString(); break; 			

					case eTimeStamp:			ColValue.Formated = QDateTime::fromTime_t(pSocket->GetCreateTimeStamp()/1000).toString("dd.MM.yyyy hh:mm:ss"); break;

					case eReceiveBytes:
					case eSendBytes:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSize(Value.toULongLong()); break; 
					case eReceiveBytesDelta:
					case eSendBytesDelta:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatSizeEx(Value.toULongLong(), bClearZeros); break; 

					case eReceiveRate:
					case eSendRate:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatRateEx(Value.toULongLong(), bClearZeros); break; 

					case eReceives:
					case eSends:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatNumber(Value.toULongLong()); break; 
					case eReceivesDelta:
					case eSendsDelta:
												if(Value.type() != QVariant::String) ColValue.Formated = FormatNumberEx(Value.toULongLong(), bClearZeros); break; 
#ifdef WIN32
					case eFirewallStatus:		ColValue.Formated = pWinSock->GetFirewallStatusString(); break; 
#endif
												
				}
			}

			if(State != (Changed != 0))
			{
				if(State && Row != -1)
					emit dataChanged(createIndex(Row, Col), createIndex(Row, section-1));
				State = (Changed != 0);
				Col = section;
			}
			if(Changed == 1)
				Changed = 0;
		}
		if(State && Row != -1)
			emit dataChanged(createIndex(Row, Col, pNode), createIndex(Row, columnCount()-1, pNode));

	}

	CListItemModel::Sync(New, Old);
}

CSocketPtr CSocketModel::GetSocket(const QModelIndex &index) const
{
	if (!index.isValid())
        return CSocketPtr();

	SSocketNode* pNode = static_cast<SSocketNode*>(index.internalPointer());
	return pNode->pSocket;
}

int CSocketModel::columnCount(const QModelIndex &parent) const
{
	return eCount;
}

QVariant CSocketModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole)
	{
		switch(section)
		{
			case eProcess:			return tr("Process");
			case eProtocol:			return tr("Protocol");
			case eState:			return tr("State");
			case eLocalAddress:		return tr("Local address");
			case eLocalPort:		return tr("Local port");
			case eRemoteAddress:	return tr("Remote address");
			case eRemotePort:		return tr("Remote port");
#ifdef WIN32
			case eOwnerService:		return tr("Owner");
#endif
			case eTimeStamp:		return tr("Time stamp");
			//case eLocalHostname:	return tr("Local hostname");
			//case eRemoteHostname:	return tr("Remote hostname");
			case eReceives:			return tr("Receives");
			case eSends:			return tr("Sends");
			case eReceiveBytes:		return tr("Receive bytes");
			case eSendBytes:		return tr("Send bytes");
			//case eTotalBytes:		return tr("Total bytes");
			case eReceivesDelta:	return tr("Receives delta");
			case eSendsDelta:		return tr("Sends delta");
			case eReceiveBytesDelta:return tr("Receive bytes delta");
			case eSendBytesDelta:	return tr("Send bytes delta");
			//case eTotalBytesDelta:	return tr("Total bytes delta");
#ifdef WIN32
			case eFirewallStatus:	return tr("Firewall status");
#endif
			case eReceiveRate:		return tr("Receive rate");
			case eSendRate:			return tr("Send rate");
			//case eTotalRate:		return tr("Total rate");
		}
	}
    return QVariant();
}


QVariant CSocketModel::GetDefaultIcon() const 
{ 
	return g_ExeIcon;
}