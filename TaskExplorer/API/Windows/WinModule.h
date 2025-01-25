#pragma once
#include "../ModuleInfo.h"

class CWinModule : public CModuleInfo
{
	Q_OBJECT
public:
	CWinModule(quint64 ProcessId = -1, bool IsSubsystemProcess = false, QObject *parent = nullptr);
	virtual ~CWinModule();

	virtual QString GetFileNameNt() const					{ QReadLocker Locker(&m_Mutex); return m_FileNameNt; }

	virtual quint64 GetEntryPoint() const 					{ QReadLocker Locker(&m_Mutex); return m_EntryPoint; }
	virtual QStringList GetRefServices()const				{ QReadLocker Locker(&m_Mutex); return m_Services; }

	virtual quint64 GetType() const 						{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	
	virtual quint16 GetLoadCount() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadCount; }
	virtual quint64 GetLoadTime() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadTime; }

	virtual quint32 GetEnclaveType() const					{ QReadLocker Locker(&m_Mutex); return m_EnclaveType; }
	virtual QString GetEnclaveTypeString() const;
	virtual quint64 GetEnclaveBaseAddress() const			{ QReadLocker Locker(&m_Mutex); return m_EnclaveBaseAddress; }
	virtual quint64 GetEnclaveSize() const					{ QReadLocker Locker(&m_Mutex); return m_EnclaveSize; }

	virtual quint64 GetTimeStamp() const 					{ QReadLocker Locker(&m_Mutex); return m_ImageTimeDateStamp; }
	virtual quint16 GetImageCharacteristics() const			{ QReadLocker Locker(&m_Mutex); return m_ImageCharacteristics; }
	virtual quint16 GetImageDllCharacteristics() const		{ QReadLocker Locker(&m_Mutex); return m_ImageDllCharacteristics; }
	virtual quint16 GetImageDllCharacteristicsEx() const	{ QReadLocker Locker(&m_Mutex); return m_ImageDllCharacteristicsEx; }

	virtual quint16 GetImageMachine() const					{ QReadLocker Locker(&m_Mutex); return m_ImageMachine; }
	virtual QString GetImageMachineString() const;

	virtual void ClearControlFlowGuardEnabled();
	virtual void SetCetEnabled();
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

	virtual float GetImageCoherency() const					{ QReadLocker Locker(&m_Mutex); return m_ImageCoherency; }
	virtual QString GetImageCoherencyString() const;

	struct SModPage
	{
		quint64		VirtualAddress = 0;
		QString		Name;
	};

	virtual void SetModifiedPage(quint64 VirtualAddress);
	virtual QMap<quint64, SModPage> GetModifiedPages() const			{ QReadLocker Locker(&m_Mutex); return m_ModifiedPages; }
	virtual void ClearModifiedPages()									{ QWriteLocker Locker(&m_Mutex); m_ModifiedPages.clear(); }

	void InitAsyncData(const QString& PackageFullName = "");

	virtual STATUS				Unload(bool bForce = false);

signals:
	void	AsyncDataDone(bool IsPacked, quint32 ImportFunctions, quint32 ImportModules);

private slots:
	void OnSymbolFromAddress(quint64 ProcessId, quint64 Address, int ResolveLevel, const QString& StartAddressString, const QString& FileName, const QString& SymbolName);

protected:
	friend class CWinProcess;
	friend class CWinDriver;
	friend class CWinService;
		
	bool InitStaticData(struct _PH_MODULE_INFO* module, quint64 ProcessHandle);
	bool InitStaticData(const QVariantMap& Module);
	bool InitStaticData(const QString& FileName);
	void InitFileInfo();
	bool ResolveRefServices();

	bool UpdateDynamicData(struct _PH_MODULE_INFO* module);

	QString						m_FileNameNt;

	quint64						m_ProcessId;
	bool						m_IsSubsystemProcess;
    quint64						m_EntryPoint;
	QStringList					m_Services;
    quint32						m_Flags;
    quint32						m_Type;
    quint16						m_LoadReason;
    quint16						m_LoadCount;
	quint64						m_LoadTime;

	quint32						m_EnclaveType;
	quint64						m_EnclaveBaseAddress;
	quint64						m_EnclaveSize;

	union
	{
		quint8					m_StateFlags;
		struct
		{
			quint8				m_JustProcessed : 1;
			quint8				m_IsFirst : 1;
			quint8				m_ImageNotAtBase : 1;
			quint8				m_ImageKnownDll : 1;
			quint8				m_Spare : 4;
		};
	};

	quint16						m_ImageMachine;
	quint32						m_ImageCHPEVersion; // todo
	quint64						m_ImageTimeDateStamp;
	quint16						m_ImageCharacteristics;
	quint16						m_ImageDllCharacteristics;
	quint32						m_ImageDllCharacteristicsEx;
	quint32						m_ImageFlags;
	quint32						m_GuardFlags;
	qint32						m_ImageCoherencyStatus;
	float						m_ImageCoherency;

	// Signature
	EVerifyResult				m_VerifyResult;
	QString						m_VerifySignerName;
	
	// Signature, Packed
	bool						m_IsPacked;
	quint32						m_ImportFunctions;
	quint32						m_ImportModules;

	QMap<quint64, SModPage>		m_ModifiedPages;

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
	
	bool InitStaticData(quint64 ProcessId, quint64 ProcessHandle, const QString& FileName, const QString& FileNameNt, bool IsSubsystemProcess, bool IsWow64);

    quint16						m_ImageSubsystem;
	quint64						m_PebBaseAddress;
	quint64						m_PebBaseAddress32;
};