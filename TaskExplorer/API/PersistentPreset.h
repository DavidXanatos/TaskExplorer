#pragma once
#include <qobject.h>

class CPersistentPresetData: public QSharedData
{
public:
	CPersistentPresetData() : bTerminate(false),
		bPriority(false), iPriority(0),
		bAffinity(false), uAffinity(0),
		bIOPriority(false), iIOPriority(0),
		bPagePriority(false), iPagePriority(0)
	{
	}

	QString		sPattern;

	bool		bTerminate;

	bool		bPriority;
	long		iPriority;

	bool		bAffinity;
	quint64		uAffinity;

	bool		bIOPriority;
	long		iIOPriority;

	bool		bPagePriority;
	long		iPagePriority;
};

typedef QSharedDataPointer<CPersistentPresetData> CPersistentPresetDataPtr;


class CPersistentPreset: public QObject
{
	Q_OBJECT
public:
	CPersistentPreset(const QString Pattern = QString());
	virtual ~CPersistentPreset();

	void SetData(const CPersistentPresetDataPtr& pData)		{ QWriteLocker Locker(&m_Mutex); m_Data = pData; }
	CPersistentPresetDataPtr GetData() const				{ QReadLocker Locker(&m_Mutex); return m_Data; }

	QString GetPattern() const								{ QReadLocker Locker(&m_Mutex); return m_Data->sPattern; }

	void SetPriority(long Value)							{ QWriteLocker Locker(&m_Mutex); m_Data->bPriority = true; m_Data->iPriority = Value; }
	void SetAffinityMask(quint64 Value)						{ QWriteLocker Locker(&m_Mutex); m_Data->bAffinity = true; m_Data->uAffinity = Value; }
	void SetIOPriority(long Value)							{ QWriteLocker Locker(&m_Mutex); m_Data->bIOPriority = true; m_Data->iIOPriority = Value; }
	void SetPagePriority(long Value)						{ QWriteLocker Locker(&m_Mutex); m_Data->bPagePriority = true; m_Data->iPagePriority = Value; }

	bool Test(const QString& FileName, const QString& CommandLine = QString());

	QVariantMap Store() const;
	bool Load(const QVariantMap& Map);

protected:
	mutable QReadWriteLock		m_Mutex;
	CPersistentPresetDataPtr	m_Data;
};

typedef QSharedPointer<CPersistentPreset> CPersistentPresetPtr;
typedef QWeakPointer<CPersistentPreset> CPersistentPresetRef;