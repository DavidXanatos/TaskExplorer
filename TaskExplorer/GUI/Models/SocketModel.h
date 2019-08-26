#pragma once
#include <qwidget.h>
#include "Common/ListItemModel.h"
#include "../../API/SocketInfo.h"

class CSocketModel : public CListItemModel
{
    Q_OBJECT

public:
    CSocketModel(QObject *parent = 0);
	~CSocketModel();

	void			SetProcessFilter(const QList<CProcessPtr>& Processes) { m_ProcessFilter = true; m_Processes = Processes; }

	void			Sync(QMultiMap<quint64, CSocketPtr> SocketList);
	
	CSocketPtr		GetSocket(const QModelIndex &index) const;

    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eProcess = 0,
		eProtocol,
		eState,
		eLocalAddress,
		eLocalPort,
		eRemoteAddress,
		eRemotePort,
#ifdef WIN32
		eOwnerService,
#endif
		eTimeStamp,
		//eLocalHostname,
		//eRemoteHostname,

		eReceives,
		eSends,
		eReceiveBytes,
		eSendBytes,
		//eTotalBytes,
		eReceivesDetla,
		eSendsDelta,
		eReceiveBytesDelta,
		eSendBytesDelta,
		//eTotalBytesDelta,
		eReceiveRate,
		eSendRate,
		//eTotalRate,
#ifdef WIN32
		eFirewallStatus,
#endif
		eCount
	};

protected:
	bool					m_ProcessFilter;
	QList<CProcessPtr>		m_Processes;

	struct SSocketNode: SListNode
	{
		SSocketNode(const QVariant& Id) : SListNode(Id), iColor(0) {}

		CSocketPtr			pSocket;

		int					iColor;
	};

	virtual SListNode* MkNode(const QVariant& Id) { return new SSocketNode(Id); }
	

	virtual QVariant GetDefaultIcon() const;
};