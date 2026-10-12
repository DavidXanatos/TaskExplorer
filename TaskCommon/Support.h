#pragma once

//
// The supporter certificate, and what it unlocks.
//
// Ported from the MajorPrivacy driver's Support.cpp, which is where the format
// is defined and where certificates for the whole family of programs are
// checked. The format is not ours to change: the same issuer signs them, the
// same fields mean the same things, and a certificate that a driver accepts has
// to be accepted here for the same reasons. So the parsing, the date
// arithmetic and the type and level tables below follow that file closely
// enough that a difference between them is a bug rather than a decision.
//
// What is different is where it runs. The original is kernel mode: pool
// allocations, RtlTimeFieldsToTime, a file read through a driver's own IO
// helper. This is a library used by a service, a module and a viewer, so it is
// QString, QDateTime and QFile, and the one thing that genuinely needs a
// platform is the signature check - see VerifySignature in the .cpp.
//
// ---- what it gates ----
//
// TaskServer and TaskRemote, and nothing else. The viewer watching its own
// machine is the whole program and needs no certificate; what needs one is the
// half that serves a machine to somebody else and the half that reaches one.
// See the comments at those two call sites for why the answer differs there -
// a service refuses to start, a module refuses to be created.
//

#include <QString>
#include <QByteArray>
#include <QDateTime>

//
// The name this program answers to in a certificate's SOFTWARE line.
//
// A certificate must name it. One issued for something else is refused rather
// than ignored: the SOFTWARE tag is inside the signed material, so accepting a
// certificate that names another program would accept a signature over a
// statement about that program. It is also what is asked *for* when there is no
// serial to ask with - see TaskServer's evaluation certificate command - which
// is why it is here rather than private to the file that checks it.
//
#define MY_SOFTWARE_NAME	"TaskExplorer"

//
// The state, as one word plus an expiry.
//
// A bitfield rather than a struct of bools because that is what the issuer's
// scheme numbers: type and level are small integers with their own meanings and
// the flags around them are what the checks below produce. Keeping the layout
// means a certificate can be read here and its state compared with what another
// program in the family made of the same file.
//
union SCertInfo
{
	unsigned long long State;
	struct
	{
		unsigned long
			active		: 1,	// valid, and its features are on
			expired		: 1,	// past its date, may still be active
			outdated	: 1,	// past its date for *this build*, not for all time
			reservd_1	: 2,
			grace_period: 1,	// expired or outdated, kept alive one more month
			locked		: 1,	// bound to one machine
			lock_req	: 1,

			type		: 5,	// ECertType
			level		: 3,	// ECertLevel

			reservd_3	: 8,
			reservd_4	: 4,
			opt_desk	: 1,
			opt_net		: 1,
			opt_enc		: 1,
			opt_sec		: 1;

		//
		// Seconds until it expires, negative once it has. Signed on purpose:
		// "expired 3 days ago" is as much worth saying as "expires in 3 days".
		//
		long expirers_in_sec;
	};
};

enum ECertType
{
	eCertNoType			= 0x00,

	eCertEternal		= 0x04,
	eCertContributor	= 0x05,

	eCertBusiness		= 0x08,

	eCertPersonal		= 0x0C,

	eCertHome			= 0x10,
	eCertFamily			= 0x11,

	eCertDeveloper		= 0x14,

	eCertPatreon		= 0x18,
	eCertGreatPatreon	= 0x19,
	eCertEntryPatreon	= 0x1A,

	eCertEvaluation		= 0x1C
};

enum ECertLevel
{
	eCertNoLevel		= 0,
	eCertStandard		= 2,
	eCertStandard2		= 3,
	eCertAdvanced1		= 4,
	eCertAdvanced		= 5,
	eCertMaxLevel		= 7,
};

//
// The type is a class in its top three bits and a variant in the bottom two, so
// "is this a Patreon certificate" is a masked compare rather than a list.
//
#define CERT_IS_TYPE(cert,t)		(((cert).type & 0x1C) == (unsigned long)(t))
#define CERT_IS_SUBSCRIPTION(cert)	(CERT_IS_TYPE(cert, eCertBusiness) || CERT_IS_TYPE(cert, eCertHome) \
									 || (cert).type == eCertEntryPatreon || CERT_IS_TYPE(cert, eCertEvaluation))
#define CERT_IS_INSIDER(cert)		(CERT_IS_TYPE(cert, eCertEternal) || (cert).type == eCertGreatPatreon)
#define CERT_IS_LEVEL(cert,l)		((cert).active && (cert).level >= (unsigned long)(l))

//
// Why a certificate was refused.
//
// Separate from the flags because a certificate that is absent, one that is
// forged and one that ran out are three different things to say to somebody,
// and only the third of them is their own doing.
//
enum ECertStatus
{
	eCertOk = 0,
	eCertNotFound,			// no Certificate.dat where one was looked for
	eCertUnreadable,		// there is a file, but it could not be read
	eCertMalformed,			// it is not a certificate, or a line is not a tag
	eCertNoSignature,		// no SIGNATURE line
	eCertBadSignature,		// the signature does not match the contents
	eCertWrongSoftware,		// signed, but issued for another program
	eCertWrongMachine,		// signed and node-locked, to another machine
	eCertExpired,			// signed and ours, but past its life
	eCertNoCrypto,			// this build cannot check a signature at all
};


//
// Where Certificate.dat lives: beside the running executable.
//
// Beside the *binary* and not in a per-user place, because it is one
// machine's certificate and the service reads the same file the viewer
// wrote. That is also why writing it needs administrator rights and the
// viewer has to ask for them.
//
QString		CertFilePath();

//
// This machine's hardware id, in the form the issuer node-locks against.
//
// The firmware UUID: the SMBIOS system UUID on Windows, the same value
// under /sys/class/dmi/id on Linux. Empty when there is none to read, which
// is not an error - it only means a node-locked certificate cannot be
// matched here.
//
QString		CertSystemHwid();

//
// Read, parse and verify. Path empty means FilePath().
//
// Every failure fills pStatus and leaves the returned state cleared, so a
// caller that only wants "may I run" can ignore the rest.
//
SCertInfo	CertValidate(const QString& Path = QString(), ECertStatus* pStatus = nullptr,
						QString* pName = nullptr);

//
// The answer, remembered.
//
// Read once and kept, because the two callers that gate on it are a service
// deciding whether to start and a module deciding whether to load, and
// neither should touch the disk on every request. bReload re-reads the file
// - which is what the viewer does after writing a new one.
//
SCertInfo	CertState(bool bReload = false);
ECertStatus	CertStatus(bool bReload = false);
QString		CertName();

//
// True when there is a certificate that is currently valid. The one
// question TaskServer and TaskRemote ask.
//
bool		CertIsValid(bool bReload = false);

//
// ---- what is deliberately not here ----
//
// No wording. Turning a status or a type into a sentence is the business of
// whoever shows it, and the two who do are a service writing one line to a
// log and a settings page with room to explain - see TaskServer/main.cpp
// and GUI/SettingsWindow.cpp. Keeping the words here would have made this
// file the place a translator had to look for text used by a program that
// has no user interface.
//
