#pragma once
#include "../DnsEntry.h"

class CDnsResolverJob;


inline bool operator < (const QHostAddress &key1, const QHostAddress &key2)
{
	// Note: toIPv6Address also works for IPv4 addresses
	Q_IPV6ADDR ip1 = key1.toIPv6Address();
	Q_IPV6ADDR ip2 = key2.toIPv6Address();
    return memcmp(&ip1, &ip2, sizeof(Q_IPV6ADDR)) < 0;
}


class CDnsResolver: public QThread
{
	Q_OBJECT
public:
	CDnsResolver(QObject* parent = NULL);
	~CDnsResolver();

	virtual bool Init();

	virtual QString GetHostName(const QHostAddress& Address, QObject *receiver = NULL, const char *member = NULL);

	virtual QMultiMap<QString, CDnsEntryPtr> GetEntryList() const { QReadLocker Locker(&m_Mutex);  return m_DnsCache; }

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

	virtual QString GetHostNameImpl(const QHostAddress& Address);
	virtual QString GetHostNamesImpl(const QString& HostName, int Limit = 10);

	bool m_bRunning;

	mutable QMutex							m_JobMutex;
	QMap<QHostAddress, CDnsResolverJob*>	m_JobQueue;

	mutable QReadWriteLock					m_Mutex;
	QMultiMap<QString, CDnsEntryPtr>		m_DnsCache; // HostName -> Entry
	quint64									m_LastCacheTraverse;
	QMultiMap<QHostAddress, CDnsEntryPtr>	m_AddressCache;
	QMultiMap<QString, CDnsEntryPtr>		m_RedirectionCache;
	

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