#pragma once
#include <qwidget.h>
#include "..\..\API\HandleInfo.h"

class CHandleModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    CHandleModel(QObject *parent = 0);
	~CHandleModel();

	void			Sync(QMap<quint64, CHandlePtr> HandleList);
	QModelIndex		FindIndex(quint64 SubID);
	void			Clear();

	CHandlePtr		GetHandle(const QModelIndex &index) const;

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
		eHandle = 0,
		eType, // including sub type
		eName,
		ePosition,
		eSize,
		eGrantedAccess,
#ifdef WIN32
		eFileShareAccess,
		eAttributes,
		eObjectAddress,
		eOriginalName,
#endif
		eCount
	};

protected:
	struct SHandleNode
	{
		quint64				UID;
		
		CHandlePtr			pHandle;

		struct SValue
		{
			QVariant Raw;
			QVariant Formated;
		};
		QVector<SValue>		Values;
	};

	void Sync(QList<SHandleNode*>& New, QMap<quint64, SHandleNode*>& Old);

	QList<SHandleNode*>			m_List;
	QMap<quint64, SHandleNode*>	m_Map;
};