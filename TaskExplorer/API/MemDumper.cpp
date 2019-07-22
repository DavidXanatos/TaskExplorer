#include "stdafx.h"
#include "MemDumper.h"
#ifdef WIN32
#include "Windows/WinDumper.h"
#endif // WIN32

CMemDumper::CMemDumper(QObject* parent)
	: QThread(parent)
{
}

CMemDumper::~CMemDumper()
{
	if(!wait(10*1000))
		terminate();
}

CMemDumper* CMemDumper::New()
{
#ifdef WIN32
	return new CWinDumper();
#else
	// todo: implement other systems liek Linux
#endif // WIN32
}