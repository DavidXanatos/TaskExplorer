#include "stdafx.h"
#include "MemDumper.h"
#ifdef WIN32
#include "Windows/WinDumper.h"
#else
#include "Linux/LinuxDumper.h"
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
	return new CLinuxDumper();
#endif // WIN32
}