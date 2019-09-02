#pragma once

class CSidResolverJob;

class CSidResolver: public QThread
{
	Q_OBJECT
public:
	CSidResolver(QObject* parent = NULL);
	~CSidResolver();

	bool Init();

	QString GetSidFullName(const QByteArray& Sid, QObject *receiver = NULL, const char *member = NULL);

private slots:
	virtual void OnSidResolved(const QByteArray& Sid, const QString& FullName);

protected:
	virtual void run();

	bool m_bRunning;

	mutable QMutex				m_JobMutex;
	QMap<QByteArray, CSidResolverJob*>	m_JobQueue;

	mutable QReadWriteLock		m_Mutex;
	QMap<QByteArray, QString>	m_Sid2NameCache;
};

class CSidResolverJob : public QObject
{
	Q_OBJECT

protected:
	friend class CSidResolver;

	CSidResolverJob(const QByteArray& SID, QObject *parent = nullptr) : QObject(parent) { m_SID = SID; }
	virtual ~CSidResolverJob() {}

	QByteArray	m_SID;

signals:
	void SidResolved(const QByteArray& Sid, const QString& FullName);
};