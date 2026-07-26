#include "stdafx.h"
#include "LinuxElf.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

#include <elf.h>
#include <string.h>

#ifndef NT_GNU_BUILD_ID
#define NT_GNU_BUILD_ID	3
#endif

// systemd's "Package Metadata for Core Files" note: owner "FDO", this type,
// descriptor is a NUL terminated JSON object.
#define NT_FDO_PACKAGING_METADATA	0xcafe1a7e

namespace {

//
// Sanity limits. These files are attacker-controlled in the sense that any
// process can map any file, so every count and length out of the header is
// treated as a hint rather than a fact.
//
const quint32 MaxProgramHeaders	= 4096;
const quint64 MaxStringTable	= 4 * 1024 * 1024;
const quint64 MaxNoteSegment	= 1 * 1024 * 1024;

QString ExtractString(const QByteArray& Table, quint64 Offset)
{
	if (Offset >= (quint64)Table.size())
		return QString();

	// The table is a run of NUL terminated strings; stop at the first one.
	const char* Start = Table.constData() + Offset;
	const quint64 Available = (quint64)Table.size() - Offset;
	const quint64 Length = qstrnlen(Start, Available);

	return QString::fromUtf8(Start, (int)Length);
}

void ParseNotes(const QByteArray& Segment, SElfInfo& Info)
{
	int Pos = 0;
	while (Pos + (int)sizeof(Elf64_Nhdr) <= Segment.size())
	{
		// Elf32_Nhdr and Elf64_Nhdr are both three 32-bit words, so one shape
		// covers either class.
		Elf32_Nhdr Nhdr;
		memcpy(&Nhdr, Segment.constData() + Pos, sizeof(Nhdr));
		Pos += sizeof(Nhdr);

		const int NameSize = (int)((Nhdr.n_namesz + 3) & ~3u);
		const int DescSize = (int)((Nhdr.n_descsz + 3) & ~3u);

		if (NameSize < 0 || DescSize < 0 || Pos + NameSize + DescSize > Segment.size())
			break;

		const QByteArray Name(Segment.constData() + Pos, (int)qMin<quint32>(Nhdr.n_namesz ? Nhdr.n_namesz - 1 : 0, (quint32)NameSize));
		const QByteArray Desc(Segment.constData() + Pos + NameSize, (int)Nhdr.n_descsz);

		if (Name == "GNU" && Nhdr.n_type == NT_GNU_BUILD_ID)
		{
			Info.BuildId = QString::fromLatin1(Desc.toHex());
		}
		else if (Name == "FDO" && Nhdr.n_type == NT_FDO_PACKAGING_METADATA)
		{
			//
			// The descriptor is NUL terminated JSON; the trailing NUL and any
			// alignment padding have to come off before parsing.
			//
			const QJsonObject Package = QJsonDocument::fromJson(
				QByteArray(Desc.constData(), (int)qstrnlen(Desc.constData(), Desc.size()))).object();

			Info.PackageName = Package.value("name").toString();
			Info.PackageVersion = Package.value("version").toString();
			Info.PackageOs = Package.value("os").toString();
		}

		Pos += NameSize + DescSize;
	}
}

//
// The parse itself, written once against whichever set of ELF types the file
// turns out to use.
//
template<typename TEhdr, typename TPhdr, typename TDyn>
bool ParseElf(QFile& File, SElfInfo& Info)
{
	TEhdr Ehdr;
	File.seek(0);
	if (File.read((char*)&Ehdr, sizeof(Ehdr)) != (qint64)sizeof(Ehdr))
		return false;

	Info.Type = Ehdr.e_type;
	Info.Machine = Ehdr.e_machine;

	if (Ehdr.e_phnum == 0 || Ehdr.e_phnum > MaxProgramHeaders || Ehdr.e_phentsize < sizeof(TPhdr))
		return false;

	QVector<TPhdr> Phdrs(Ehdr.e_phnum);
	for (int i = 0; i < (int)Ehdr.e_phnum; i++)
	{
		if (!File.seek((qint64)Ehdr.e_phoff + (qint64)i * Ehdr.e_phentsize))
			return false;
		if (File.read((char*)&Phdrs[i], sizeof(TPhdr)) != (qint64)sizeof(TPhdr))
			return false;
	}

	//
	// Addresses inside .dynamic are virtual, so they have to be translated back
	// into file offsets through the PT_LOAD segments that map them.
	//
	auto ToFileOffset = [&Phdrs](quint64 VAddr, quint64* pOffset) -> bool
	{
		foreach(const TPhdr& Phdr, Phdrs)
		{
			if (Phdr.p_type != PT_LOAD)
				continue;
			if (VAddr >= Phdr.p_vaddr && VAddr < Phdr.p_vaddr + Phdr.p_filesz)
			{
				*pOffset = Phdr.p_offset + (VAddr - Phdr.p_vaddr);
				return true;
			}
		}
		return false;
	};

	quint64 StrTabAddr = 0;
	quint64 StrTabSize = 0;
	quint64 SoNameOffset = 0;
	bool bHaveSoName = false;
	QList<quint64> NeededOffsets;

	foreach(const TPhdr& Phdr, Phdrs)
	{
		if (Phdr.p_type == PT_INTERP)
		{
			if (Phdr.p_filesz > 0 && Phdr.p_filesz < 4096 && File.seek(Phdr.p_offset))
			{
				const QByteArray Interp = File.read(Phdr.p_filesz);
				Info.Interpreter = QString::fromUtf8(Interp.constData(), (int)qstrnlen(Interp.constData(), Interp.size()));
			}
		}
		else if (Phdr.p_type == PT_NOTE)
		{
			if (Phdr.p_filesz > 0 && Phdr.p_filesz <= MaxNoteSegment && File.seek(Phdr.p_offset))
				ParseNotes(File.read(Phdr.p_filesz), Info);
		}
		else if (Phdr.p_type == PT_DYNAMIC)
		{
			if (!File.seek(Phdr.p_offset))
				continue;

			const quint64 Count = Phdr.p_filesz / sizeof(TDyn);
			for (quint64 i = 0; i < Count; i++)
			{
				TDyn Dyn;
				if (File.read((char*)&Dyn, sizeof(Dyn)) != (qint64)sizeof(Dyn))
					break;
				if (Dyn.d_tag == DT_NULL)
					break;

				switch (Dyn.d_tag)
				{
					case DT_STRTAB:	StrTabAddr = Dyn.d_un.d_ptr; break;
					case DT_STRSZ:	StrTabSize = Dyn.d_un.d_val; break;
					case DT_SONAME:	SoNameOffset = Dyn.d_un.d_val; bHaveSoName = true; break;
					case DT_NEEDED:	NeededOffsets.append(Dyn.d_un.d_val); break;
					default: break;
				}
			}
		}
	}

	//
	// The names live in the dynamic string table, which is only worth reading
	// once something actually referenced it.
	//
	if (StrTabAddr && StrTabSize && StrTabSize <= MaxStringTable && (bHaveSoName || !NeededOffsets.isEmpty()))
	{
		quint64 StrTabOffset = 0;
		if (ToFileOffset(StrTabAddr, &StrTabOffset) && File.seek(StrTabOffset))
		{
			const QByteArray StrTab = File.read(StrTabSize);

			if (bHaveSoName)
				Info.SoName = ExtractString(StrTab, SoNameOffset);

			foreach(quint64 Offset, NeededOffsets)
			{
				const QString Name = ExtractString(StrTab, Offset);
				if (!Name.isEmpty())
					Info.Needed.append(Name);
			}
		}
	}

	//
	// A position independent executable and a shared library are both ET_DYN,
	// so they have to be told apart by what else is present: an executable has
	// a program interpreter and no SONAME.
	//
	// The interpreter alone is not enough. Some libraries are also runnable and
	// carry one - glibc is the well known case, since "/lib/.../libc.so.6"
	// prints its version when executed - but they still declare a SONAME, which
	// no executable does.
	//
	Info.IsPie = (Ehdr.e_type == ET_DYN) && !Info.Interpreter.isEmpty() && Info.SoName.isEmpty();

	Info.Valid = true;
	return true;
}

} // namespace

SElfInfo ReadElfInfo(const QString& Path)
{
	SElfInfo Info;

	QFile File(Path);
	if (!File.open(QIODevice::ReadOnly))
		return Info;

	unsigned char Ident[EI_NIDENT];
	if (File.read((char*)Ident, EI_NIDENT) != EI_NIDENT)
		return Info;

	if (memcmp(Ident, ELFMAG, SELFMAG) != 0)
		return Info;

	//
	// Only native-endian files are parsed. A cross-endian ELF would need every
	// field byte-swapped, and one cannot be mapped by a local process anyway,
	// so the effort would buy nothing.
	//
	const unsigned char Data = Ident[EI_DATA];
	const bool bNativeEndian = (Q_BYTE_ORDER == Q_LITTLE_ENDIAN) ? (Data == ELFDATA2LSB) : (Data == ELFDATA2MSB);
	if (!bNativeEndian)
		return Info;

	if (Ident[EI_CLASS] == ELFCLASS64)
	{
		Info.Is64Bit = true;
		ParseElf<Elf64_Ehdr, Elf64_Phdr, Elf64_Dyn>(File, Info);
	}
	else if (Ident[EI_CLASS] == ELFCLASS32)
	{
		Info.Is64Bit = false;
		ParseElf<Elf32_Ehdr, Elf32_Phdr, Elf32_Dyn>(File, Info);
	}

	return Info;
}

QString ElfVersionFromName(const QString& Path, const QString& SoName)
{
	//
	// Everything after ".so." is the version, by long standing convention:
	// "libc.so.6" -> "6", "libstdc++.so.6.0.33" -> "6.0.33".
	//
	static const QRegularExpression Version("\\.so\\.([0-9][0-9.]*)$");

	const QRegularExpressionMatch SoMatch = Version.match(SoName);
	if (SoMatch.hasMatch())
	{
		//
		// The SONAME usually carries only the ABI major ("libstdc++.so.6") while
		// the file it resolves to carries the full version. Prefer the longer
		// one when the resolved name agrees with the SONAME so far.
		//
		const QString Short = SoMatch.captured(1);

		const QString Resolved = QFileInfo(Path).canonicalFilePath();
		const QRegularExpressionMatch FileMatch = Version.match(Resolved);
		if (FileMatch.hasMatch())
		{
			const QString Long = FileMatch.captured(1);
			if (Long.startsWith(Short) && Long.length() > Short.length())
				return Long;
		}

		return Short;
	}

	const QRegularExpressionMatch FileMatch = Version.match(QFileInfo(Path).canonicalFilePath());
	if (FileMatch.hasMatch())
		return FileMatch.captured(1);

	return QString();
}
