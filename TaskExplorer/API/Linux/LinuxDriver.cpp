#include "stdafx.h"
#include "LinuxDriver.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>

#include <sys/utsname.h>

//
// Maps a loaded module's name to the file it was loaded from.
//
// The index comes from modules.dep rather than from running modinfo: it is the
// same list depmod generated for modprobe, it is one file, and it avoids
// spawning a process per module - a desktop kernel has well over a hundred
// modules loaded.
//
// Module names and file names do not always agree. The kernel normalises dashes
// to underscores, so "snd-hda-intel.ko" appears in /proc/modules as
// "snd_hda_intel"; the index is keyed on the normalised form.
//
static QString ResolveModulePath(const QString& Name)
{
	static QMutex Lock;
	static QHash<QString, QString> Index;
	static bool bBuilt = false;

	QMutexLocker Locker(&Lock);

	if (!bBuilt)
	{
		bBuilt = true;

		struct utsname Uts;
		if (uname(&Uts) == 0)
		{
			const QString ModulesDir = QString("/lib/modules/%1").arg(QString::fromLocal8Bit(Uts.release));

			QFile Deps(ModulesDir + "/modules.dep");
			if (Deps.open(QIODevice::ReadOnly | QIODevice::Text))
			{
				while (!Deps.atEnd())
				{
					// "kernel/drivers/net/foo.ko.zst: kernel/drivers/bar.ko.zst ..."
					const QString Line = QString::fromUtf8(Deps.readLine());
					const QString Path = Line.section(':', 0, 0).trimmed();
					if (Path.isEmpty())
						continue;

					//
					// Strip the compression suffix and then ".ko" to recover the
					// module name. Distributions ship .ko, .ko.xz, .ko.gz and
					// .ko.zst, so this cannot assume one of them.
					//
					QString Base = Path.section('/', -1);
					if (Base.endsWith(".zst") || Base.endsWith(".xz") || Base.endsWith(".gz"))
						Base = Base.section('.', 0, -2);
					if (!Base.endsWith(".ko"))
						continue;
					Base.chop(3);

					Index.insert(Base.replace('-', '_'), ModulesDir + "/" + Path);
				}
			}
		}
	}

	QString Normalised = Name;
	return Index.value(Normalised.replace('-', '_'));
}

CLinuxDriver::CLinuxDriver(QObject *parent)
	: CDriverInfo(parent)
{
	m_Size = 0;
	m_RefCount = 0;
}

CLinuxDriver::~CLinuxDriver()
{
}

bool CLinuxDriver::InitStaticData(const QString& Name)
{
	//
	// Read before taking the lock: resolving the path can touch the filesystem.
	//
	const QString Path = ResolveModulePath(Name);

	//
	// An unresolved name is normal rather than an error: a module compiled into
	// the kernel image has no file of its own, and one loaded from outside the
	// modules tree (a locally built driver, or one inserted by full path) is not
	// in modules.dep either.
	//
	QWriteLocker Locker(&m_Mutex);

	m_Name = Name;
	m_FileName = Name;
	m_BinaryPath = Path;

	return true;
}

bool CLinuxDriver::UpdateDynamicData(quint64 Size, quint32 RefCount, const QString& UsedBy, const QString& State)
{
	QWriteLocker Locker(&m_Mutex);

	bool bChanged = (m_Size != Size) || (m_RefCount != RefCount) || (m_UsedBy != UsedBy) || (m_State != State);

	m_Size = Size;
	m_RefCount = RefCount;
	m_UsedBy = UsedBy;
	m_State = State;

	return bChanged;
}
