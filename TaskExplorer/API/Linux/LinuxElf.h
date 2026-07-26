#pragma once

#include <QString>
#include <QStringList>

//
// A small read-only ELF inspector, used to describe a mapped module the way the
// Windows build uses VERSIONINFO.
//
// ELF has no direct equivalent of a VERSIONINFO resource, so the information
// comes from three places, in descending order of authority:
//
//   1. .note.package - a JSON blob some distributions (and anything built with
//      systemd's package-metadata support) embed, naming the package, its
//      version and the distribution. When present this is exact.
//   2. DT_SONAME in .dynamic - the library's canonical name, which by
//      convention carries the ABI version ("libc.so.6").
//   3. The file name itself, once symlinks are resolved ("libfoo.so.1.2.3").
//
// Everything is read straight out of the file with plain seeks - no libelf
// dependency, and nothing is mapped into this process.
//
struct SElfInfo
{
	bool		Valid = false;

	quint16		Type = 0;			// ET_EXEC, ET_DYN, ...
	quint16		Machine = 0;		// EM_X86_64, ...
	bool		Is64Bit = false;

	QString		SoName;				// DT_SONAME, e.g. "libc.so.6"
	QString		Interpreter;		// PT_INTERP, e.g. "/lib64/ld-linux-x86-64.so.2"
	QStringList	Needed;				// DT_NEEDED

	QString		BuildId;			// .note.gnu.build-id, lower case hex

	// From .note.package, empty when the binary carries no such note.
	QString		PackageName;
	QString		PackageVersion;
	QString		PackageOs;

	// True for a position independent executable, which on Linux is reported by
	// the kernel as ET_DYN and is otherwise indistinguishable from a library.
	bool		IsPie = false;
};

//
// Reads Path and returns what could be determined. Never raises; an unreadable
// or non-ELF file simply comes back with Valid == false.
//
SElfInfo	ReadElfInfo(const QString& Path);

//
// Best-effort version string for a module, derived from the SONAME or, failing
// that, from the file name with its symlinks resolved.
//
// "libc.so.6" gives "6"; a "libfoo.so" that resolves to "libfoo.so.1.2.3"
// gives "1.2.3". Returns an empty string when the name carries no version,
// which is the normal case for an executable.
//
QString		ElfVersionFromName(const QString& Path, const QString& SoName);
