#pragma once
#include <qobject.h>

class CStackTrace : public QSharedData
{
public:
	CStackTrace(quint64 ProcessId, quint64 ThreadId);

	quint64 GetThreadId()	const			{ return m_ThreadId; }
	quint64 GetProcessId()	const			{ return m_ProcessId; }

	
	void Clear()							{ m_StackFrames.clear(); }

	void AddFrame(QString Symbol, quint64 PcAddress, quint64 ReturnAddress, quint64 FrameAddress, quint64 StackAddress, quint64 BStoreAddress, quint64 Params[4], quint32 Flags, const QString& FileInfo = "");

	int	GetCount() const					{ return m_StackFrames.count(); }

	struct SStackFrame
	{
		QString Symbol;
		quint64 PcAddress;
		quint64 ReturnAddress;
		quint64 FrameAddress;
		quint64 StackAddress;
		quint64 BStoreAddress;
		quint64 Params[4];
		quint32 Flags;
		QString FileInfo;
	};

	const SStackFrame& GetFrame(int index) const;

protected:

	quint64 m_ProcessId;
	quint64 m_ThreadId;

	QList<SStackFrame>	m_StackFrames;
};

typedef QSharedDataPointer<CStackTrace> CStackTracePtr;
