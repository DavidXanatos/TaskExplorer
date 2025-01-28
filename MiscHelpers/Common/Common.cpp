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
	time_t time = dateTime.toSecsSinceEpoch(); // returns time in seconds (since 1970-01-01T00:00:00) in UTC !
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

QString UnEscape(QString Text)
{
	QString Value;
	bool bEsc = false;
	for(int i = 0; i < Text.size(); i++)
	{
		QChar Char = Text.at(i);
		if(bEsc)
		{
			switch(Char.unicode())
			{
				case L'\\':	Value += L'\\';	break;
				case L'\'':	Value += L'\'';	break;
				case L'\"':	Value += L'\"';	break;
				case L'a':	Value += L'\a';	break;
				case L'b':	Value += L'\b';	break;
				case L'f':	Value += L'\f';	break;
				case L'n':	Value += L'\n';	break;
				case L'r':	Value += L'\r';	break;
				case L't':	Value += L'\t';	break;
				case L'v':	Value += L'\v';	break;
				default:	Value += Char.unicode();break;
			}
			bEsc = false;
		}
		else if(Char == L'\\')
			bEsc = true;
		else
			Value += Char;
	}	
	return Value;
}

//////////////////////////////////////////////////////////////////////////////////////////
// Other Functions
// 

quint64 GetRand64()
{
	quint64 Rand64;
#ifdef USE_OPENSSL
	int Ret = RAND_bytes((byte*)&Rand64, sizeof(quint64));
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
		return QString::number(double(Size)/Div, 'f', Precision) + " KB";
	return QString::number(double(Size)) + "B";
}

QString FormatRate(quint64 Size, int Precision)
{
	return FormatSize(Size, Precision) + "/s";
}

QString FormatUnit(quint64 Size, int Precision)
{
	double Div;
	if(Size > (quint64)(Div = 1.0*1000*1000*1000*1000*1000*1000))
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


QString FormatTime(quint64 Time, bool ms)
{
	int miliseconds = 0;
	if (ms) {
		miliseconds = Time % 1000;
		Time /= 1000;
	}
	int seconds = Time % 60;
	Time /= 60;
	int minutes = Time % 60;
	Time /= 60;
	int hours = Time % 24;
	int days = Time / 24;
	if (ms && (minutes == 0) && (hours == 0) && (days == 0))
		return QString("%1.%2").arg(seconds, 2, 10, QChar('0')).arg(miliseconds, 2, 10, QChar('0'));
	if ((hours == 0) && (days == 0))
		return QString("%1:%2").arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	if (days == 0)
		return QString("%1:%2:%3").arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
	return QString("%1d%2:%3:%4").arg(days, 2, 10, QChar('0')).arg(hours, 2, 10, QChar('0')).arg(minutes, 2, 10, QChar('0')).arg(seconds, 2, 10, QChar('0'));
}

QString	FormatNumber(quint64 Number)
{
	QString String = QString::number(Number);
	for (int i = String.length() - 3; i > 0; i -= 3)
		String.insert(i, QString::fromWCharArray(L"\u202F")); // L"\u2009"
	return String;
}

QString	FormatAddress(quint64 Address, int length)
{
	return "0x" + QString::number(Address, 16).rightJustified(length, '0');
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

		uchar* end = (Image.bits() + Image.sizeInBytes());
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

QIcon MakeActionIcon(const QString& IconFile)
{
	QImage Image(IconFile);
	QIcon Icon;
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Normal);
	GrayScale(Image);
	Icon.addPixmap(QPixmap::fromImage(Image), QIcon::Disabled);
	return Icon;
}

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text)
{
	QAction* pAction = new QAction(Text, pParent);
	pAction->setIcon(MakeActionIcon(IconFile));
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

QAction* MakeActionCheck(QMenu* pParent, const QString& Text, const QVariant& Data, bool bTriState)
{
	QCheckBox *checkBox = new QCheckBox(Text, pParent);
	if(bTriState) checkBox->setTristate(true);
	QWidgetAction *pAction = new QWidgetAction(pParent);
	QObject::connect(checkBox, SIGNAL(stateChanged(int)), pAction, SLOT(trigger()));
	pAction->setDefaultWidget(checkBox);
	pAction->setData(Data);
	pParent->addAction(pAction);
	return pAction;
}


#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
bool operator < (const QVariant& l, const QVariant& r)
{
	auto lt = l.type();
	//auto lv = l.isValid();
	//auto ln = l.isNull();
	auto rt = r.type();
	//auto rv = r.isValid();
	//auto rn = r.isNull();
	if(lt != rt)
		return lt < rt;
	if (lt == QVariant::List)
	{
		auto lList = l.toList();
		auto rList = r.toList();
		if (lList.size() != rList.size())
			return lList.size() < rList.size();
		for (int i = 0; i < lList.size(); i++)
		{
			if (lList[i] < rList[i])
				return true;
			if (rList[i] < lList[i])
				return false;
		}
		return false;
	}
	auto ret = QVariant::compare(l, r);
	Q_ASSERT(ret != QPartialOrdering::Unordered);
	return ret == QPartialOrdering::Less;
}
#endif

//////////////////////////////////////////////////////////////////////////////////////////
// 
// 

#ifdef WIN32
#include <windows.h>
#include <io.h>
#include <fcntl.h>

bool InitConsole(bool bCreateIfNeeded)
{
	if (AttachConsole(ATTACH_PARENT_PROCESS) == FALSE)
	{
		if (!bCreateIfNeeded)
			return false;
		AllocConsole();
	}
	freopen("CONOUT$", "w", stdout);
	printf("\r\n");
	return true;
}
#endif