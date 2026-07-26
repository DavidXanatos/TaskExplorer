#include "stdafx.h"
#include "LinuxHandleFinder.h"
#include "../LinuxHandle.h"
#include "../ProcFs.h"
#include "../../SystemAPI.h"

CLinuxHandleFinder::CLinuxHandleFinder(const QVariant& Type, const QRegularExpression& RegExp, QObject* parent)
	: CAbstractFinder(parent)
{
	m_Type = Type;
	m_RegExp = RegExp;
}

CLinuxHandleFinder::~CLinuxHandleFinder()
{
}

void CLinuxHandleFinder::run()
{
	const QList<quint64> Pids = ProcFs::EnumProcesses();

	//
	// Type filter. -1 (or an unset variant) means every type; otherwise it is a
	// CLinuxHandle::EHandleType value, matching what the handle model shows.
	//
	bool bHaveType = false;
	const int WantType = m_Type.isValid() ? m_Type.toInt(&bHaveType) : -1;
	const bool bFilterType = bHaveType && WantType >= 0;

	QList<QSharedPointer<QObject>> Batch;
	int Index = 0;

	for (quint64 Pid : Pids)
	{
		if (IsCanceled())
			break;

		emit Progress((float)(++Index) / Pids.count());

		for (quint64 Fd : ProcFs::EnumFds(Pid))
		{
			if (IsCanceled())
				break;

			// Cheap pre-filter on the raw link target, so a CLinuxHandle is only
			// constructed for entries that can actually match.
			const QString Target = ProcFs::ReadLink(ProcFs::ProcPath(Pid, QString("fd/%1").arg(Fd)));
			if (Target.isEmpty())
				continue; // closed, or fd table not readable for this process
			if (!m_RegExp.match(Target).hasMatch())
				continue;

			QSharedPointer<CLinuxHandle> pHandle = QSharedPointer<CLinuxHandle>(new CLinuxHandle());
			if (!pHandle->InitStaticData(Pid, Fd))
				continue;

			if (bFilterType && (int)pHandle->GetTypeIndex() != WantType)
				continue;

			// The results view shows which process each hit belongs to.
			if (theAPI)
				pHandle->SetProcess(theAPI->GetProcessByID(Pid));

			Batch.append(pHandle);

			// Emitted in batches so the view fills in progressively rather than
			// all at once when the scan completes.
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
