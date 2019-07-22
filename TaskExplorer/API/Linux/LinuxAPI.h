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


protected:

};
