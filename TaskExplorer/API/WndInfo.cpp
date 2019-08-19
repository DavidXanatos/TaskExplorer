#include "stdafx.h"
#include "WndInfo.h"


CWndInfo::CWndInfo(QObject *parent) : CAbstractInfo(parent)
{
	m_hWnd = 0;
	m_ParentWnd = 0;
	m_ProcessId = 0;
    m_ThreadId = 0;
	m_WindowVisible = false;
	m_WindowEnabled = false;
	m_ShowCommand = 0;
	m_WindowOnTop = false;
	m_WindowAlpha = 255;
}

CWndInfo::~CWndInfo()
{
}
