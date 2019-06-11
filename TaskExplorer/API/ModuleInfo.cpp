#include "stdafx.h"
#include "ModuleInfo.h"


CModuleInfo::CModuleInfo(QObject *parent) : QObject(parent)
{
	m_BaseAddress = NULL;
	m_Size = 0;
	m_ParentBaseAddress = 0;
	m_IsFirst = false;
}

CModuleInfo::~CModuleInfo()
{
}
