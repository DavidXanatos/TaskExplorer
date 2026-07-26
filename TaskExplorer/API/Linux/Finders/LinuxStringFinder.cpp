#include "stdafx.h"
#include "LinuxStringFinder.h"
#include "../LinuxMemIO.h"
#include "../ProcFs.h"
#include "../../StringInfo.h"
#include "../../SystemAPI.h"

CLinuxStringFinder::CLinuxStringFinder(const SMemOptions& Options, const QRegularExpression& RegExp,
                                       const CProcessPtr& pProcess, QObject* parent)
	: CAbstractFinder(parent)
{
	m_Options = Options;
	m_RegExp = RegExp;
	m_pProcess = pProcess;
}

CLinuxStringFinder::~CLinuxStringFinder()
{
}

//
// Extracts printable runs from a block of memory and reports the ones matching
// the pattern.
//
// Two encodings are recognised, mirroring the Windows finder: single byte
// ASCII, and UTF-16LE, which shows up as alternating printable/NUL bytes.
//
static void ScanBlock(const QByteArray& Block, quint64 BlockAddress, const CAbstractFinder::SMemOptions& Options,
                      const QRegularExpression& RegExp, quint64 BaseAddress, quint64 RegionSize,
                      const CProcessPtr& pProcess, QList<QSharedPointer<QObject>>& Batch)
{
	const int MinLength = Options.MinLength > 0 ? Options.MinLength : 4;
	const char* Data = Block.constData();
	const int Size = Block.size();

	auto IsPrintable = [](unsigned char c) { return c >= 0x20 && c < 0x7F; };

	auto Emit = [&](int Start, int Length, int Stride) {
		QString Text;
		Text.reserve(Length);
		for (int i = 0; i < Length; i++)
			Text.append(QChar((unsigned char)Data[Start + i * Stride]));

		if (!RegExp.match(Text).hasMatch())
			return;

		Batch.append(QSharedPointer<CStringInfo>(new CStringInfo(
			BlockAddress + Start, (quint64)Length * Stride, BaseAddress, RegionSize, Text, pProcess)));
	};

	// ASCII
	int Run = 0;
	for (int i = 0; i < Size; i++)
	{
		if (IsPrintable((unsigned char)Data[i]))
		{
			Run++;
			continue;
		}
		if (Run >= MinLength)
			Emit(i - Run, Run, 1);
		Run = 0;
	}
	if (Run >= MinLength)
		Emit(Size - Run, Run, 1);

	if (!Options.Unicode)
		return;

	// UTF-16LE: printable byte followed by NUL, repeating.
	Run = 0;
	for (int i = 0; i + 1 < Size; i += 2)
	{
		if (IsPrintable((unsigned char)Data[i]) && Data[i + 1] == 0)
		{
			Run++;
			continue;
		}
		if (Run >= MinLength)
			Emit(i - Run * 2, Run, 2);
		Run = 0;
	}
	if (Run >= MinLength)
		Emit(Size - Run * 2, Run, 2);
}

STATUS CLinuxStringFinder::FindStrings(const CProcessPtr& pProcess)
{
	if (pProcess.isNull())
		return OK;

	const quint64 Pid = pProcess->GetProcessId();

	const QList<ProcFs::SMapEntry> Maps = ProcFs::ReadMaps(Pid);
	if (Maps.isEmpty())
		return OK; // gone, or maps not readable - not worth reporting per process

	QList<QSharedPointer<QObject>> Batch;

	for (const ProcFs::SMapEntry& Entry : Maps)
	{
		if (IsCanceled())
			break;

		// A region with no read permission cannot be scanned at all.
		if (!Entry.Read)
			continue;

		//
		// Region class filter, matching the Windows finder's options:
		//   Private - anonymous memory
		//   Image   - file backed and executable
		//   Mapped  - file backed, not executable
		//
		const bool bFileBacked = (Entry.Inode != 0);
		const bool bImage = bFileBacked && Entry.Exec;
		const bool bMapped = bFileBacked && !Entry.Exec;
		const bool bPrivate = !bFileBacked;

		if (bPrivate && !m_Options.Private) continue;
		if (bImage && !m_Options.Image) continue;
		if (bMapped && !m_Options.Mapped) continue;

		const quint64 RegionSize = Entry.End - Entry.Start;

		CLinuxMemIO Reader(Entry.Start, RegionSize, Pid);
		if (!Reader.open(QIODevice::ReadOnly))
		{
			//
			// Denied for this process entirely; there is no point walking the
			// rest of its regions. Reported with a zero status so the caller
			// keeps going with the next process rather than aborting the scan.
			//
			return ERR(tr("Cannot read memory of process %1").arg(pProcess->GetName()), 0);
		}

		//
		// Read in windows with an overlap, so a string straddling a boundary is
		// still found. The overlap is bounded by the longest string worth
		// stitching across.
		//
		const qint64 WindowSize = 1024 * 1024;
		const qint64 Overlap = 4096;

		quint64 Offset = 0;
		while (Offset < RegionSize)
		{
			if (IsCanceled())
				break;

			if (!Reader.seek((qint64)Offset))
				break;

			const QByteArray Block = Reader.read(WindowSize);
			if (Block.isEmpty())
				break; // unreadable page inside an otherwise readable region

			ScanBlock(Block, Entry.Start + Offset, m_Options, m_RegExp, Entry.Start, RegionSize, pProcess, Batch);

			if ((qint64)Block.size() < WindowSize)
				break;

			Offset += WindowSize - Overlap;

			if (Batch.count() >= 256)
			{
				emit Results(Batch);
				Batch.clear();
			}
		}
	}

	if (!Batch.isEmpty())
		emit Results(Batch);

	return OK;
}

void CLinuxStringFinder::run()
{
	if (!m_pProcess.isNull())
	{
		STATUS status = FindStrings(m_pProcess);
		if (status.IsError())
			emit Error(status.GetText(), status.GetStatus());
	}
	else
	{
		QMap<quint64, CProcessPtr> Processes = theAPI->GetProcessList();
		int Modulo = Processes.count() / 100;
		int i = 0;
		for (QMap<quint64, CProcessPtr>::iterator I = Processes.begin(); I != Processes.end() && !IsCanceled(); ++I)
		{
			if (Modulo && (i++ % Modulo) == 0)
				emit Progress(float(i) / Processes.count(), I.value()->GetName());

			STATUS status = FindStrings(I.value());
			// A zero status means "this process was not readable", which is the
			// norm when unprivileged - only a real error stops the whole scan.
			if (status.IsError() && status.GetStatus() > 0)
			{
				emit Error(status.GetText(), status.GetStatus());
				break;
			}
		}
	}

	emit Finished();
}
