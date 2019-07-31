#pragma once
#include <qobject.h>

class CAssemblyList : public QSharedData
{
public:
	CAssemblyList();

	void Clear()							{ m_Assemblies.clear(); }

	void AddAssembly(quint64 ID, quint64 ParrentID, QString Structure, QString FileName, QString Flags, QString NativePath);

	int	GetCount() const					{ return m_Assemblies.count(); }

	struct SAssembly
	{
		quint64 ID;
		quint64 ParrentID;

		QString Structure;
		QString FileName;
		QString Flags;
		QString NativePath;
	};

	const SAssembly& GetAssembly(int index) const;

protected:

	quint64 m_ProcessId;
	quint64 m_ThreadId;

	QList<SAssembly>	m_Assemblies;
};

typedef QSharedDataPointer<CAssemblyList> CAssemblyListPtr;
