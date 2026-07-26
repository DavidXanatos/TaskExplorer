#include "stdafx.h"
#include "LinuxModuleFinder.h"
#include "../LinuxModule.h"
#include "../ProcFs.h"
#include "../../SystemAPI.h"

CLinuxModuleFinder::CLinuxModuleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent)
	: CAbstractFinder(parent)
{
	m_Type = Type;
	m_RegExp = RegExp;
}

CLinuxModuleFinder::~CLinuxModuleFinder()
{
}

void CLinuxModuleFinder::run()
{
	const QList<quint64> Pids = ProcFs::EnumProcesses();

	QList<QSharedPointer<QObject>> Batch;
	int Index = 0;

	for (quint64 Pid : Pids)
	{
		if (IsCanceled())
			break;

		emit Progress((float)(++Index) / Pids.count());

		//
		// Only the first mapping of each file is reported. A shared object
		// contributes several segments, and without this the same library would
		// appear once per segment for every process.
		//
		QSet<QString> Seen;

		for (const ProcFs::SMapEntry& Entry : ProcFs::ReadMaps(Pid))
		{
			if (IsCanceled())
				break;

			if (Entry.Inode == 0 || Entry.Path.isEmpty() || Entry.Path.startsWith('['))
				continue;
			if (Seen.contains(Entry.Path))
				continue;
			if (!m_RegExp.match(Entry.Path).hasMatch())
				continue;

			Seen.insert(Entry.Path);

			QSharedPointer<CLinuxModule> pModule = QSharedPointer<CLinuxModule>(new CLinuxModule());
			pModule->InitStaticData(Entry.Path, Entry.Start, Entry.End - Entry.Start);
			pModule->SetLoaded(true);

			Batch.append(pModule);

			if (Batch.count() >= 64)
			{
				emit Results(Batch);
				Batch.clear();
			}
		}
	}

	if (!Batch.isEmpty())
		emit Results(Batch);

	emit Finished();
}
