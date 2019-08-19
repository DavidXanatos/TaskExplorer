#pragma once
#include "../ModuleInfo.h"

class CWinModule : public CModuleInfo
{
	Q_OBJECT
public:
	CWinModule(quint64 ProcessId = -1, bool IsSubsystemProcess = false, QObject *parent = nullptr);
	virtual ~CWinModule();

	void SetSubsystemProcess(bool IsSubsystemProcess)		{ QReadLocker Locker(&m_Mutex); m_IsSubsystemProcess = IsSubsystemProcess; }

	virtual quint64 GetEntryPoint() const 					{ QReadLocker Locker(&m_Mutex); return m_EntryPoint; }

	virtual quint64 GetType() const 						{ QReadLocker Locker(&m_Mutex); return m_Type; }
	virtual QString GetTypeString() const;
	
	virtual quint16 GetLoadCount() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadCount; }
	virtual QDateTime GetLoadTime() const 					{ QReadLocker Locker(&m_Mutex); return m_LoadTime; }

	virtual QDateTime GetImageTimeDateStamp() const 		{ QReadLocker Locker(&m_Mutex); return m_ImageTimeDateStamp; }

	virtual void ClearControlFlowGuardEnabled();

	virtual QString GetASLRString() const;
	virtual QString GetCFGuardString() const;
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
	virtual ulong GetImportFunctions() const 				{ QReadLocker Locker(&m_Mutex); return m_ImportFunctions; }
	virtual ulong GetImportModules() const 					{ QReadLocker Locker(&m_Mutex); return m_ImportModules; }


	void InitAsyncData(const QString& FileName, const QString& PackageFullName = "");

	virtual STATUS				Unload(bool bForce = false);

signals:
	void	AsyncDataDone(bool IsPacked, ulong ImportFunctions, ulong ImportModules);

protected:
	friend class CWindowsAPI;
	friend class CWinProcess;

	bool InitStaticData(struct _PH_MODULE_INFO* module, quint64 ProcessHandle);

	bool UpdateDynamicData(struct _PH_MODULE_INFO* module);


	quint64						m_ProcessId;
	bool						m_IsSubsystemProcess;
    quint64						m_EntryPoint;
    ulong						m_Flags;
    ulong						m_Type;
    quint16						m_LoadReason;
    quint16						m_LoadCount;
	QDateTime					m_LoadTime;

	QDateTime					m_ImageTimeDateStamp;
	quint16						m_ImageCharacteristics;
	quint16						m_ImageDllCharacteristics;

	// Signature
	EVerifyResult				m_VerifyResult;
	QString						m_VerifySignerName;
	
	// Signature, Packed
	bool						m_IsPacked;
	ulong						m_ImportFunctions;
	ulong						m_ImportModules;

protected slots:
	void OnInitAsyncData(int Index);

private:
	static QVariantMap InitAsyncData(QVariantMap Params);
};
