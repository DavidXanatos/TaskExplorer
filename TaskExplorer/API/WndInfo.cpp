#include "stdafx.h"
#include "WndInfo.h"


CWndInfo::CWndInfo(QObject *parent) : QObject(parent)
{
	m_hWnd = 0;
	m_ParentWnd = 0;
	m_ProcessId = 0;
    m_ThreadId = 0;
	m_WindowVisible = false;
	m_WindowEnabled = false;
	m_WindowOnTop = false;
	m_WindowAlpha = 255;
}

CWndInfo::~CWndInfo()
{
}
