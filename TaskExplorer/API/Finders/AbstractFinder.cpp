#include "stdafx.h"
#include "AbstractFinder.h"

#ifdef WIN32
#include "../Windows/Finders/WinHandleFinder.h"
#include "../Windows/Finders/WinModuleFinder.h"
#include "../Windows/Finders/WinStringFinder.h"
#endif

int _QList_QSharedPointer_QObject_type = qRegisterMetaType<QList<QSharedPointer<QObject> >>("QList<QSharedPointer<QObject> >");

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
	// linux-todo:
#endif // WIN32
}

CAbstractFinder* CAbstractFinder::FindModules(const QVariant& Type, const QRegularExpression& RegExp)
{
#ifdef WIN32
	return new CWinModuleFinder(Type, RegExp);
#else
	// linux-todo:
#endif // WIN32
}

CAbstractFinder* CAbstractFinder::FindStrings(const SMemOptions& Options, const QRegularExpression& RegExp, const CProcessPtr& pProcess)
{
#ifdef WIN32
	return new CWinStringFinder(Options, RegExp, pProcess);
#else
	// linux-todo:
#endif // WIN32
}