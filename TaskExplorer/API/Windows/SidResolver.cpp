#include "stdafx.h"
#include "SidResolver.h"
#include "ProcessHacker.h"

CSidResolver::CSidResolver(QObject* parent)
{
	m_bRunning = false;
}

CSidResolver::~CSidResolver()
{
	m_bRunning = false;

	if (!wait(10 * 1000))
		terminate();

	// cleanup unfinished tasks
	foreach(CSidResolverJob* pJob, m_JobQueue)
		pJob->deleteLater();
	m_JobQueue.clear();
}

bool CSidResolver::Init()
{
	return true;
}

QString CSidResolver::GetSidFullName(const QByteArray& Sid, QObject *receiver, const char *member)
{
    if (receiver && !QAbstractEventDispatcher::instance(QThread::currentThread())) {
        qWarning("CSidResolver::GetSidFullName() called with no event dispatcher");
        return "";
    }

	QReadLocker ReadLocker(&m_Mutex);
	QMap<QByteArray, QString>::iterator I = m_Sid2NameCache.find(Sid);
	if (I != m_Sid2NameCache.end())
		return I.value();
	ReadLocker.unlock();

	QMutexLocker Locker(&m_JobMutex);
	if (!m_bRunning)
	{
		m_bRunning = true;
		start();
	}

	CSidResolverJob* &pJob = m_JobQueue[Sid];
	if (!pJob)
	{
		pJob = new CSidResolverJob(Sid);
		pJob->moveToThread(this);
		QObject::connect(pJob, SIGNAL(SidResolved(const QByteArray&, const QString&)), this, SLOT(OnSidResolved(const QByteArray&, const QString&)), Qt::QueuedConnection);
	}
	if (receiver)
	{
		QObject::connect(pJob, SIGNAL(SidResolved(const QByteArray&, const QString&)), receiver, member, Qt::QueuedConnection);
		return tr("Resolving...");
	}
	return tr("Not resolved...");
}

void CSidResolver::OnSidResolved(const QByteArray& Sid, const QString& FullName)
{
	QWriteLocker WriteLocker(&m_Mutex);
	m_Sid2NameCache.insert(Sid, FullName);
}

void CSidResolver::run()
{
	//SetThreadDescription(GetCurrentThread(), L"SID Resolver");

	int IdleCount = 0;
	while (m_bRunning)
	{
		QMutexLocker Locker(&m_JobMutex);
		if (m_JobQueue.isEmpty()) 
		{
			if (IdleCount++ > 4 * 10) // if we were idle for 10 seconds end the thread
			{
				m_bRunning = false;
				break;
			}
			Locker.unlock();
			QThread::msleep(250);
			continue;
		}
		IdleCount = 0;
		CSidResolverJob* pJob = m_JobQueue.begin().value();
		Locker.unlock();

		PPH_STRING fullName = PhGetSidFullName((PSID)pJob->m_SID.data(), TRUE, NULL);
		QString FullName = CastPhString(fullName);

		Locker.relock();
		emit pJob->SidResolved(pJob->m_SID, FullName);
		m_JobQueue.take(pJob->m_SID)->deleteLater();
		Locker.unlock();
	}
}
