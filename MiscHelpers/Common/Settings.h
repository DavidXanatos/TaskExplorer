#pragma once

#include <QSettings>
#include <QVariant>
#include <QMutex>
#include <QStringList>

#include "../corehelpers_global.h"

class COREHELPERS_EXPORT CSettings: public QObject
{
	Q_OBJECT

public:
	struct SStrRef
	{
		SStrRef(const char* pRef)
		 : Ref(pRef) {}

		bool operator < (const SStrRef& Other) const {return strcmp(Ref, Other.Ref) < 0;}
		bool operator == (const SStrRef& Other) const {return strcmp(Ref, Other.Ref) == 0;}

		operator QString() const {return QString(Ref);}

		const char* Ref;
	};

	// The members were named _Bool/_Int/_UInt/_UInt64. _Bool is a reserved
	// keyword (C's boolean type, which GCC also recognises in C++), so the
	// declaration parsed as two types. The v-prefix keeps them distinct from
	// anything the standard library claims.
	union SCacheVal
	{
        bool	vBool;
        qint32	vInt;
        quint32	vUInt;
        quint64	vUInt64;
	};

	struct SSetting
	{
		SSetting(){
			MinValue = 0;
			MaxValue = 0;
		}

		SSetting(const QVariant& value){
			Value = value;
			MinValue = 0; 
			MaxValue = 0;
		}

		SSetting(qint64 value, qint64 minvalue = LLONG_MIN, qint64 maxvalue = LLONG_MAX) {
			Value = value;
			MinValue = minvalue; 
			MaxValue = maxvalue;
		}

		SSetting(const QString& value, const QStringList& values) {
			Value = value;
			MinValue = 0; 
			MaxValue = 0;
			Values = values;
		}

		bool Check(const QVariant& value) const
		{
			if(MinValue != MaxValue)
			{
				if(!value.canConvert(QVariant::LongLong))
					return false;
				qint64 Test = value.toLongLong();
				if(Test < MinValue || MinValue > MaxValue)
					return false;
			}
			else if(!Values.isEmpty())
			{
				if(!value.canConvert(QVariant::String))
					return false;
				if(!Values.contains(value.toString()))
					return false;
			}
			return true;
		}

		virtual bool IsBlob() const {return false;}

		QVariant Value;
		qint64 MinValue;
		qint64 MaxValue;
		QStringList Values;
	};

	struct SBlobSetting: SSetting
	{
		SBlobSetting(const QByteArray& value){
			Value = value;
			MinValue = 0;
			MaxValue = 0;
		}
		virtual bool IsBlob() const {return true;}
	};

	CSettings(const QString& AppDir, const QString& AppName, const QString& GroupName = QString(), QMap<QString, SSetting> DefaultValues = QMap<QString, SSetting>(), QObject* qObject = NULL);

	//
	// Settings in a directory named outright, with no searching.
	//
	// For a program that has nowhere else to keep them: a system service has no
	// home directory - systemd sets no HOME at all - and the usual search then
	// lands on "/.local/share", a directory at the root of the filesystem that
	// nothing else will ever look in. A daemon knows where its own state
	// belongs; this lets it say so.
	//
	//
	// The tag is not decoration, and this overload must never take a bool again.
	//
	// It used to, and a bool is what a string literal binds to in preference to
	// a QString: "Xanasoft" is a const char*, and const char* -> bool is a
	// standard conversion while const char* -> QString is a user-defined one,
	// which loses. So every three-string call in this program - the viewer's,
	// the console's, the daemon's - silently resolved to *this* constructor with
	// bExplicit = true, and every one of them kept its settings in the directory
	// the program was started from instead of the user's own. It looked like an
	// elevation bug because writing next to the executable is what only an
	// administrator may do; unelevated the same call landed in the VirtualStore.
	//
	// A const char* does not convert to an enumeration, so the two constructors
	// can no longer be confused.
	//
	enum EExplicitDir { eExplicitDir };

	CSettings(const QString& ConfigDir, const QString& AppName, EExplicitDir, QObject* qObject = NULL);
	virtual ~CSettings();

	void				DelValue(const QString& key);
	bool				SetValue(const QString& key, const QVariant& value);
	QVariant			GetValue(const QString& key, const QVariant& preset = QVariant());

	void				SetBlob(const QString& key, const QByteArray& value);
	QByteArray			GetBlob(const QString& key);

#define IMPL_CFG_CACHE_GET(x,y,z) \
	x					Get##y(const SStrRef& key, x def = 0) \
	{ \
		QMutexLocker Locker(&m_Mutex); \
		QMap<SStrRef, SCacheVal>::Iterator I =  m_ValueCache.find(key); \
		if(I != m_ValueCache.end()) \
            return I.value().v##y; \
		Locker.unlock(); \
		x val = GetValue(key, def).to##z(); \
		Locker.relock(); \
		SCacheVal entry; \
        entry.v##y = val; \
		m_ValueCache.insert(key, entry); \
		return val; \
	}
	IMPL_CFG_CACHE_GET(bool, Bool, Bool);
	IMPL_CFG_CACHE_GET(qint32, Int, Int);
	IMPL_CFG_CACHE_GET(quint32, UInt, UInt);
	IMPL_CFG_CACHE_GET(quint64, UInt64, ULongLong);
#undef IMPL_CFG_CACHE_GET

	QString				GetString(const QString& key, const QVariant& preset = QVariant())		{return GetValue(key, preset).toString();}
	QStringList			GetStringList(const QString& key, const QVariant& preset = QVariant())	{return GetValue(key, preset).toStringList();}

	QStringList 		ListSettings()												{QMutexLocker Locker(&m_Mutex); return m_pConf->allKeys();}
	QStringList 		ListGroupes()												{QMutexLocker Locker(&m_Mutex); return m_pConf->childGroups();}
	QStringList 		ListKeys(const QString& Root);

	QString				GetConfigDir()												{QMutexLocker Locker(&m_Mutex); return m_ConfigDir;}
	bool				IsPortable()												{QMutexLocker Locker(&m_Mutex); return m_bPortable;}
	//
	// Whether the directory was named outright rather than searched for. Like
	// portable, it means "everything of ours lives here" - the difference is
	// only who decided, and nothing that follows the configuration around
	// should care which of the two it was.
	//
	bool				IsExplicitDir()												{QMutexLocker Locker(&m_Mutex); return m_bExplicitDir;}

	bool				IsWritable()												{QMutexLocker Locker(&m_Mutex); return m_pConf->isWritable();}

	void				Sync()														{QMutexLocker Locker(&m_Mutex); m_ValueCache.clear(); m_pConf->sync();}

protected:
	QMutex				m_Mutex;
	//QMap<QString, SSetting> m_DefaultValues;

	QMap<SStrRef, SCacheVal>m_ValueCache;

	QString				m_ConfigDir;
	bool				m_bPortable;
	bool				m_bExplicitDir = false;

	QSettings*			m_pConf;
};
