#include "stdafx.h"
#include "Settings.h"

CSettings::CSettings(const QString &fileName) : QSettings(fileName, QSettings::IniFormat)
{
	sync();
}

CSettings::~CSettings()
{
	sync();
}

QVariant CSettings::GetValue(const QString& Key, const QVariant& Default) const
{
	QMutexLocker Locker(&m_Mutex);
	return value(Key, Default);
}

void CSettings::SetValue(const QString& Key, const QVariant& Value)
{
	QMutexLocker Locker(&m_Mutex);
	return setValue(Key, Value);
}