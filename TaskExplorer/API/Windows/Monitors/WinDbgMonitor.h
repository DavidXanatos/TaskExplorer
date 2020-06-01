#pragma once

#include "../../../../MiscHelpers/Common/FlexError.h"

class CWinDbgMonitor : public QObject
{
	Q_OBJECT
public:
	CWinDbgMonitor(QObject *parent = nullptr);
    virtual ~CWinDbgMonitor();

	enum EModes
	{
		eNone = 0,
		eLocal = 1,
		eGlobal = 2,
		eKernel = 4,
		eAll = eLocal | eGlobal | eKernel
	};

	virtual STATUS		SetMonitor(EModes Mode);

	virtual EModes		GetMode() const;

signals:
	void DebugMessage(quint64 PID, const QString& Message, const QDateTime& TimeStamp = QDateTime::currentDateTime());

protected:
	friend long DbgEventsThread(bool bGlobal, CWinDbgMonitor* This);

	struct SWinDbgMonitor* m;
};
