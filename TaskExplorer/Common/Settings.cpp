#include "stdafx.h"
#include "Settings.h"
#include "qzlib.h"
#include "Common.h"

/*QString CSettings::m_sAppDir;
QString CSettings::m_sConfigDir;
bool CSettings::m_bPortable = true;*/

bool TestWriteRight(const QString& Path)
{
	QFile TestFile(Path + "/~test-" + GetRand64Str() + ".tmp");
	if(!TestFile.open(QFile::WriteOnly))
		return false;
	TestFile.close();
	return TestFile.remove();
}

/*void CSettings::InitSettingsEnvironment(const QString& Orga, const QString& Name, const QString& Domain)
{
	QCoreApplication::setOrganizationName(Orga);
	QCoreApplication::setApplicationName(Name);
	QCoreApplication::setOrganizationDomain(Domain);

	m_sAppDir = QCoreApplication::applicationDirPath();

#ifndef __APPLE__
	m_sConfigDir = m_sAppDir + "/Config";
	if(!CreateDir(m_sConfigDir) || !TestWriteRight(m_sConfigDir)
#ifdef WIN32
		|| QFile::exists(m_sAppDir + "/NeoInstaller.exe")
#endif
	 )
#endif
		InitInstalled();

	QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, GetValuesDir());
}

void CSettings::InitInstalled()
{
	QStringList dirs = QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation);
	if(dirs.isEmpty())
		m_sConfigDir = QDir::homePath() + "/." + QCoreApplication::organizationName().toLower();
	else
		m_sConfigDir = dirs.first() + "/" + QCoreApplication::organizationName();

	CreateDir(m_sConfigDir);
	m_bPortable = false;
}*/

CSettings::CSettings(const QString& FileName, QMap<QString, SSetting> DefaultValues, QObject* qObject) : QSettings(FileName, QSettings::IniFormat, qObject)
{
	m_DefaultValues = DefaultValues;

	sync();

	foreach (const QString& Key, m_DefaultValues.uniqueKeys())
	{
		const SSetting& Setting = m_DefaultValues[Key];
		if(!contains(Key) || !Setting.Check(value(Key)))
		{
			if(Setting.IsBlob())
				setValue(Key, Setting.Value.toByteArray().toBase64().replace("+","-").replace("/","_").replace("=",""));
			else
				setValue(Key, Setting.Value);
		}
	}
}

CSettings::~CSettings()
{
	sync();
}

bool CSettings::SetValue(const QString &key, const QVariant &value)
{
	QMutexLocker Locker(&m_Mutex);

	if (!m_DefaultValues.isEmpty())
	{
		ASSERT(contains(key));
#ifndef _DEBUG
		if (!m_DefaultValues[key].Check(value))
			return false;
#endif
	}

	setValue(key, value);

	m_ValueCache.clear();
	return true;
}

QVariant CSettings::GetValue(const QString &key, const QVariant& default)
{
	QMutexLocker Locker(&m_Mutex);

	ASSERT(m_DefaultValues.isEmpty() || contains(key));	

	return value(key, default);
}

void CSettings::SetBlob(const QString& key, const QByteArray& value)
{
	QString str;
	QByteArray data = Pack(value);
	if(data.size() < value.size())
		str = ":PackedArray:" + data.toBase64().replace("+","-").replace("/","_").replace("=","");
	else
		str = ":ByteArray:" + value.toBase64().replace("+","-").replace("/","_").replace("=","");
	SetValue(key, str);
}

QByteArray CSettings::GetBlob(const QString& key)
{
	QByteArray value;
	QByteArray str = GetValue(key).toByteArray();
	if(str.left(11) == ":ByteArray:")
		value = QByteArray::fromBase64(str.mid(11).replace("-","+").replace("_","/"));
	else if(str.left(13) == ":PackedArray:")
		value = Unpack(QByteArray::fromBase64(str.mid(13).replace("-","+").replace("_","/")));
	return value;
}

const QStringList CSettings::ListKeys(const QString& Root)
{
	QMutexLocker Locker(&m_Mutex); 
	QStringList Keys;
	foreach(const QString& Key, allKeys())
	{
		QStringList Path = Key.split("/");
		ASSERT(Path.count() == 2);
		if(Path[0] == Root)
			Keys.append(Path[1]);
	}
	return Keys;
}
