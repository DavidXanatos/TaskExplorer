#include "stdafx.h"
#include "PackageInfo.h"
#include "ProcFs.h"
#include "../ModuleInfo.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QProcess>

//
// ---- dpkg ----
//
// /var/lib/dpkg/info/<package>.list is a plain list of the paths a package
// owns, one per line, and there is one such file per installed package. The
// reverse index is built by reading all of them once.
//
// Only paths that can be a process image are kept. A full index of every file
// on a desktop system is a few hundred thousand entries and tens of megabytes
// for a question only ever asked about executables; these prefixes cover every
// place a program actually runs from and cut it by an order of magnitude.
//
static bool IsExecutablePath(const QString& Path)
{
	return Path.startsWith("/usr/bin/") || Path.startsWith("/usr/sbin/")
		|| Path.startsWith("/bin/") || Path.startsWith("/sbin/")
		|| Path.startsWith("/usr/games/") || Path.startsWith("/usr/libexec/")
		|| Path.startsWith("/usr/lib/") || Path.startsWith("/lib/")
		|| Path.startsWith("/opt/");
}

struct SDpkgIndex
{
	bool					bValid = false;
	QHash<QString, QString>	FileToPackage;		// "/usr/bin/firefox" -> "firefox"
	QHash<QString, QString>	PackageVersion;
	QHash<QString, QString>	PackageMaintainer;
};

static const SDpkgIndex& DpkgIndex()
{
	static QMutex Lock;
	static SDpkgIndex Index;
	static bool bBuilt = false;

	QMutexLocker Locker(&Lock);
	if (bBuilt)
		return Index;
	bBuilt = true;

	QDir Info("/var/lib/dpkg/info");
	if (!Info.exists())
		return Index;

	Index.bValid = true;

	QDirIterator It("/var/lib/dpkg/info", QStringList() << "*.list", QDir::Files);
	while (It.hasNext())
	{
		const QString ListPath = It.next();

		//
		// "firefox.list" or "firefox:amd64.list"; the architecture is part of
		// the file name and not part of the package's name.
		//
		const QString Package = QFileInfo(ListPath).completeBaseName().section(':', 0, 0);

		QFile File(ListPath);
		if (!File.open(QIODevice::ReadOnly))
			continue;

		while (!File.atEnd())
		{
			const QByteArray Line = File.readLine().trimmed();
			if (Line.isEmpty())
				continue;

			const QString Path = QString::fromUtf8(Line);
			if (IsExecutablePath(Path))
				Index.FileToPackage.insert(Path, Package);
		}
		File.close();
	}

	//
	// Versions and maintainers, from the one file that has them. Parsed as
	// paragraphs: a blank line ends a package's stanza.
	//
	QFile Status("/var/lib/dpkg/status");
	if (Status.open(QIODevice::ReadOnly))
	{
		QString Package, Version, Maintainer;
		while (true)
		{
			const bool bEnd = Status.atEnd();
			const QString Line = bEnd ? QString() : QString::fromUtf8(Status.readLine()).trimmed();

			if (Line.isEmpty())
			{
				if (!Package.isEmpty())
				{
					if (!Version.isEmpty())
						Index.PackageVersion.insert(Package, Version);
					if (!Maintainer.isEmpty())
						Index.PackageMaintainer.insert(Package, Maintainer);
				}
				Package.clear();
				Version.clear();
				Maintainer.clear();

				if (bEnd)
					break;
				continue;
			}

			if (Line.startsWith("Package: "))			Package = Line.mid(9);
			else if (Line.startsWith("Version: "))		Version = Line.mid(9);
			else if (Line.startsWith("Maintainer: "))	Maintainer = Line.mid(12);
		}
		Status.close();
	}

	return Index;
}

//
// Whether the file still matches what was installed.
//
// <package>.md5sums holds "<md5>  <path without the leading slash>" for every
// file the package shipped. Missing means the package recorded no checksums,
// which several do - that is "unknown", not "modified".
//
static int DpkgVerify(const QString& Package, const QString& Path)
{
	QFile Sums("/var/lib/dpkg/info/" + Package + ".md5sums");
	if (!Sums.open(QIODevice::ReadOnly))
	{
		Sums.setFileName("/var/lib/dpkg/info/" + Package + ":" + QSysInfo::buildCpuArchitecture() + ".md5sums");
		if (!Sums.open(QIODevice::ReadOnly))
			return CModuleInfo::VrUnknown;
	}

	const QByteArray Want = Path.mid(1).toUtf8();	// the file lists them without the leading /
	QByteArray Expected;

	while (!Sums.atEnd())
	{
		const QByteArray Line = Sums.readLine();
		const int Split = Line.indexOf("  ");
		if (Split <= 0)
			continue;

		if (Line.mid(Split + 2).trimmed() == Want)
		{
			Expected = Line.left(Split);
			break;
		}
	}
	Sums.close();

	if (Expected.isEmpty())
		return CModuleInfo::VrUnknown;

	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly))
		return CModuleInfo::VrUnknown;

	//
	// Hashed in chunks rather than read whole: this runs for every distinct
	// image on the machine, and some of them are large.
	//
	QCryptographicHash Hash(QCryptographicHash::Md5);
	if (!Hash.addData(&File))
	{
		File.close();
		return CModuleInfo::VrUnknown;
	}
	File.close();

	return (Hash.result().toHex() == Expected) ? CModuleInfo::VrTrusted : CModuleInfo::VrBadSignature;
}

//
// ---- pacman ----
//
// /var/lib/pacman/local/<name>-<version>/files lists the owned paths under a
// %FILES% header, without the leading slash. desc has the name, the version and
// the packager. Verification would mean parsing the gzipped mtree, which is
// more machinery than the answer is worth here.
//
struct SPacmanIndex
{
	bool					bValid = false;
	QHash<QString, QString>	FileToPackage;
	QHash<QString, QString>	PackageVersion;
	QHash<QString, QString>	PackageMaintainer;
};

static const SPacmanIndex& PacmanIndex()
{
	static QMutex Lock;
	static SPacmanIndex Index;
	static bool bBuilt = false;

	QMutexLocker Locker(&Lock);
	if (bBuilt)
		return Index;
	bBuilt = true;

	QDir Local("/var/lib/pacman/local");
	if (!Local.exists())
		return Index;

	Index.bValid = true;

	foreach(const QString& Dir, Local.entryList(QDir::Dirs | QDir::NoDotAndDotDot))
	{
		const QString Base = "/var/lib/pacman/local/" + Dir;

		QString Package, Version, Packager;

		QFile Desc(Base + "/desc");
		if (Desc.open(QIODevice::ReadOnly))
		{
			QString Section;
			while (!Desc.atEnd())
			{
				const QString Line = QString::fromUtf8(Desc.readLine()).trimmed();
				if (Line.startsWith('%'))
				{
					Section = Line;
					continue;
				}
				if (Line.isEmpty())
					continue;

				if (Section == "%NAME%")			Package = Line;
				else if (Section == "%VERSION%")	Version = Line;
				else if (Section == "%PACKAGER%")	Packager = Line;
			}
			Desc.close();
		}

		if (Package.isEmpty())
			continue;

		Index.PackageVersion.insert(Package, Version);
		Index.PackageMaintainer.insert(Package, Packager);

		QFile Files(Base + "/files");
		if (!Files.open(QIODevice::ReadOnly))
			continue;

		bool bInFiles = false;
		while (!Files.atEnd())
		{
			const QString Line = QString::fromUtf8(Files.readLine()).trimmed();
			if (Line.startsWith('%'))
			{
				bInFiles = (Line == "%FILES%");
				continue;
			}
			if (!bInFiles || Line.isEmpty())
				continue;

			const QString Path = "/" + Line;
			if (IsExecutablePath(Path))
				Index.FileToPackage.insert(Path, Package);
		}
		Files.close();
	}

	return Index;
}

//
// ---- rpm ----
//
// The only one that cannot be read as a file: the database needs librpm, which
// is not a dependency worth taking for one column. So the command answers, once
// per distinct path, and the result is cached with everything else.
//
static bool RpmAvailable()
{
	static const bool bHave = QFileInfo::exists("/var/lib/rpm") || QFileInfo::exists("/usr/lib/sysimage/rpm");
	return bHave;
}

static SPackageInfo RpmQuery(const QString& Path)
{
	SPackageInfo Info;

	QProcess Rpm;
	Rpm.start("rpm", QStringList() << "-qf" << "--queryformat"
		<< "%{NAME}\\n%{VERSION}-%{RELEASE}\\n%{VENDOR}\\n" << Path);
	if (!Rpm.waitForFinished(3000) || Rpm.exitCode() != 0)
		return Info;

	const QStringList Lines = QString::fromUtf8(Rpm.readAllStandardOutput()).split('\n');
	if (Lines.count() < 3 || Lines[0].isEmpty())
		return Info;

	Info.Valid = true;
	Info.Name = Lines[0];
	Info.Version = Lines[1];
	Info.Maintainer = Lines[2];
	Info.Origin = "rpm";

	//
	// rpm -V prints one line per file that differs and nothing at all when
	// everything matches, which is the whole answer. "5" in the flags column is
	// the digest - the others are mode, ownership, timestamps and so on, which
	// are not what this column is about.
	//
	QProcess Verify;
	Verify.start("rpm", QStringList() << "-Vf" << Path);
	if (Verify.waitForFinished(5000))
	{
		const QString Output = QString::fromUtf8(Verify.readAllStandardOutput());
		Info.Verify = CModuleInfo::VrTrusted;
		foreach(const QString& Line, Output.split('\n'))
		{
			if (Line.contains(Path) && Line.left(9).contains('5'))
			{
				Info.Verify = CModuleInfo::VrBadSignature;
				break;
			}
		}
	}
	else
		Info.Verify = CModuleInfo::VrUnknown;

	return Info;
}

//
// ---- what owns this file ----
//

static SPackageInfo PackageForFileUncached(const QString& Path)
{
	SPackageInfo Info;

	//
	// Nothing to say about a file that is not there. A path that has been
	// deleted or replaced - a program running from an image that was upgraded
	// under it - is a real case, and reporting it as "belongs to no package"
	// would be a claim about a file rather than about its absence.
	//
	if (!QFileInfo::exists(Path))
		return Info;

	//
	// A snap or a flatpak is its own package, and its path says which. Asked
	// first because those files belong to no distribution package and the
	// databases below would rightly say so.
	//
	if (Path.startsWith("/snap/"))
	{
		Info.Valid = true;
		Info.Name = Path.section('/', 2, 2);
		Info.Origin = "snap";

		//
		// The revision is the directory below the name and is what a snap
		// actually has instead of a version; the human version is in the yaml
		// beside it.
		//
		QFile Yaml(QString("/snap/%1/current/meta/snap.yaml").arg(Info.Name));
		if (Yaml.open(QIODevice::ReadOnly))
		{
			while (!Yaml.atEnd())
			{
				const QString Line = QString::fromUtf8(Yaml.readLine()).trimmed();
				if (Line.startsWith("version:"))
				{
					Info.Version = Line.mid(8).trimmed();
					break;
				}
			}
			Yaml.close();
		}
		if (Info.Version.isEmpty())
			Info.Version = Path.section('/', 3, 3);

		//
		// Snaps are signed by the store and snapd checks that at install; what
		// is on disk is a read-only squashfs image that cannot be edited in
		// place. Reporting it as trusted says exactly that.
		//
		Info.Verify = CModuleInfo::VrTrusted;
		return Info;
	}

	const SDpkgIndex& Dpkg = DpkgIndex();
	if (Dpkg.bValid)
	{
		const QString Package = Dpkg.FileToPackage.value(Path);
		if (!Package.isEmpty())
		{
			Info.Valid = true;
			Info.Name = Package;
			Info.Version = Dpkg.PackageVersion.value(Package);
			Info.Maintainer = Dpkg.PackageMaintainer.value(Package);
			Info.Origin = "deb";
			Info.Verify = DpkgVerify(Package, Path);
			return Info;
		}

		//
		// A path this database covers and does not list is genuinely
		// unpackaged - built here, copied in, or installed by something that is
		// not the package manager.
		//
		if (IsExecutablePath(Path))
		{
			Info.Valid = true;
			Info.Verify = CModuleInfo::VrNoSignature;
			return Info;
		}
		return Info;
	}

	const SPacmanIndex& Pacman = PacmanIndex();
	if (Pacman.bValid)
	{
		const QString Package = Pacman.FileToPackage.value(Path);
		if (!Package.isEmpty())
		{
			Info.Valid = true;
			Info.Name = Package;
			Info.Version = Pacman.PackageVersion.value(Package);
			Info.Maintainer = Pacman.PackageMaintainer.value(Package);
			Info.Origin = "pkg";
			Info.Verify = CModuleInfo::VrUnknown;	// the mtree is not read
			return Info;
		}

		if (IsExecutablePath(Path))
		{
			Info.Valid = true;
			Info.Verify = CModuleInfo::VrNoSignature;
		}
		return Info;
	}

	if (RpmAvailable())
		return RpmQuery(Path);

	return Info;
}

SPackageInfo LinuxPackageForFile(const QString& Path)
{
	if (Path.isEmpty() || !Path.startsWith('/'))
		return SPackageInfo();

	static QMutex Lock;
	static QHash<QString, SPackageInfo> Cache;

	QMutexLocker Locker(&Lock);
	auto It = Cache.find(Path);
	if (It != Cache.end())
		return It.value();
	Locker.unlock();

	const SPackageInfo Info = PackageForFileUncached(Path);

	Locker.relock();
	Cache.insert(Path, Info);
	return Info;
}

//
// ---- the sandbox ----
//

SSandboxInfo LinuxSandboxForProcess(quint64 Pid, const QString& Confinement)
{
	SSandboxInfo Info;

	//
	// flatpak writes what this instance was actually started with into its own
	// root, which is better than any manifest on the host: it is the permission
	// set of *this* process, overrides and all.
	//
	const QString FlatpakInfo = ProcFs::ProcPath(Pid, "root/.flatpak-info");
	if (ProcFs::FileExists(FlatpakInfo))
	{
		Info.Valid = true;
		Info.Kind = "flatpak";
		Info.Confinement = "sandboxed";

		QString Section;
		foreach(const QByteArray& Raw, ProcFs::ReadFile(FlatpakInfo).split('\n'))
		{
			const QString Line = QString::fromUtf8(Raw).trimmed();
			if (Line.startsWith('['))
			{
				Section = Line;
				continue;
			}

			const int Equals = Line.indexOf('=');
			if (Equals <= 0)
				continue;

			const QString Key = Line.left(Equals);
			const QString Value = Line.mid(Equals + 1);

			if (Section == "[Application]" && Key == "name")
				Info.AppId = Value;

			//
			// The permission keys, kept in the sandbox's own words. Each is a
			// semicolon separated list; an empty one means the permission was
			// not granted and is not worth a row.
			//
			if (Section == "[Context]")
			{
				foreach(const QString& Item, Value.split(';', Qt::SkipEmptyParts))
					Info.Permissions += QString("%1=%2").arg(Key, Item);
			}
		}
		return Info;
	}

	//
	// A snap says what it is confined to in its yaml, and the AppArmor profile
	// the caller already read says which snap this is - "snap.firefox.firefox".
	//
	if (Confinement.startsWith("snap."))
	{
		Info.Valid = true;
		Info.Kind = "snap";
		Info.AppId = Confinement.section(' ', 0, 0).section('.', 1, 1);
		Info.Confinement = "strict";	// snapd's default, overridden below

		QFile Yaml(QString("/snap/%1/current/meta/snap.yaml").arg(Info.AppId));
		if (Yaml.open(QIODevice::ReadOnly))
		{
			//
			// Read as lines rather than as YAML. What is wanted is one scalar
			// and one list of scalars, both at a known indentation, and a YAML
			// parser is a dependency this does not need for that.
			//
			//
			// Only the names, and only the ones one level under "plugs:". A plug
			// can carry a mapping of its own - what it reads, where it mounts,
			// with which options - and taking every line of that produced a
			// permission list of "read", "mount", "ro" and file paths. What
			// belongs in a column is the name of the interface; the rest is that
			// interface's configuration.
			//
			bool bInPlugs = false;
			int PlugIndent = -1;

			while (!Yaml.atEnd())
			{
				const QString Raw = QString::fromUtf8(Yaml.readLine());
				const QString Line = Raw.trimmed();

				if (Line.isEmpty() || Line.startsWith('#'))
					continue;

				int Indent = 0;
				while (Indent < Raw.length() && (Raw.at(Indent) == QChar(' ') || Raw.at(Indent) == QChar('\t')))
					Indent++;

				//
				// Anything back at column zero ends the section, including the
				// next top level key.
				//
				if (Indent == 0)
				{
					bInPlugs = Line.startsWith("plugs:");
					PlugIndent = -1;

					if (Line.startsWith("confinement:"))
						Info.Confinement = Line.mid(12).trimmed();
					continue;
				}

				if (!bInPlugs)
					continue;

				if (PlugIndent < 0)
					PlugIndent = Indent;
				if (Indent != PlugIndent)
					continue;			// an attribute of the plug above, not a plug

				QString Name = Line;
				if (Name.startsWith("- "))
					Name = Name.mid(2).trimmed();
				if (Name.endsWith(':'))
					Name.chop(1);

				if (!Name.isEmpty())
					Info.Permissions += Name.trimmed();
			}
			Yaml.close();
		}
		return Info;
	}

	return Info;
}

quint32 LinuxMitigationFlags(const QMap<QString, QString>& Status)
{
	quint32 Flags = 0;

	//
	// Seccomp: 0 disabled, 1 strict, 2 filtered. Filtered is what a sandboxed
	// or hardened program uses; strict is the almost unusable original mode and
	// is worth distinguishing because seeing it at all is remarkable.
	//
	const int Seccomp = Status.value("Seccomp").trimmed().toInt();
	if (Seccomp == 1)
		Flags |= LINUX_MITIGATION_SECCOMP | LINUX_MITIGATION_SECCOMP_STRICT;
	else if (Seccomp >= 2)
		Flags |= LINUX_MITIGATION_SECCOMP;

	if (Status.value("NoNewPrivs").trimmed().toInt() != 0)
		Flags |= LINUX_MITIGATION_NO_NEW_PRIVS;

	return Flags;
}
