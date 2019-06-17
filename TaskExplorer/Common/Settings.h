#pragma once
#include <qwidget.h>


class CSettings : protected QSettings
{
	Q_OBJECT
public:
	CSettings(const QString &fileName);
	virtual ~CSettings();

	QVariant GetValue(const QString& Key, const QVariant& Default = QVariant()) const;

	void SetValue(const QString& Key, const QVariant& Value);

protected:
	mutable QMutex m_Mutex;
};

