#pragma once
#include <qwidget.h>
#include "..\..\API\SocketInfo.h"

class CSocketModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CSocketModel(QObject *parent = 0);
	~CSocketModel();

	void			SetProcessFilter(const CProcessPtr& pProcess) { m_ProcessFilter = true; m_CurProcess = pProcess; }

	void			Sync(QMultiMap<quint64, CSocketPtr> SocketList);
	QModelIndex		FindIndex(quint64 SubID);
	void			Clear();

	CSocketPtr		GetSocket(const QModelIndex &index) const;

	QVariant		Data(const QModelIndex &index, int role, int section) const;

	// derived functions
    QVariant		data(const QModelIndex &index, int role) const;
    Qt::ItemFlags	flags(const QModelIndex &index) const;
    QModelIndex		index(int row, int column, const QModelIndex &parent = QModelIndex()) const;
    QModelIndex		parent(const QModelIndex &index) const;
    int				rowCount(const QModelIndex &parent = QModelIndex()) const;
    int				columnCount(const QModelIndex &parent = QModelIndex()) const;
    QVariant		headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;

	enum EColumns
	{
		eProcess = 0,
		eLocalAddress,
		eLocalPort,
		eRemoteAddress,
		eRemotePort,
		eProtocol,
		eState,
#ifdef WIN32
		eOwner,
#endif
		eTimeStamp,
		eLocalHostname,
		eRemoteHostname,
		eReceives,
		eSends,
		eReceiveBytes,
		eSendBytes,
		eTotalBytes,
		eReceivesDetla,
		eSendsDelta,
		eReceiveBytesDelta,
		eSendBytesDelta,
		eTotalBytesDelta,
		eFirewallStatus,
		eReceiveRate,
		eSendRate,
		eTotalRate,
		eCount
	};

protected:
	bool					m_ProcessFilter;
	CProcessPtr				m_CurProcess;

	struct SSocketNode
	{
		quint64				UID;
		
		CSocketPtr			pSocket;

		QVariant			Icon;
		struct SValue
		{
			QVariant Raw;
			QVariant Formated;
		};
		QVector<SValue>		Values;
	};

	void Sync(QList<SSocketNode*>& New, QMap<quint64, SSocketNode*>& Old);

	QList<SSocketNode*>			m_List;
	QMap<quint64, SSocketNode*>	m_Map;
};