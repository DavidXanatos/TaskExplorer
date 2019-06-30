#pragma once
#include "ProcessInfo.h"

class CMemDumper : public QThread
{
	Q_OBJECT
public:
	CMemDumper(QObject* parent = NULL);
	~CMemDumper();

	static CMemDumper* New();

	virtual STATUS	PrepareDump(const CProcessPtr& pProcess, const QString& DumpPath) = 0;

public slots:
	virtual void	Cancel() = 0;

signals:
	void			ProgressMessage(const QString& Message, int Progress = -1);
	void			StatusMessage(const QString& Message, int Code = 0);

protected:
	virtual void	run() {}
};