#include "stdafx.h"
#include "Functions.h"
#ifdef USE_OPENSSL
#include <openssl/rand.h>
#endif

#ifndef WIN32 // vswprintf
#include <stdio.h>
#include <stdarg.h>


int vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap)
{
	wchar_t _Format_l[1025];
	ASSERT(wcslen(_Format) < 1024);
	wcscpy(_Format_l, _Format);

	for(int i=0; i<wcslen(_Format_l); i++)
	{
		if(_Format_l[i] == L'%')
		{
			switch(_Format_l[i+1])
			{
				case L's':	_Format_l[i+1] = 'S'; break;
				case L'S':	_Format_l[i+1] = 's'; break;
			}
		}
	}

	return vswprintf(_String, _Count, _Format_l, _Ap);
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// Time Functions
// 

time_t GetTime()
{
	QDateTime dateTime = QDateTime::currentDateTime();
	time_t time = dateTime.toTime_t(); // returns time in seconds (since 1970-01-01T00:00:00) in UTC !
	return time;
}

struct SCurTick
{
	SCurTick()	{Timer.start();}
	quint64 Get(){return Timer.elapsed();}
	QElapsedTimer Timer;
}	g_CurTick;

quint64 GetCurTick()
{
	return g_CurTick.Get();
}

uint MkTick(uint& uCounter)
{
	uCounter++;

	uint Tick = E100PerSec;
	if (uCounter % 10 == 0) // every 100 ms
		Tick |= E10PerSec;
	if (uCounter % 50 == 0) // every 500 ms
		Tick |= E2PerSec;
	if (uCounter % 25 == 0) // every 250 ms
		Tick |= E4PerSec;
	if (uCounter % 100 == 0) // every 1 s
		Tick |= EPerSec;
	if (uCounter % 500 == 0) // every 5 s
		Tick |= EPer5Sec;
	if (uCounter % 1000 == 0) // every 10 s
		Tick |= EPer10Sec;
	if (uCounter % 10000 == 0) // every 100 s
	{
		Tick |= EPer100Sec;
		uCounter = 0;
	}

	return Tick;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Other Functions
// 

quint64 GetRand64()
{
	quint64 Rand64;
#ifdef USE_OPENSSL
	int Ret = RAND_bytes((byte*)&Rand64, sizeof(uint64));
	ASSERT(Ret == 1); // An error occurs if the PRNG has not been seeded with enough randomness to ensure an unpredictable byte sequence.
#else
	//CryptoPP::AutoSeededRandomPool rng;
	//rng.GenerateBlock((byte*)&Rand64, sizeof(quint64));

	((quint16*)&Rand64)[0] = qrand();
	((quint16*)&Rand64)[1] = qrand();
	((quint16*)&Rand64)[2] = qrand();
	((quint16*)&Rand64)[3] = qrand();
#endif
	return Rand64;
}

QString GetRand64Str(bool forUrl)
{
	quint64 Rand64 = GetRand64();
	QString sRand64 = QByteArray((char*)&Rand64,sizeof(quint64)).toBase64();
	if(forUrl)
		sRand64.replace("+","-").replace("/","_");
	return sRand64.replace("=","");
}

StrPair Split2(const QString& String, QString Separator, bool Back)
{
	int Sep = Back ? String.lastIndexOf(Separator) : String.indexOf(Separator);
	if(Sep != -1)
		return qMakePair(String.left(Sep).trimmed(), String.mid(Sep+Separator.length()).trimmed());
	return qMakePair(String.trimmed(), QString());
}

QStringList SplitStr(const QString& String, QString Separator)
{
	QStringList List = String.split(Separator);
	for(int i=0; i < List.count(); i++)
	{
		List[i] = List[i].trimmed();
		if(List[i].isEmpty())
			List.removeAt(i--);
	}
	return List;
}