#pragma once

#include "../MemDumper.h"

class CWinDumper : public CMemDumper
{
	Q_OBJECT
public:
	CWinDumper(QObject* parent = NULL);
	~CWinDumper();

	virtual STATUS	PrepareDump(const CProcessPtr& pProcess, const QString& DumpPath);

	quint64			GetProcessId() const;
	bool			IsCanceling() const;
	void			Cancel();

protected:
	virtual void	run();

private:
	struct SWinDumper* m;
};

quint32 PhSvcApiWriteMiniDumpProcess(const QVariantMap& Parameters);