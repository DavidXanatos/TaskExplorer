#include "stdafx.h"
#include "DesktopInfo.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QMutex>
#include <QMutexLocker>
#include <QStandardPaths>

//
// The program a desktop entry actually runs.
//
// Exec is a command line, not a path: field codes (%u, %f), arguments and
// wrapper programs all have to come off before the basename means anything.
// "env FOO=1 /usr/bin/foo %u" has to yield "foo".
//
static QString ExecBasename(const QString& Exec)
{
	const QStringList Tokens = Exec.split(' ', Qt::SkipEmptyParts);
	for (int i = 0; i < Tokens.count(); i++)
	{
		const QString Token = Tokens[i];

		if (Token.startsWith('%'))		// a field code; the rest is arguments
			break;
		if (Token.contains('='))		// an environment assignment for "env"
			continue;

		const QString Base = Token.section('/', -1);
		if (Base == "env" || Base == "sh" || Base == "bash" || Base == "flatpak" || Base == "snap")
			continue;					// a wrapper; the real program is further along

		return Base;
	}
	return QString();
}

//
// One .desktop file, parsed.
//
// pExec receives the command line, which the index needs and the entry does
// not: what a caller wants to know is what the program is called and what it
// looks like, not how the launcher starts it.
//
static SDesktopEntry ParseDesktopFile(const QString& Path, QString* pExec)
{
	SDesktopEntry Entry;

	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly | QIODevice::Text))
		return Entry;

	bool bInEntry = false;

	while (!File.atEnd())
	{
		const QString Line = QString::fromUtf8(File.readLine()).trimmed();

		if (Line.startsWith('['))
		{
			//
			// Only the main group describes the application; the
			// "Desktop Action ..." groups describe menu entries, and their
			// Name and Icon belong to the action rather than to the program.
			//
			bInEntry = (Line == "[Desktop Entry]");
			if (!bInEntry && !pExec->isEmpty())
				break;
			continue;
		}

		if (!bInEntry)
			continue;

		//
		// The plain keys only, never a localised "Name[xx]" one: these strings
		// end up next to process names and file paths, which are not
		// translated, and a half translated tree reads worse than an
		// untranslated one. The same rule the description index followed.
		//
		const int Equals = Line.indexOf('=');
		if (Equals <= 0)
			continue;

		const QString Key = Line.left(Equals);
		const QString Value = Line.mid(Equals + 1).trimmed();

		if (Key == "Exec")					*pExec = Value;
		else if (Key == "Name")				Entry.Name = Value;
		else if (Key == "GenericName")		Entry.GenericName = Value;
		else if (Key == "Comment")			Entry.Comment = Value;
		else if (Key == "Icon")				Entry.Icon = Value;
		else if (Key == "Categories")		Entry.Categories = Value;
		else if (Key == "StartupWMClass")	Entry.WmClass = Value;
		else if (Key == "Terminal")			Entry.Terminal = (Value.compare("true", Qt::CaseInsensitive) == 0);
		else if (Key == "NoDisplay")		Entry.NoDisplay = (Value.compare("true", Qt::CaseInsensitive) == 0);
	}
	File.close();

	if (Entry.Name.isEmpty() && pExec->isEmpty())
		return Entry;

	//
	// The id is the file name without its suffix - "org.mozilla.firefox" - which
	// is what a Wayland compositor matches an app_id against and what Windows
	// would call an application user model id.
	//
	Entry.Id = QFileInfo(Path).completeBaseName();
	Entry.Valid = true;
	return Entry;
}

struct SDesktopIndex
{
	QHash<QString, SDesktopEntry> ByExec;
	QHash<QString, SDesktopEntry> ByWmClass;
};

//
// Built once and kept, like the description index it replaces: an entry
// appearing while TaskExplorer runs is missed, which is the price of not
// walking a few thousand files on every refresh of the process list.
//
static const SDesktopIndex& DesktopIndex()
{
	static QMutex Lock;
	static SDesktopIndex Index;
	static bool bBuilt = false;

	QMutexLocker Locker(&Lock);
	if (bBuilt)
		return Index;
	bBuilt = true;

	//
	// XDG search order, least significant first, so a user's own entry
	// overwrites the packaged one rather than losing to it. The list covers
	// flatpak and snap exports too, because both put their entries in
	// directories that XDG_DATA_DIRS names.
	//
	QStringList Dirs = QStandardPaths::standardLocations(QStandardPaths::ApplicationsLocation);
	std::reverse(Dirs.begin(), Dirs.end());

	foreach(const QString& Dir, Dirs)
	{
		QDirIterator It(Dir, QStringList() << "*.desktop", QDir::Files, QDirIterator::Subdirectories);
		while (It.hasNext())
		{
			const QString Path = It.next();

			QString Exec;
			const SDesktopEntry Entry = ParseDesktopFile(Path, &Exec);
			if (!Entry.Valid)
				continue;

			const QString Base = ExecBasename(Exec);
			if (!Base.isEmpty())
				Index.ByExec.insert(Base, Entry);

			if (!Entry.WmClass.isEmpty())
				Index.ByWmClass.insert(Entry.WmClass.toLower(), Entry);
		}
	}

	return Index;
}

SDesktopEntry LinuxDesktopEntry(const QString& ExePath)
{
	if (ExePath.isEmpty())
		return SDesktopEntry();

	return DesktopIndex().ByExec.value(ExePath.section('/', -1));
}

SDesktopEntry LinuxDesktopEntryForWmClass(const QString& WmClass)
{
	if (WmClass.isEmpty())
		return SDesktopEntry();

	const SDesktopIndex& Index = DesktopIndex();

	SDesktopEntry Entry = Index.ByWmClass.value(WmClass.toLower());
	if (Entry.Valid)
		return Entry;

	//
	// A class that names no entry is very often the program's own name, which
	// is the other key into the same table - "firefox" as a class and "firefox"
	// as a binary.
	//
	return Index.ByExec.value(WmClass);
}

//
// ---- icons ----
//
// The freedesktop icon theme lookup, minus the parts that need a display.
//
// The spec's full algorithm reads index.theme for each theme, follows Inherits
// chains and picks by size and scale. What is done here is the part that
// answers correctly for practically every entry: look through the theme
// directories for a file called <name>, largest useful size first, and fall
// back to hicolor and to /usr/share/pixmaps.
//
// Deliberately no QIcon::fromTheme. QIcon is a drawing class, resolving a theme
// needs a platform plugin, and this runs in a collector that may be a service
// with no display - and whose answer may be drawn on another machine anyway.
// See the same decision for the distribution logo in CLinuxAPI.
//
static QStringList IconSearchDirs()
{
	QStringList Dirs;

	//
	// The user's own first, then the system's, which is the order the spec
	// gives. XDG_DATA_DIRS again covers flatpak and snap exports.
	//
	Dirs += QDir::homePath() + "/.local/share/icons";
	Dirs += QDir::homePath() + "/.icons";

	foreach(const QString& Dir, QStandardPaths::standardLocations(QStandardPaths::GenericDataLocation))
		Dirs += Dir + "/icons";

	Dirs += "/usr/share/icons";
	Dirs += "/usr/local/share/icons";

	Dirs.removeDuplicates();
	return Dirs;
}

//
// Which themes to look in, and in what order.
//
// The desktop's current theme is not knowable here - it is in a GTK or Qt
// setting belonging to a session this process may not be part of - so this
// takes the themes that exist, with the common ones first and hicolor last.
// hicolor is the one theme every application installs into and the spec's
// mandatory fallback, so it is what makes this work at all; the others only
// make the result prettier when they happen to be installed.
//
static QStringList IconThemes(const QString& Dir)
{
	QStringList Themes;
	foreach(const QString& Name, QStringList() << "Adwaita" << "breeze" << "Papirus" << "Yaru" << "gnome" << "oxygen")
	{
		if (QFileInfo::exists(Dir + "/" + Name))
			Themes += Name;
	}
	Themes += "hicolor";
	return Themes;
}

QByteArray LinuxResolveIcon(const QString& IconName, int PreferredSize)
{
	if (IconName.isEmpty())
		return QByteArray();

	QStringList Candidates;

	//
	// An absolute path is legal in Icon= and means exactly itself.
	//
	if (IconName.startsWith('/'))
		Candidates += IconName;
	else
	{
		//
		// A raster icon of a useful size first, and scalable last.
		//
		// Not what the spec would do - an SVG is every size and would be the
		// better answer - but this is bytes on their way to somebody else's
		// image reader, and decoding SVG needs Qt's SVG image plugin at the far
		// end. A viewer without it draws nothing at all, which is how this was
		// found: every icon here resolved to an SVG and every row showed the
		// placeholder. PNG needs no plugin anywhere.
		//
		// Sizes are near-first rather than largest-first: scaling down is
		// cheap and scaling up is ugly, so 48 and 64 beat 256.
		//
		QStringList Sizes;
		foreach(int Size, QList<int>() << PreferredSize << 64 << 48 << 32 << 128 << 256 << 24 << 16)
		{
			const QString Dir = QString("%1x%1").arg(Size);
			if (!Sizes.contains(Dir))
				Sizes += Dir;
		}
		Sizes << "scalable";

		foreach(const QString& Dir, IconSearchDirs())
		{
			foreach(const QString& Theme, IconThemes(Dir))
			{
				foreach(const QString& Size, Sizes)
				{
					//
					// The category directory under the size - apps, devices,
					// places - is not worth guessing at: "apps" is where a
					// program's icon goes, and anything else is looked for by
					// the glob below.
					//
					Candidates += QString("%1/%2/%3/apps/%4.png").arg(Dir, Theme, Size, IconName);
					Candidates += QString("%1/%2/%3/apps/%4.svg").arg(Dir, Theme, Size, IconName);
					Candidates += QString("%1/%2/apps/%3/%4.png").arg(Dir, Theme, Size, IconName);
					Candidates += QString("%1/%2/apps/%3/%4.svg").arg(Dir, Theme, Size, IconName);
				}
			}
		}

		//
		// Where a program that ships no theme icon puts one, and where a
		// distribution's own logo lives.
		//
		foreach(const QString& Suffix, QStringList() << ".png" << ".xpm" << ".svg")
		{
			Candidates += "/usr/share/pixmaps/" + IconName + Suffix;
			Candidates += QDir::homePath() + "/.local/share/pixmaps/" + IconName + Suffix;
		}
	}

	foreach(const QString& Path, Candidates)
	{
		QFile File(Path);
		if (!File.open(QIODevice::ReadOnly))
			continue;

		//
		// A cap, because this is read into memory and then sent over a socket.
		// Icons are kilobytes; anything past this is not an icon and should not
		// become one refresh's worth of traffic.
		//
		const QByteArray Data = File.read(512 * 1024);
		File.close();

		if (!Data.isEmpty())
			return Data;
	}

	return QByteArray();
}

QByteArray LinuxExecutableIcon(const QString& ExePath, int PreferredSize)
{
	if (ExePath.isEmpty())
		return QByteArray();

	//
	// Cached by path, because a hundred processes of one image must read one
	// file once - and because a miss is worth remembering too: most processes on
	// a machine have no desktop entry, and looking for one over and over is the
	// expensive half of this.
	//
	static QMutex Lock;
	static QHash<QString, QByteArray> Cache;

	QMutexLocker Locker(&Lock);
	auto It = Cache.find(ExePath);
	if (It != Cache.end())
		return It.value();

	Locker.unlock();

	const SDesktopEntry Entry = LinuxDesktopEntry(ExePath);
	const QByteArray Icon = Entry.Valid ? LinuxResolveIcon(Entry.Icon, PreferredSize) : QByteArray();

	Locker.relock();
	Cache.insert(ExePath, Icon);
	return Icon;
}
