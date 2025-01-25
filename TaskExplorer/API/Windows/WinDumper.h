#pragma once

#include "../MemDumper.h"
#include "WinProcess.h"

class CWinDumper : public CMemDumper
{
	Q_OBJECT
public:
	CWinDumper(QObject* parent = NULL);
	~CWinDumper();

	virtual STATUS	PrepareDump(const CProcessPtr& pProcess, quint32 DumpType, const QString& DumpPath);

	QString			GetFileName() const;
	quint64			GetProcessId() const;
	bool			IsCanceling() const;
	void			Cancel();

	bool			IsEnableProcessSnapshot() const;
	bool			IsProcessSnapshot() const;
	bool			IsEnableKernelSnapshot() const;

	void			SetKernelDumpHandle(void* hFile);

protected:
	virtual void	run();

	QSharedPointer<CWinProcess> m_pProcess;

private:
	struct SWinDumper* m;
};

quint32 SvcApiWriteMiniDumpProcess(const QVariantMap& Parameters);