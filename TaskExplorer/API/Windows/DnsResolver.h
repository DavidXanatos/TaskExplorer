#pragma once
#include "../DnsEntry.h"
#include "../../../MiscHelpers/Common/Common.h"

class CDnsResolverJob;

class CDnsResolver: public QThread
{
	Q_OBJECT
public:
	CDnsResolver(QObject* parent = NULL);
	~CDnsResolver();

	virtual bool Init();

	virtual QString GetHostName(const QHostAddress& Address, QObject *receiver = NULL, const char *member = NULL);

	virtual QMultiMap<QString, CDnsCacheEntryPtr> GetEntryList() const { QReadLocker Locker(&m_Mutex);  return m_DnsCache; }

	virtual void ClearPersistence();

signals:
	void DnsCacheUpdated();

public slots:
	virtual bool UpdateDnsCache();

	virtual void FlushDnsCache();

private slots:
	//virtual void OnHostResolved(const QHostAddress& Address, const QStringList& HostNames);

protected:
	friend class CDnsResolverJob;

	virtual void run();

	virtual QString GetHostNameSmart(const QHostAddress& Address, const QStringList& RevHostNames);
	virtual QString GetHostNamesSmart(const QString& HostName, int Limit = 10);

	bool m_bRunning;

	mutable QMutex							m_JobMutex;
	QMap<QHostAddress, CDnsResolverJob*>	m_JobQueue;

	mutable QReadWriteLock					m_Mutex;
	QMultiMap<QString, CDnsCacheEntryPtr>		m_DnsCache; // HostName -> Entry
	quint64									m_LastCacheTraverse;
	QMultiMap<QHostAddress, CDnsCacheEntryPtr>	m_AddressCache;
	QMultiMap<QString, CDnsCacheEntryPtr>		m_RedirectionCache;

	QMap<QHostAddress, quint64>					m_NoResultBlackList;

private:
	struct SDnsResolver* m;
};

class CDnsResolverJob : public QObject
{
	Q_OBJECT

protected:
	friend class CDnsResolver;

	CDnsResolverJob(const QHostAddress& Address, QObject *parent = nullptr) : QObject(parent) { m_Address = Address; }
	virtual ~CDnsResolverJob() {}

	QHostAddress m_Address;

signals:
	void HostResolved(const QHostAddress& Address, const QString& HostName);
};