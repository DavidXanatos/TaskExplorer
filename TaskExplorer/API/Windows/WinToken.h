#pragma once
#include <qobject.h>
#include "../../../MiscHelpers/Common/FlexError.h"
#include "../ProcessInfo.h"
#include "../AbstractInfo.h"

#define VIRTUALIZATION_NOT_ALLOWED	0x00
#define VIRTUALIZATION_ALLOWED		0x01
#define VIRTUALIZATION_ENABLED		0x02

#undef GetUserName

class CWinToken : public CAbstractInfo
{
	Q_OBJECT
public:
	CWinToken(QObject *parent = nullptr);
	virtual ~CWinToken();

	static CWinToken* NewSystemToken();
	static CWinToken* TokenFromProcess(void* QueryHandle);
	static CWinToken* TokenFromHandle(quint64 ProcessId, quint64 HandleId);
	static CWinToken* TokenFromThread(quint64 ThreadId);
	static CWinToken* OriginalToken(quint64 ProcessId);
	static CWinToken* OriginalToken(quint64 ProcessId, quint64 ThreadId);

	virtual QString			GetUserName() const { QReadLocker Locker(&m_Mutex); return m_UserName; }
	virtual QString			GetContainerName() const { QReadLocker Locker(&m_Mutex); return m_ContainerName; }
	virtual QByteArray		GetUserSid(bool bReal = false) const { QReadLocker Locker(&m_Mutex); if(bReal && m_IsAppContainer == 2) return m_OwnerSid; return m_UserSid; }
	virtual QString			GetSidString() const { QReadLocker Locker(&m_Mutex); return m_SidString; }
	virtual bool			IsAppContainer() const { QReadLocker Locker(&m_Mutex); return m_IsAppContainer != 0; }
	virtual quint32			GetSessionId() const { QReadLocker Locker(&m_Mutex); return m_SessionId; }
	virtual QString			GetOwnerName() const { QReadLocker Locker(&m_Mutex); return m_OwnerName; }
	virtual QByteArray		GetOwnerSid() const { QReadLocker Locker(&m_Mutex); return m_OwnerSid; }
	virtual QString			GetGroupName() const { QReadLocker Locker(&m_Mutex); return m_GroupName; }
	virtual QByteArray		GetGroupSid() const { QReadLocker Locker(&m_Mutex); return m_GroupSid; }

	virtual bool			IsElevated() const { QReadLocker Locker(&m_Mutex); return m_Elevated; }
	virtual int				GetElevationType() const { QReadLocker Locker(&m_Mutex); return m_ElevationType; }
	virtual quint32			GetIntegrityLevel() const { QReadLocker Locker(&m_Mutex); return m_IntegrityLevel; }
	virtual STATUS			SetIntegrityLevel(quint32 IntegrityLevel) const;
	virtual QString			GetIntegrityString() const { QReadLocker Locker(&m_Mutex); return m_IntegrityString; }
	virtual int				GetVirtualization() const { QReadLocker Locker(&m_Mutex); return m_Virtualization; }


	struct SGroup
	{
		QByteArray Sid;
		//QString SidString;
		bool Restricted;
		QString Name;
		quint32 Attributes;
	};
	static QString			GetGroupStatusString(quint32 Attributes, bool Restricted = false);
	static QString			GetGroupDescription(quint32 Attributes);
	static bool				IsGroupEnabled(quint32 Attributes);
	static bool				IsGroupModified(quint32 Attributes);

	struct SPrivilege
	{
		QString Name;
		quint32 Attributes;
		quint32 lLuid;
		qint32 hLuid;
		QString Description;
	};
	static QString			GetPrivilegeAttributesString(quint32 Attributes);
	static bool				IsPrivilegeEnabled(quint32 Attributes);
	static bool				IsPrivilegeModified(quint32 Attributes);

	virtual QMap<QByteArray, SGroup> GetGroups() const { QReadLocker Locker(&m_Mutex); return m_Groups; }
	virtual QMap<QString, SPrivilege> GetPrivileges() const { QReadLocker Locker(&m_Mutex); return m_Privileges; }

	virtual QString			GetElevationString() const;
	virtual QString			GetVirtualizationString() const;

	virtual bool			IsVirtualizationAllowed() const { QReadLocker Locker(&m_Mutex); return (m_Virtualization & VIRTUALIZATION_ALLOWED) != 0; }
	virtual bool			IsVirtualizationEnabled() const { QReadLocker Locker(&m_Mutex); return (m_Virtualization & VIRTUALIZATION_ENABLED) != 0; }
	virtual STATUS			SetVirtualizationEnabled(bool bSet);

	enum EAction
	{
		eEnable,
		eDisable,
		eReset,
		eRemove
	};

	virtual STATUS			PrivilegeAction(const SPrivilege& Privilege, EAction Action, bool bForce = false);
	virtual STATUS			GroupAction(const SGroup& Group, EAction Action);

	virtual void OpenPermissions(bool bDefaultToken = false);

	virtual QSharedPointer<CWinToken> GetLinkedToken();

	struct SAdvancedInfo
	{
		SAdvancedInfo()
		{
			tokenLuid = 0;
			authenticationLuid = 0;
			tokenModifiedLuid = 0;

			memoryUsed = 0;
			memoryAvailable = 0;

			tokenOriginLogonSession = 0;
		}

		QString sourceName;
		QString sourceLuid;

		QString tokenType;
		QString tokenImpersonationLevel;

		quint32 tokenLuid;
		quint32 authenticationLuid;
		quint32 tokenModifiedLuid;

		quint64 memoryUsed;
		quint64 memoryAvailable;

		quint32 tokenOriginLogonSession;

		QString tokenNamedObjectPath;
		QString tokenSecurityDescriptor;

		QString tokenTrustLevelSid;
		QString tokenTrustLevelName;

		QString tokenLogonName;
		QString tokenLogonSid;

		QString tokenProfilePath;
		QString tokenProfileRegistry;
	};

	virtual SAdvancedInfo GetAdvancedInfo();

	struct SContainerInfo
	{
		SContainerInfo()
		{
			appContainerNumber = 0;
			isLessPrivilegedAppContainer = false;
		}

		QString appContainerName;
		QString appContainerSid;
		QString appContainerSidType;

		quint32 appContainerNumber;
		bool isLessPrivilegedAppContainer;
		QString tokenNamedObjectPath;

		QString parentContainerName;
		QString parentContainerSid;


		QString packageFullName;
		QString packagePath;


		QString appContainerFolderPath;
		QString appContainerRegistryPath;
	};

	virtual SContainerInfo GetContainerInfo();


	struct SCapability
	{
		QString SidString;

		QString FullName;
		QString Capability;

		QString Package;
		QString Guid;
	};

	virtual QMap<QByteArray, SCapability> GetCapabilities();

	struct SAttribute
	{
		quint16 Type;
		quint32 Flags;
		QVector<QVariant> Values;
	};

	virtual QMap<QString, SAttribute> GetClaims(bool DeviceClaims);

	virtual QMap<QString, SAttribute> GetAttributes();

	enum EDangerousFlags
	{
		eNoWriteUpDisabled,
		eSandboxInertEnabled,
		eUIAccessEnabled,
	};

	virtual QSet<EDangerousFlags> GetDangerousFlags() const { QReadLocker Locker(&m_Mutex); return m_DangerousFlags; }

    static QString GetSecurityAttributeTypeString(quint16 Type);
    static QString GetSecurityAttributeFlagsString(quint32 Flags);

	enum EQueryType
	{
		eProcess = 0,
		eLinked,
		eOriginalPrimary,
		eOriginalThread,
		eHandle,
		eThread
	};

public slots:
	virtual void OnSidResolved(const QByteArray& SID, const QString& Name);

protected:
	friend class CWinProcess;
	friend class CTokenView;

	bool InitStaticData();
	bool UpdateDynamicData(bool MonitorChange = true, bool IsOrWasRunning = false);
	bool UpdateExtendedData();

	QString		m_UserName;
	QString		m_ContainerName;
	QByteArray	m_UserSid;
	QString		m_SidString;
	int			m_IsAppContainer;
	QString		m_OwnerName;
	QByteArray	m_OwnerSid;
	QString		m_GroupName;
	QByteArray	m_GroupSid;
	quint32		m_SessionId;

	bool		m_Elevated;
	int			m_ElevationType;
	quint32		m_IntegrityLevel;
	QString		m_IntegrityString;
	int			m_Virtualization;

	enum ETokenState
	{
		eNotInitialized = 0,
		eNotYetLocked,
		eInitialized,
		eHasChanged
	}			m_TokenState;

	QMap<QByteArray, SGroup> m_Groups;
	QMap<QString, SPrivilege> m_Privileges;
	QSet<EDangerousFlags> m_DangerousFlags;
	
private:
	void SetDangerousFlag(EDangerousFlags Flag, bool Set);

	struct SWinToken*		m;
};


typedef QSharedPointer<CWinToken> CWinTokenPtr;
