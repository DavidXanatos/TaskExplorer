#pragma once

#include <QString>
#include <QStringList>
#include <QByteArray>

//
// What the desktop knows about a program, which on Linux is where the metadata
// lives.
//
// A Windows executable carries its own description, company, version and icon
// in the file. An ELF binary carries none of that: /usr/bin/firefox is a file
// with a name and nothing else to say about itself. What the desktop shows
// instead comes from a freedesktop desktop entry - the same .desktop file a
// launcher reads - and that is a file this can read too.
//
// Which is also why this is display-server independent, and the answer to
// "how do we do icons on Wayland": nothing here talks to a display. A taskbar
// resolves an icon by taking the toplevel's app_id or WM_CLASS, finding the
// matching entry, and looking its Icon= up in the theme; two of those three
// steps need no window at all, and the first has a fallback - the name of the
// program that is running - that works headless, over the wire, and for a
// process that has no window yet.
//
// Everything is read once into an index and kept. A desktop entry appearing
// while TaskExplorer runs is missed, which is the trade the description index
// already made for not walking a few thousand files per refresh.
//

struct SDesktopEntry
{
	bool	Valid = false;

	//
	// The entry's id - "org.mozilla.firefox" from org.mozilla.firefox.desktop.
	// This is what a Wayland compositor matches an app_id against and what
	// Windows would call an application user model id, which is why it goes in
	// the same column.
	//
	QString	Id;

	QString	Name;			// "Firefox Web Browser"
	QString	GenericName;	// "Web Browser"
	QString	Comment;		// "Browse the World Wide Web"
	QString	Icon;			// an icon theme name, or an absolute path
	QString	Categories;		// "Network;WebBrowser;"
	QString	WmClass;		// StartupWMClass, for matching a window to this

	bool	Terminal = false;
	bool	NoDisplay = false;
};

//
// The entry for a running program, matched on the executable's basename - the
// same match LinuxDescribeExecutable makes, and for the same reason: it is the
// one key that exists for every process, with or without a window.
//
SDesktopEntry	LinuxDesktopEntry(const QString& ExePath);

//
// And by the window class a toplevel reports, which is what a taskbar matches
// on. More accurate than the basename where an entry declares it - one binary
// can back several entries - and only available where windows can be seen at
// all, which on Linux means X11.
//
SDesktopEntry	LinuxDesktopEntryForWmClass(const QString& WmClass);

//
// An icon theme name resolved to the bytes of an image file.
//
// Bytes, not a QIcon or a QPixmap: those are drawing classes that need a
// platform plugin, and this runs in a collector that may be a daemon with no
// display and whose answers may be drawn on another machine entirely. The same
// decision, for the same reasons, as the distribution logo in CLinuxAPI and the
// module icons in CModuleInfo.
//
// PNG when there is one, SVG otherwise; the viewer's image reader handles both
// and should not be told which to expect. Empty when the name resolves to
// nothing, which is not an error - most daemons have no icon and are better
// shown without one than with a guess.
//
QByteArray		LinuxResolveIcon(const QString& IconName, int PreferredSize = 48);

//
// The icon for a running program, by executable path. Cached, because a hundred
// processes of one image should read one file once.
//
QByteArray		LinuxExecutableIcon(const QString& ExePath, int PreferredSize = 48);
