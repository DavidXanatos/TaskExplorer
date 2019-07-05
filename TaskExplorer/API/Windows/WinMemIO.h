#pragma once

#include "WinMemory.h"

class CWinMemIO : public QIODevice
{
	Q_OBJECT
public:
	CWinMemIO(CWinMemory* pMemory, QObject* parent = NULL);
	virtual ~CWinMemIO();

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
