#pragma once
#include "../SystemAPI.h"

class CLinuxAPI : public CSystemAPI
{
	Q_OBJECT

public:
    CLinuxAPI(QObject *parent = nullptr);
    virtual ~CLinuxAPI();

	virtual bool Init();

	virtual bool RootAvaiable();

	virtual bool UpdateSysStats();

	virtual bool UpdateProcessList();

	virtual bool UpdateSocketList();

	virtual bool UpdateOpenFileList();

	virtual bool UpdateServiceList(bool bRefresh = false);

	virtual bool UpdateDriverList();

    virtual quint64 GetUpTime() const {return 0;}
    virtual QList<SUser> GetUsers() const {return QList<SUser>();}
    virtual QMultiMap<QString, CDnsCacheEntryPtr> GetDnsEntryList() const {return QMultiMap<QString, CDnsCacheEntryPtr>();}

    virtual bool UpdateDnsCache() {return false;}
    virtual void FlushDnsCache() {}
    virtual void OnHardwareChanged() {}

protected:

};

#define CPU_TIME_DIVIDER (10 * 1000 * 1000) // the clock resolution is 100ns we need 1sec
