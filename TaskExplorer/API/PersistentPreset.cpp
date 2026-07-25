#include "stdafx.h"
#include "PersistentPreset.h"

CPersistentPreset::CPersistentPreset(const QString Pattern) 
{ 
	m_Data = new CPersistentPresetData(); 
	m_Data->sPattern = Pattern;
}

CPersistentPreset::~CPersistentPreset()
{
}

bool CPersistentPreset::Test(const QString& FileName, const QString& CommandLine)
{
	QReadLocker Locker(&m_Mutex);

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	QRegExp RegExp(m_Data->sPattern, Qt::CaseInsensitive, QRegExp::WildcardUnix);
	return RegExp.exactMatch(FileName) || RegExp.exactMatch(CommandLine);
#else
	QString ExpStr = QRegularExpression::escape(m_Data->sPattern);
	ExpStr = ".*" + ExpStr.replace("\\*",".*").replace("\\?",".") + ".*";
	QRegularExpression RegExp(ExpStr, QRegularExpression::CaseInsensitiveOption);

	return FileName.contains(RegExp) || CommandLine.contains(RegExp);
#endif	
}

QVariantMap CPersistentPreset::Store() const
{
	QReadLocker Locker(&m_Mutex);

	QVariantMap Map;
	Map["Pattern"] = m_Data->sPattern;
	if (m_Data->bTerminate)		Map["Terminate"] = true;
	if (m_Data->bPriority)		Map["CpuPriority"] = m_Data->iPriority;
	if (m_Data->bAffinity)		Map["CpuAffinity"] = m_Data->uAffinity;
	if (m_Data->bIOPriority)	Map["IoPriority"] = m_Data->iIOPriority;
	if (m_Data->bPagePriority)	Map["PagePriority"] = m_Data->iPagePriority;
	return Map;
}

bool CPersistentPreset::Load(const QVariantMap& Map)
{
	QWriteLocker Locker(&m_Mutex);

	m_Data->sPattern = Map["Pattern"].toString();
	m_Data->bTerminate = Map["Terminate"].toBool();
	if (m_Data->bPriority = Map.contains("CpuPriority"))		m_Data->iPriority = Map["CpuPriority"].toInt();
	if (m_Data->bAffinity = Map.contains("CpuAffinity"))		m_Data->uAffinity = Map["CpuAffinity"].toULongLong();
	if (m_Data->bIOPriority = Map.contains("IoPriority"))		m_Data->iIOPriority = Map["IoPriority"].toInt();
	if (m_Data->bPagePriority = Map.contains("PagePriority"))	m_Data->iPagePriority = Map["PagePriority"].toInt();

	return !m_Data->sPattern.isEmpty();
}
