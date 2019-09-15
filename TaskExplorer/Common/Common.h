#pragma once


#ifndef WIN32
int vswprintf_l(wchar_t * _String, size_t _Count, const wchar_t * _Format, va_list _Ap);
#endif

time_t GetTime();
quint64 GetCurTick();


quint64 GetRand64();
QString GetRand64Str(bool forUrl = true);

int	GetRandomInt(int iMin, int iMax);

typedef QPair<QString,QString> StrPair;
StrPair Split2(const QString& String, QString Separator = "=", bool Back = false);
QStringList SplitStr(const QString& String, QString Separator);


QString FormatSize(quint64 Size, int Precision = 2);
__inline QString FormatSizeEx(quint64 Size, bool bEx) { return bEx && (Size == 0) ? QString() : FormatSize(Size); }
QString FormatRate(quint64 Size, int Precision = 2);
__inline QString FormatRateEx(quint64 Size, bool bEx) { return bEx && (Size == 0) ? QString() : FormatRate(Size); }
QString FormatUnit(quint64 Size, int Precision = 0);
QString	FormatTime(quint64 Time, bool ms = false);
QString	FormatNumber(quint64 Number);
__inline QString FormatNumberEx(quint64 Number, bool bEx) { return bEx && (Number == 0) ? QString() : FormatNumber(Number); }
QString	FormatAddress(quint64 Address, int length = 16);


template <typename T>
QVariantList toVariantList( const QList<T> &list )
{
    QVariantList newList;
    foreach( const T &item, list )
        newList << item;

    return newList;
}

template <typename T>
QList<T> reversed( const QList<T> & in ) {
    QList<T> result;
    result.reserve( in.size() ); // reserve is new in Qt 4.7
    std::reverse_copy( in.begin(), in.end(), std::back_inserter( result ) );
    return result;
}

template <class T>
class CScoped
{
public:
	CScoped(T* Val = NULL)			{m_Val = Val;}
	~CScoped()						{delete m_Val;}

	CScoped<T>& operator=(const CScoped<T>& Scoped)	{ASSERT(0); return *this;} // copying is explicitly forbidden
	CScoped<T>& operator=(T* Val)	{ASSERT(!m_Val); m_Val = Val; return *this;}

	inline T* Val() const			{return m_Val;}
	inline T* &Val()				{return m_Val;}
	inline T* Detache()				{T* Val = m_Val; m_Val = NULL; return Val;}
    inline T* operator->() const	{return m_Val;}
    inline T& operator*() const     {return *m_Val;}
    inline operator T*() const		{return m_Val;}

private:
	T*	m_Val;
};

bool ReadFromDevice(QIODevice* dev, char* data, int len, int timeout = 5000);


void GrayScale (QImage& Image);

QIcon MakeActionIcon(const QString& IconFile);
QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text = "");
QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data);