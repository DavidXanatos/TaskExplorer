#pragma once

#include <QString>
#include <QStringList>

//
// Where a binary came from, and whether it is still what was installed.
//
// Windows answers both from the file: a signature says who vouches for it and
// whether it has been altered since. An ELF binary carries no signature, and
// the equivalent question is answered by the package manager instead - which
// package owns this file, at what version, from whom, and does its recorded
// checksum still match. That is the same question with the same three useful
// answers, which is why it maps onto CModuleInfo::EVerifyResult rather than
// getting a scale of its own.
//
// Nothing here shells out where a file will do. dpkg and pacman keep plain text
// databases and are read directly; rpm's is a database that needs its own
// library, so that one asks the rpm command and caches hard.
//

struct SPackageInfo
{
	bool	Valid = false;

	QString	Name;			// "firefox", "org.mozilla.firefox"
	QString	Version;		// "1:128.3.0esr-1"
	QString	Maintainer;		// who packaged it - the closest thing to a signer
	QString	Origin;			// "deb", "rpm", "pkg", "flatpak", "snap"

	//
	// CModuleInfo::EVerifyResult, as a plain int so that this header does not
	// have to pull the module interface in:
	//
	//   VrTrusted        owned by a package and matching its recorded checksum
	//   VrBadSignature   owned, and altered since it was installed
	//   VrNoSignature    owned by nothing - built here, copied in, or unpackaged
	//   VrUnknown        no package database this understands, or no checksum
	//
	int		Verify = 0;
};

//
// The package owning a file, or an invalid entry when nothing does.
//
// Cached by path: a hundred processes of one image ask the same question, and
// the answer only changes when a package is installed.
//
SPackageInfo	LinuxPackageForFile(const QString& Path);

//
// What a sandboxed process is allowed to do.
//
// Read from the sandbox itself rather than from a manifest on disk: a flatpak
// leaves a .flatpak-info in its own root describing the permissions it was
// actually started with, which is the truth for this process rather than the
// default for the application. A snap's confinement mode comes from its
// snap.yaml, which is the only place it is written down.
//
struct SSandboxInfo
{
	bool		Valid = false;

	QString		Kind;			// "flatpak" or "snap"
	QString		AppId;			// "org.mozilla.firefox", "firefox"
	QString		Confinement;	// "strict", "classic", "devmode", "sandboxed"

	//
	// The permissions as the sandbox words them - "filesystem=home",
	// "network", "x11". Values out of a manifest, not sentences of ours: the
	// viewer decides how to show them, and a permission this build has never
	// heard of still arrives intact.
	//
	QStringList	Permissions;
};

SSandboxInfo	LinuxSandboxForProcess(quint64 Pid, const QString& Confinement);

//
// Kernel-side hardening a process has asked for, as flags for the mitigations
// column. Seccomp and no-new-privs are the two that a process either has or
// does not, and both are one line of /proc/<pid>/status.
//
#define LINUX_MITIGATION_SECCOMP		0x0100
#define LINUX_MITIGATION_SECCOMP_STRICT	0x0200
#define LINUX_MITIGATION_NO_NEW_PRIVS	0x0400

quint32			LinuxMitigationFlags(const QMap<QString, QString>& Status);
