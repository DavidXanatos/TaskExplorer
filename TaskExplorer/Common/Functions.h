#pragma once

#ifndef WIN32
int vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap);
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Time Functions
// 

time_t GetTime();
quint64 GetCurTick();

enum EEProcessTicks
{
	EPer100Sec		= 128,		// 1000 0000
	EPer10Sec		= 64,		// 0100 0000
	EPer5Sec		= 32,		// 0010 0000
	EPerSec			= 16,		// 0001 0000
	E2PerSec		= 8,		// 0000 1000
	E4PerSec		= 4,		// 0000 0100
	E10PerSec		= 2,		// 0000 0010
	E100PerSec		= 1,		// 0000 0001
};

uint MkTick(uint& uCounter);

//////////////////////////////////////////////////////////////////////////////////////////
// Other Functions
// 

quint64 GetRand64();
QString GetRand64Str(bool forUrl = true);

inline int		GetRandomInt(int iMin, int iMax)			{return qrand() % ((iMax + 1) - iMin) + iMin;}

typedef QPair<QString,QString> StrPair;
StrPair Split2(const QString& String, QString Separator = "=", bool Back = false);
QStringList SplitStr(const QString& String, QString Separator);