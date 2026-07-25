#pragma once

//////////////////////////////////////////////////////////////
// Number std::string Conversion
//

/*std::wstring int2wstring(int Int);
int wstring2int(const std::wstring& Str);

std::string int2string(int Int);
int string2int(const std::string& Str);

std::wstring double2wstring(double Double);
double wstring2double(const std::wstring& Str);*/

//////////////////////////////////////////////////////////////
// String Comparations
//

bool CompareStr(const std::wstring &StrL,const std::wstring &StrR);

bool CompareStrs(const std::wstring &StrL,const std::wstring &StrR, wchar_t SepC = ' ', bool bTrim = false);

int compareex(const wchar_t *StrL, size_t OffL, size_t CountL, const wchar_t* StrR, size_t OffR, size_t CountR);

template <typename T>
const T* wildcmpex(const T* Wild, const T* Str)
{
	const T *cp = NULL, *mp = NULL;

	while ((*Str) && (*Wild != '*')) 
	{
		if ((*Wild != *Str) && (*Wild != '?')) 
			return NULL;
		Wild++;
		Str++;
	}

	while (*Str) 
	{
		if (*Wild == '*') 
		{
			if (!*++Wild) 
				return Str;
			mp = Wild;
			cp = Str+1;
		} 
		else if ((*Wild == *Str) || (*Wild == '?')) 
		{
			Wild++;
			Str++;
		}
		else 
		{
			Wild = mp;
			Str = cp++;
		}
	}

	while (*Wild == '*') 
		Wild++;
	return *Wild ? NULL : Str;
}

//////////////////////////////////////////////////////////////
// String Operations
//

std::wstring StrLine(const wchar_t* sLine, ...);
//std::string StrLine(const char* sLine, ...);

#pragma warning(push)
#pragma warning(disable : 4244)
__inline std::wstring s2w(const std::string& s) {return std::wstring(s.begin(),s.end());}
__inline std::string w2s(const std::wstring& w) {return std::string(w.begin(),w.end());}
#pragma warning(pop)

std::wstring MkLower(std::wstring Str);
std::wstring MkUpper(std::wstring Str);

template <typename T>
T Trimx(const T& String, const T& Blank)
{
	typename T::size_type Start = String.find_first_not_of(Blank);
	typename T::size_type End = String.find_last_not_of(Blank)+1;
	if(Start == std::wstring::npos)
		return T();
	return  String.substr(Start, End - Start);
}

__inline std::wstring Trim(const std::wstring& String)	{return Trimx(String, std::wstring(L" \r\n\t"));}
__inline std::string Trim(const std::string& String)		{return Trimx(String, std::string(" \r\n\t"));}

template <typename T>
std::pair<T,T> Split2x(const T& String, T Separator, bool Back)
{
	typename T::size_type Sep = Back ? String.rfind(Separator) : String.find(Separator);
	if(Sep != T::npos)
		return std::pair<T,T>(String.substr(0, Sep), String.substr(Sep+Separator.length()));
	return std::pair<T,T>(String, T());
}

__inline std::pair<std::string,std::string> Split2(const std::string& String, std::string Separator = ",", bool Back = false) {return Split2x(String, Separator, Back);}
__inline std::pair<std::wstring,std::wstring> Split2(const std::wstring& String, std::wstring Separator = L",", bool Back = false) {return Split2x(String, Separator, Back);}

template <typename T>
std::vector<T> SplitStrx(const T& String, const T& Separator, bool bKeepEmpty = true, bool bMulti = false)
{
	std::vector<T> StringList;
	typename T::size_type Pos = 0;
	for(;;)
	{
		typename T::size_type Sep = bMulti ? String.find_first_of(Separator,Pos) : String.find(Separator,Pos);
		if(Sep != T::npos)
		{
			if(bKeepEmpty || Sep-Pos > 0)
				StringList.push_back(String.substr(Pos,Sep-Pos));
			Pos = Sep+1;
		}
		else
		{
			if(bKeepEmpty || Pos < String.length())
				StringList.push_back(String.substr(Pos));
			break;
		}
	}
	return StringList;
}

__inline std::vector<std::string> SplitStr(const std::string& String, std::string Separator = ",", bool bKeepEmpty = true, bool bMulti = false) {return SplitStrx(String, Separator, bKeepEmpty, bMulti);}
__inline std::vector<std::wstring> SplitStr(const std::wstring& String, std::wstring Separator = L",", bool bKeepEmpty = true, bool bMulti = false) {return SplitStrx(String, Separator, bKeepEmpty, bMulti);}

template <typename T>
T JoinStrx(const std::vector<T>& StringList, const T& Separator)
{
	T String;
    for(typename std::vector<T>::const_iterator I = StringList.begin(); I != StringList.end(); I++)
	{
		if(!String.empty())
			String += Separator;
		String += *I;
	}
	return String;
}

__inline std::string JoinStr(const std::vector<std::string>& String, std::string Separator = ",") {return JoinStrx(String, Separator);}
__inline std::wstring JoinStr(const std::vector<std::wstring>& String, std::wstring Separator = L",") {return JoinStrx(String, Separator);}

std::wstring SubStrAt(const std::wstring& String, const std::wstring& Separator, int Index);
std::wstring::size_type FindNth(const std::wstring& String, const std::wstring& Separator, int Index);
std::wstring::size_type FindNthR(const std::wstring& String, const std::wstring& Separator, int Index);
int CountSep(const std::wstring& String, const std::wstring& Separator);

std::wstring::size_type FindStr(const std::wstring& str, const std::wstring& find, std::wstring::size_type Off = 0);
std::wstring::size_type RFindStr(const std::wstring& str, const std::wstring& find, std::wstring::size_type Off = std::wstring::npos);

__inline bool IsWhiteSpace(char Char) {return Char == ' ' || Char == '\t' || Char == '\r' || Char == '\n';}

template <typename T, typename S>
void toHexadecimal(T val, S *buf)
{
	int i;
    for (i = 0; i < sizeof(T) * 2; ++i) 
	{
        buf[i] = (val >> (4 * (sizeof(T) * 2 - 1 - i))) & 0xf;
        if (buf[i] < 10)
            buf[i] += '0';
        else
            buf[i] += 'A' - 10;
    }
    buf[i] = 0;
}

// templated version of my_equal so it could work with both char and wchar_t
/*template<typename charT>
struct my_equal {
    my_equal( const std::locale& loc ) : loc_(loc) {}
    bool operator()(charT ch1, charT ch2) {
        return std::toupper(ch1, loc_) == std::toupper(ch2, loc_);
    }
private:
    const std::locale& loc_;
};

// find substring (case insensitive)
template<typename T>
int ci_find( const T& str1, const T& str2, const std::locale& loc = std::locale() )
{
	typename T::const_iterator it = std::search( str1.begin(), str1.end(),
        str2.begin(), str2.end(), my_equal<typename T::value_type>(loc) );
    if ( it != str1.end() ) return it - str1.begin();
    else return -1; // not found
}*/

int wmemcmpex(const wchar_t *Str1, const wchar_t *Str2, size_t size);

std::wstring charArrayToWString(const char* charArray);

//////////////////////////////////////////////////////////////
// Manual conversion
//

wchar_t* UTF8toWCHAR(const char *str);
bool verify_encoding(std::string& target, bool fix_paths = false);
