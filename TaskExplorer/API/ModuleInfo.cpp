#include "stdafx.h"
#include "ModuleInfo.h"


CModuleInfo::CModuleInfo(QObject *parent) : CAbstractInfo(parent)
{
	m_FileSize = 0;
	m_ModificationTime = 0;

	m_BaseAddress = NULL;
	m_Size = 0;
	m_ParentBaseAddress = 0;
	m_IsFirst = false;
	m_IsLoaded = false;
}

CModuleInfo::~CModuleInfo()
{
}
