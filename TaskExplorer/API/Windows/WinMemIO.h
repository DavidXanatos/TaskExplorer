#pragma once

#include "WinMemory.h"

class CWinMemIO : public QIODevice
{
	Q_OBJECT
public:
	CWinMemIO(CWinMemory* pMemory, QObject* parent = NULL);
	CWinMemIO(quint64 BaseAddress, quint64 RegionSize, quint64 ProcessId, bool bUnMap = false, QObject* parent = NULL);
	virtual ~CWinMemIO();

	static CWinMemIO* FromHandle(quint64 ProcessId, quint64 HandleId);

	virtual quint64 GetBaseAddress();
	virtual quint64 GetRegionSize();

	virtual bool open(OpenMode flags);
	virtual void close();
	virtual qint64 size() const;
	virtual bool seek(qint64 pos);
	virtual bool isSequential() const		{return false;}
	virtual bool atEnd() const				{return pos() >= size();}

protected:
	virtual qint64	readData(char *data, qint64 maxlen);
    virtual qint64	writeData(const char *data, qint64 len);

	struct SWinMemIO* m;
};
