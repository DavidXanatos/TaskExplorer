#include "stdafx.h"
#include "LinuxModule.h"
#include "LinuxHelper.h"
#include "LinuxElf.h"

#include <QFileInfo>

CLinuxModule::CLinuxModule(QObject *parent)
	: CModuleInfo(parent)
{
	m_IsExecutable = false;
}

CLinuxModule::~CLinuxModule()
{
}

bool CLinuxModule::InitStaticData(const QString& FileName, quint64 BaseAddress, quint64 Size)
{
	QWriteLocker Locker(&m_Mutex);

	m_FileName = FileName;
	m_ModuleName = QFileInfo(FileName).fileName();
	m_BaseAddress = BaseAddress;
	m_Size = Size;

	QFileInfo Info(FileName);
	if (Info.exists())
	{
		m_FileSize = Info.size();
		m_ModificationTime = Info.lastModified().toMSecsSinceEpoch();

		//
		// ELF has no VERSIONINFO resource, so the three columns the shared GUI
		// expects are assembled from what the format does carry. See LinuxElf.h
		// for where each piece comes from; anything genuinely unavailable is
		// left blank rather than filled with a guess.
		//
		const SElfInfo Elf = ReadElfInfo(FileName);
		if (Elf.Valid)
		{
			m_FileDetails["FileVersion"] = !Elf.PackageVersion.isEmpty()
				? Elf.PackageVersion
				: ElfVersionFromName(FileName, Elf.SoName);

			// Only .note.package names a vendor; there is no other honest source.
			m_FileDetails["CompanyName"] = Elf.PackageOs;

			m_FileDetails["Description"] = !Elf.PackageName.isEmpty()
				? Elf.PackageName
				: LinuxDescribeExecutable(FileName);

			if (!Elf.SoName.isEmpty())
				m_FileDetails["InternalName"] = Elf.SoName;
			if (!Elf.BuildId.isEmpty())
				m_FileDetails["BuildId"] = Elf.BuildId;
		}
	}

	return true;
}

STATUS CLinuxModule::Unload(bool bForce)
{
	// Would require ptrace-injecting a dlclose() into the target.
	return ERR(tr("Unloading a module from a running process is not supported on Linux."));
}
