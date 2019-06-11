#include "stdafx.h"
#include "StackTrace.h"

int _CStackTracePtr_type = qRegisterMetaType<CStackTracePtr>("CStackTracePtr");

CStackTrace::CStackTrace(quint64 ProcessId, quint64 ThreadId)
{
	m_ProcessId = ProcessId;
	m_ThreadId = ThreadId;
}

void CStackTrace::AddFrame(QString Symbol, quint64 PcAddress, quint64 ReturnAddress, quint64 FrameAddress, quint64 StackAddress, quint64 BStoreAddress, quint64 Params[4], ulong Flags, const QString& FileInfo)
{
	SStackFrame Frame;
	Frame.Symbol = Symbol;
	Frame.PcAddress = PcAddress;
	Frame.ReturnAddress = ReturnAddress;
	Frame.FrameAddress = FrameAddress;
	Frame.StackAddress = StackAddress;
	Frame.BStoreAddress = BStoreAddress;
	Frame.Params[0] = Params[0];
	Frame.Params[1] = Params[1];
	Frame.Params[2] = Params[2];
	Frame.Params[3] = Params[3];
	Frame.Flags = Flags;
	Frame.FileInfo = FileInfo;
	m_StackFrames.prepend(Frame);
}

const CStackTrace::SStackFrame& CStackTrace::GetFrame(int index) const
{
	static SStackFrame dummy;
	if (index > GetCount())
		return dummy;
	return m_StackFrames.at(index);
}