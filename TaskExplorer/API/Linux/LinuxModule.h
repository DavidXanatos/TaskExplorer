#pragma once
#include "../ModuleInfo.h"

//
// A mapped shared object, coalesced from the file-backed entries of
// /proc/<pid>/maps (one .so contributes several segments; they collapse into a
// single module keyed by its lowest base address).
//
class CLinuxModule : public CModuleInfo
{
	Q_OBJECT

	TRACK_OBJECT(CLinuxModule)
public:
	CLinuxModule(QObject *parent = nullptr);
	virtual ~CLinuxModule();

	virtual bool			InitStaticData(const QString& FileName, quint64 BaseAddress, quint64 Size);

	virtual STATUS			Unload(bool bForce = false);

	virtual bool			IsExecutable() const	{ QReadLocker Locker(&m_Mutex); return m_IsExecutable; }

	//
	// What the package manager makes of this file.
	//
	// An ELF binary carries no signature, so the question Windows answers with
	// WinVerifyTrust is answered here by whoever installed the file: it is
	// trusted when it still matches the checksum its package recorded, altered
	// when it does not, and unsigned when no package claims it at all. The
	// scale is CModuleInfo's own - see the note there, which anticipated
	// exactly this.
	//
	virtual EVerifyResult	GetVerifyResult() const		{ QReadLocker Locker(&m_Mutex); return m_VerifyResult; }
	virtual QString			GetVerifySignerName() const	{ QReadLocker Locker(&m_Mutex); return m_VerifySigner; }

	void					SetPackageVerification(EVerifyResult Result, const QString& Signer)
							{ QWriteLocker Locker(&m_Mutex); m_VerifyResult = Result; m_VerifySigner = Signer; }

protected:
	bool					m_IsExecutable;

	EVerifyResult			m_VerifyResult = VrUnknown;
	QString					m_VerifySigner;
};

typedef QSharedPointer<CLinuxModule> CLinuxModulePtr;
typedef QWeakPointer<CLinuxModule> CLinuxModuleRef;
