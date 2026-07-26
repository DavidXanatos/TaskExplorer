#include "stdafx.h"
#include "AbstractFinder.h"

#ifdef WIN32
#include "../Windows/Finders/WinHandleFinder.h"
#include "../Windows/Finders/WinModuleFinder.h"
#include "../Windows/Finders/WinStringFinder.h"
#else
#include "../Linux/Finders/LinuxHandleFinder.h"
#include "../Linux/Finders/LinuxModuleFinder.h"
#include "../Linux/Finders/LinuxStringFinder.h"
#endif

CAbstractFinder::CAbstractFinder(QObject* parent) : QThread(parent) 
{
	m_bCancel = false;
}

CAbstractFinder::~CAbstractFinder() 
{
	m_bCancel = true;
	if(!wait(10*1000))
		terminate();
}

CAbstractFinder* CAbstractFinder::FindHandles(const QVariant& Type, const QRegularExpression& RegExp)
{
#ifdef WIN32
	return new CWinHandleFinder(Type, RegExp);
#else
	return new CLinuxHandleFinder(Type, RegExp);
#endif // WIN32
}

CAbstractFinder* CAbstractFinder::FindModules(const QVariant& Type, const QRegularExpression& RegExp)
{
#ifdef WIN32
	return new CWinModuleFinder(Type, RegExp);
#else
	return new CLinuxModuleFinder(Type, RegExp);
#endif // WIN32
}

CAbstractFinder* CAbstractFinder::FindStrings(const SMemOptions& Options, const QRegularExpression& RegExp, const CProcessPtr& pProcess)
{
#ifdef WIN32
	return new CWinStringFinder(Options, RegExp, pProcess);
#else
	return new CLinuxStringFinder(Options, RegExp, pProcess);
#endif // WIN32
}