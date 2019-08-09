#include "stdafx.h"
#include "Common.h"

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

	Rand64 = QRandomGenerator::system()->generate64();
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


int	GetRandomInt(int iMin, int iMax)
{
	return QRandomGenerator::system()->bounded(iMin, iMax);
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

QString FormatSize(quint64 Size, int Precision)
{
	double Div;
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " EB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " PB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " TB";
	if(Size > (quint64)(Div = 1.0*1024*1024*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " GB";
	if(Size > (quint64)(Div = 1.0*1024*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " MB";
	if(Size > (quint64)(Div = 1.0*1024))
		return QString::number(double(Size)/Div, 'f', Precision) + " kB";
	return QString::number(double(Size)) + "B";
}

QString FormatUnit(quint64 Size, int Precision)
{
	double Div;
	if(Size > (quint64)(Div = 1.0*1000*1000*1000*1024*1000*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " E";
	if(Size > (quint64)(Div = 1.0*1000*1000*1000*1000*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " P";
	if(Size > (quint64)(Div = 1.0*1000*1000*1000*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " T";
	if(Size > (quint64)(Div = 1.0*1000*1000*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " G";
	if(Size > (quint64)(Div = 1.0*1000*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " M";
	if(Size > (quint64)(Div = 1.0*1000))
		return QString::number(double(Size)/Div, 'f', Precision) + " K";
	return QString::number(double(Size));
}


QString FormatTime(quint64 Time)
{
	int seconds = Time % 60;
	Time /= 60;
	int minutes = Time % 60;
	Time /= 60;
	int hours = Time % 24;
	int days = Time / 24;
	if((hours == 0) && (days == 0))
		return QString().sprintf("%02d:%02d", minutes, seconds);
	if (days == 0)
		return QString().sprintf("%02d:%02d:%02d", hours, minutes, seconds);
	return QString().sprintf("%dd%02d:%02d:%02d", days, hours, minutes, seconds);
}

bool ReadFromDevice(QIODevice* dev, char* data, int len, int timeout)
{
	while (dev->bytesAvailable() < len) {
		if (!dev->waitForReadyRead(timeout))
			return false;
	}
	return dev->read(data, len) == len;
}


void GrayScale (QImage& Image)
{
	if (Image.depth () == 32)
	{
		uchar* r = (Image.bits ());
		uchar* g = (Image.bits () + 1);
		uchar* b = (Image.bits () + 2);

		uchar* end = (Image.bits() + Image.byteCount ());
		while (r != end)
		{
			*r = *g = *b = (((*r + *g) >> 1) + *b) >> 1; // (r + b + g) / 3

			r += 4;
			g += 4;
			b += 4;
		}
	}
	else
	{
		for (int i = 0; i < Image.colorCount (); i++)
		{
			uint r = qRed (Image.color (i));
			uint g = qGreen (Image.color (i));
			uint b = qBlue (Image.color (i));

			uint gray = (((r + g) >> 1) + b) >> 1;

			Image.setColor (i, qRgba (gray, gray, gray, qAlpha (Image.color (i))));
		}
	}
}

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text)
{
	QAction* pAction = new QAction(Text, pParent);
	
	QImage Image(IconFile);
	QIcon Icon;
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
	GrayScale(Image);
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
	pAction->setIcon(Icon);
	
	pParent->addAction(pAction);
	return pAction;
}

QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		return pParent->addMenu(Icon, Text);
	}
	return pParent->addMenu(Text);
}

QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile)
{
	QAction* pAction = new QAction(Text, pParent);
	if(!IconFile.isEmpty())
	{
		QImage Image(IconFile);
		QIcon Icon;
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
		GrayScale(Image);
		Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
		pAction->setIcon(Icon);
	}
	pParent->addAction(pAction);
	return pAction;
}

QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data)
{
	QAction* pAction = new QAction(Text, pParent);
	pAction->setCheckable(true);
	pAction->setData(Data);
	pAction->setActionGroup(pGroup);
	pParent->addAction(pAction);
	return pAction;
}