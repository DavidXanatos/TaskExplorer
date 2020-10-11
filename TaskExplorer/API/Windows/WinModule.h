#pragma once
#include "../ModuleInfo.h"

class CWinModule : public CModuleInfo
{
	Q_OBJECT
public:
	CWinModule(quint64 ProcessId = -1, bool IsSubsystemProcess = false, QObject *parent = nullptr);
	virtual ~CWinModule();

	virtual quint64 GetEntryPoint() const 					{ QReadLocker Locker(&m_Mutex); return m_EntryPoint; }
	virtual QStringList GetRefServices()const				{ QReadLocker Locker(&m_Mutex); return m_Services; }

	virtual quint64 GetType() const 						{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	
	virtual quint16 GetLoadCount() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadCount; }
	virtual quint64 GetLoadTime() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadTime; }

	virtual quint64 GetTimeStamp() const 					{ QReadLocker Locker(&m_Mutex); return m_ImageTimeStamp; }

	virtual void ClearControlFlowGuardEnabled();
	virtual void ClearCetEnabled();

	virtual QString GetMitigationsString() const;

	virtual QString GetLoadReasonString() const;

	enum EVerifyResult
	{
		VrUnknown = 0,
		VrNoSignature,
		VrTrusted,
		VrExpired,
		VrRevoked,
		VrDistrust,
		VrSecuritySettings,
		VrBadSignature
	};

	virtual EVerifyResult GetVerifyResult() const 			{ QReadLocker Locker(&m_Mutex); return m_VerifyResult; }
	virtual QString GetVerifyResultString() const;
	virtual QString GetVerifySignerName() const 			{ QReadLocker Locker(&m_Mutex); return m_VerifySignerName; }

	virtual bool IsPacked() const 							{ QReadLocker Locker(&m_Mutex); return m_IsPacked; }
	virtual quint32 GetImportFunctions() const 				{ QReadLocker Locker(&m_Mutex); return m_ImportFunctions; }
	virtual quint32 GetImportModules() const 				{ QReadLocker Locker(&m_Mutex); return m_ImportModules; }


	void InitAsyncData(const QString& PackageFullName = "");

	virtual STATUS				Unload(bool bForce = false);

signals:
	void	AsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules);

protected:
	friend class CWinProcess;
	friend class CWinDriver;
		
	bool InitStaticData(struct _PH_MODULE_INFO* module, quint64 ProcessHandle);
	bool InitStaticData(const QVariantMap& Module);
	bool InitStaticData(const QString& FileName);
	void InitFileInfo();
	bool ResolveRefServices();

	bool UpdateDynamicData(struct _PH_MODULE_INFO* module);


	quint64						m_ProcessId;
	bool						m_IsSubsystemProcess;
    quint64						m_EntryPoint;
	QStringList					m_Services;
    quint32						m_Flags;
    quint32						m_Type;
    quint16						m_LoadReason;
    quint16						m_LoadCount;
	quint64						m_LoadTime;

	quint64						m_ImageTimeStamp;
	quint16						m_ImageCharacteristics;
	quint16						m_ImageDllCharacteristics;
	quint16						m_ImageDllCharacteristicsEx;

	// Signature
	EVerifyResult				m_VerifyResult;
	QString						m_VerifySignerName;
	
	// Signature, Packed
	bool						m_IsPacked;
	quint32						m_ImportFunctions;
	quint32						m_ImportModules;

protected slots:
	void OnInitAsyncData(int Index);

private:
	static QVariantMap InitAsyncData(QVariantMap Params);
};

//////////////////////////////////////////////////////////////////////////////////////////////////////
// CWinMainModule 

class CWinMainModule : public CWinModule
{
	Q_OBJECT
public:
	CWinMainModule(QObject *parent = nullptr);
	virtual ~CWinMainModule() {}
	
	virtual quint16 GetImageSubsystem() const 				{ QReadLocker Locker(&m_Mutex); return m_ImageSubsystem; }
	virtual quint64 GetPebBaseAddress(bool bWow64 = false) const;


protected:
	friend class CWinProcess;
	
	bool InitStaticData(quint64 ProcessId, const QString& FileName, bool IsSubsystemProcess, bool IsWow64);

    quint16						m_ImageSubsystem;
	quint64						m_PebBaseAddress;
	quint64						m_PebBaseAddress32;
};