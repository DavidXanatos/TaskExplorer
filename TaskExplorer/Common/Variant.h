#pragma once

#include "Buffer.h"
//#include "Strings.h"
#include "VariantDefs.h"

union _TVarIndex
{
	uint32 u;
	char c[4];
};

__inline bool operator < (_TVarIndex l, _TVarIndex r) { return l.u < r.u; }

struct SVarName
{
	const char* Buf;
	size_t Len;
};

#define VAR_TEST_NAME(Name,Test) (Name.Len == (sizeof(Test) - 1) && memcmp(Test, Name.Buf, Name.Len) == 0)

class CVariant
{
public:
	typedef VAR_TYPE EType;

	CVariant(EType Type = VAR_TYPE_EMPTY);
	CVariant(const CVariant& Variant);
	~CVariant();

	void					FromPacket(const CBuffer* pPacket, bool bDerived = false);
	void					ToPacket(CBuffer* pPacket/*, bool bPack = false*/) const;

	CVariant& operator=(const CVariant& Variant) {Assign(Variant); return *this;}
	int						Compare(const CVariant &R) const;
	bool					CompareTo(const CVariant &Variant) const	{return Compare(Variant) == 0;}
	bool operator==(const CVariant &Variant) const						{return Compare(Variant) == 0;}
	bool operator!=(const CVariant &Variant) const						{return Compare(Variant) != 0;}

	void					Freeze();
	void					Unfreeze();
	bool					IsFrozen() const;

	uint32					Count() const;

	bool					IsMap() const;
	const char*				Key(uint32 Index) const;
	std::wstring			WKey(uint32 Index) const;
	CVariant&				At(const char* Name, size_t Len = -1);
	const CVariant&			At(const char* Name, size_t Len = -1) const;
	CVariant				Get(const char* Name, const CVariant& Default = CVariant()) const;
	bool					Has(const char* Name) const;
	void					Remove(const char* Name);
	void					Insert(const char* Name, const CVariant& Variant);

	bool					IsList() const;
	void					Append(const CVariant& Variant);
	const CVariant&			At(uint32 Index) const;
	void					Remove(uint32 Index);

	bool					IsIndex() const;
	uint32					Id(uint32 Index) const;
	CVariant&				At(uint32 Index);
	CVariant				Get(uint32 Index, const CVariant& Default = CVariant()) const;
	bool					Has(uint32 Index) const;
	void					Insert(uint32 Index, const CVariant& Variant);

	void					Merge(const CVariant& Variant);

	CVariant& operator[](const char* Name)				{return At(Name);}
	const CVariant& operator[](const char* Name) const	{return At(Name);}
	CVariant& operator[](uint32 Index)					{return At(Index);}
	const CVariant& operator[](uint32 Index)	const	{return At(Index);}

	CVariant(const byte* Payload, size_t Size, EType Type = VAR_TYPE_BYTES)		{InitValue(Type, Size, Payload);}
	operator byte*() const							{const SVariant* Variant = Val(); if(Variant->IsRaw()) return Variant->Payload; return NULL;}
	byte*					GetData() const			{const SVariant* Variant = Val(); return Variant->Payload;}
	uint32					GetSize() const			{const SVariant* Variant = Val(); return Variant->Size;}
	EType					GetType() const;
	bool					IsValid() const;

	void					Clear();
	CVariant				Clone(bool Full = true) const;

	CVariant(const CBuffer& Buffer)					{InitValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}
	CVariant(CBuffer& Buffer, bool bTake = false)	{size_t uSize = Buffer.GetSize(); InitValue(VAR_TYPE_BYTES, uSize, Buffer.GetBuffer(bTake), bTake);}
	operator CBuffer() const						{const SVariant* Variant = Val(); if(Variant->IsRaw()) return CBuffer(Variant->Payload, Variant->Size, true); return CBuffer();}

	template <class T> T To() const					{return (T)*this;}

	std::string				ToString() const;
	std::wstring			ToWString() const;

	template <class T> T AsNum(bool* pOk = NULL) const
	{
		if (pOk) *pOk = true;
		try {
			EType Type = GetType();
			if (Type == VAR_TYPE_DOUBLE) {
				double val = 0;
				GetDouble(sizeof(double), &val);
				return (long long)val;
			}
			else if (Type == VAR_TYPE_SINT) {
				sint64 sint = 0;
				GetInt(sizeof(sint64), &sint);
				return sint;
			}
			else if (Type == VAR_TYPE_UINT) {
				uint64 uint = 0;
				GetInt(sizeof(sint64), &uint);
				return uint;
			}
			else if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			{
				std::wstring Num = ToWString();
				wchar_t* endPtr;
				if (Num.find(L".") != std::wstring::npos)
					return (T)std::wcstod(Num.c_str(), &endPtr);
				else
					return (T)std::wcstoll(Num.c_str(), &endPtr, 10);
			}
		} catch (...) {}
		if (pOk) *pOk = false;
		return L"";
	}
	std::wstring AsStr(bool* pOk = NULL) const
	{
		if (pOk) *pOk = true;
		try {
			EType Type = GetType();
			if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
				return ToWString();
			else if (Type == VAR_TYPE_DOUBLE) {
				double val = 0;
				GetDouble(sizeof(double), &val);
				return std::to_wstring(val);
			}
			else if (Type == VAR_TYPE_SINT) {
				sint64 sint = 0;
				GetInt(sizeof(sint64), &sint);
				return std::to_wstring(sint);
			}
			else if (Type == VAR_TYPE_UINT) {
				uint64 uint = 0;
				GetInt(sizeof(sint64), &uint);
				return std::to_wstring(uint);
			}
		}
		catch (...) {}
		if (pOk) *pOk = false;
		return L"";
	}
	std::vector<byte> AsBytes() const
	{
		const SVariant* Variant = Val(); 
		if (Variant->IsRaw()) { 
			std::vector<byte> vec(Variant->Size); 
			memcpy(vec.data(), Variant->Payload, Variant->Size); 
			return vec; 
		} 
		return std::vector<byte>();
	}

	template<typename T>
	CVariant(const T& List) : CVariant() 
	{
		BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			Write(*I);
		Finish();
	}

	template<typename T>
	std::vector<T>			AsList() const {
		std::vector<T> List;
		ReadRawList([&](const CVariant& Data) {
			List.push_back(Data.To<T>());
		});
		return List;
	}

	std::vector<std::wstring> AsStrList() const {
		std::vector<std::wstring> List;
		ReadRawList([&](const CVariant& Data) {
			List.push_back(Data.AsStr());
		});
		return List;
	}

	CVariant(const bool& b)				{InitValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	operator bool() const				{bool b = false; GetInt(sizeof(bool), &b); return b;}
	CVariant(const sint8& sint)			{InitValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	operator sint8() const				{sint8 sint = 0; GetInt(sizeof(sint8), &sint); return sint;}
	CVariant(const uint8& uint)			{InitValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	operator uint8() const				{uint8 uint = 0; GetInt(sizeof(uint8), &uint); return uint;}
	CVariant(const sint16& sint)		{InitValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	operator sint16() const				{sint16 sint = 0; GetInt(sizeof(sint16), &sint); return sint;}
	CVariant(const uint16& uint)		{InitValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	operator uint16() const				{uint16 uint = 0; GetInt(sizeof(uint16), &uint); return uint;}
	CVariant(const sint32& sint)		{InitValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	operator sint32() const				{sint32 sint = 0; GetInt(sizeof(sint32), &sint); return sint;}
	CVariant(const uint32& uint)		{InitValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
	operator uint32() const				{uint32 uint = 0; GetInt(sizeof(uint32), &uint); return uint;}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	CVariant(const time_t& time)		{sint64 uint = time; InitValue(VAR_TYPE_UINT, sizeof(sint64), &uint);}
	operator time_t() const				{sint64 uint = 0; GetInt(sizeof(sint64), &uint); return uint;}
#endif
	CVariant(const sint64& sint)		{InitValue(VAR_TYPE_SINT, sizeof(sint64), &sint);}
	operator sint64() const				{sint64 sint = 0; GetInt(sizeof(sint64), &sint); return sint;}
	CVariant(const uint64& uint)		{InitValue(VAR_TYPE_UINT, sizeof(uint64), &uint);}
	operator uint64() const				{uint64 uint = 0; GetInt(sizeof(uint64), &uint); return uint;}

	CVariant(const float& val)			{InitValue(VAR_TYPE_DOUBLE, sizeof(float), &val);}
	operator float() const				{float val = 0; GetDouble(sizeof(float), &val); return val;}
	CVariant(const double& val)			{InitValue(VAR_TYPE_DOUBLE, sizeof(double), &val);}
	operator double() const				{double val = 0; GetDouble(sizeof(double), &val); return val;}

	CVariant(const std::string& str)	{InitValue(VAR_TYPE_ASCII, str.length(), str.c_str());}
	CVariant(const char* str)			{InitValue(VAR_TYPE_ASCII, strlen(str), str);}
	operator std::string() const		{return ToString();}
	CVariant(const std::wstring& wstr, bool bUtf8 = false) : CVariant(wstr.c_str(), wstr.length(), bUtf8) {}
	CVariant(const wchar_t* wstr, size_t len = -1, bool bUtf8 = false)
	{
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			InitValue(VAR_TYPE_UTF8, len, pUtf8, true);
		} else
			InitValue(VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
	}
	operator std::wstring() const		{return ToWString();}

	CVariant(const std::vector<byte>& vec)	{InitValue(VAR_TYPE_BYTES, vec.size(), vec.data());}
	//operator std::vector<byte>() const		{ const SVariant* Variant = Val(); if (Variant->IsRaw()) { std::vector<byte> vec(Variant->Size); memcpy(vec.data(), Variant->Payload, Variant->Size); return vec; } return std::vector<byte>(); }

	
	//////////////////////////////////////////////////////////////////////////////////////////////
	// Map Read/Write

	bool ReadRawMap(const std::function<void(const SVarName& Name, const CVariant& Data)>& cb) const;

	bool Find(const char* Name, CVariant& Data) const;
	CVariant Find(const char* Name) const {CVariant Data; Find(Name, Data); return Data;}

	void BeginMap();
	void WriteValue(const char* Name, EType Type, size_t Size, const void* Value);

	void WriteVariant(const char* Name, const CVariant& Variant);

	void Write(const char* Name, const CBuffer& Buffer)		{WriteValue(Name, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	void Write(const char* Name, const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		WriteVariant(Name, vList);
	}

	void Write(const char* Name, const bool& b)				{WriteValue(Name, VAR_TYPE_SINT, sizeof(bool), &b);}
	void Write(const char* Name, const sint8& sint)			{WriteValue(Name, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	void Write(const char* Name, const uint8& uint)			{WriteValue(Name, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	void Write(const char* Name, const sint16& sint)		{WriteValue(Name, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	void Write(const char* Name, const uint16& uint)		{WriteValue(Name, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	void Write(const char* Name, const sint32& sint)		{WriteValue(Name, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	void Write(const char* Name, const uint32& uint)		{WriteValue(Name, VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	void Write(const char* Name, const time_t& time)		{sint64 uint = time; WriteValue(Name, VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
	void Write(const char* Name, const sint64& sint)		{WriteValue(Name, VAR_TYPE_SINT, sizeof(sint64), &sint);}
	void Write(const char* Name, const uint64& uint)		{WriteValue(Name, VAR_TYPE_UINT, sizeof(uint64), &uint);}

	void Write(const char* Name, const float& val)			{WriteValue(Name, VAR_TYPE_DOUBLE, sizeof(float), &val);}
	void Write(const char* Name, const double& val)			{WriteValue(Name, VAR_TYPE_DOUBLE, sizeof(double), &val);}

	void Write(const char* Name, const std::string& str)	{WriteValue(Name, VAR_TYPE_ASCII, str.length(), str.c_str());}
	void Write(const char* Name, const char* str)			{WriteValue(Name, VAR_TYPE_ASCII, strlen(str), str);}
	void Write(const char* Name, const std::wstring& wstr, bool bUtf8 = false) {Write(Name, wstr.c_str(), wstr.length(), bUtf8);}
	void Write(const char* Name, const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			WriteValue(Name, VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
			WriteValue(Name, VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
	}

	void Write(const char* Name, const std::vector<byte>& vec) {WriteValue(Name, VAR_TYPE_BYTES, vec.size(), vec.data());}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// List Read/Write

	bool ReadRawList(const std::function<void(const CVariant& Data)>& cb) const;

	void BeginList();
	void WriteValue(EType Type, size_t Size, const void* Value);

	void WriteVariant(const CVariant& Variant);

	void Write(const CBuffer& Buffer)		{WriteValue(VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	void Write(const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		WriteVariant(vList);
	}

	void Write(const bool& b)				{WriteValue(VAR_TYPE_SINT, sizeof(bool), &b);}
	void Write(const sint8& sint)			{WriteValue(VAR_TYPE_SINT, sizeof(sint8), &sint);}
	void Write(const uint8& uint)			{WriteValue(VAR_TYPE_UINT, sizeof(uint8), &uint);}
	void Write(const sint16& sint)			{WriteValue(VAR_TYPE_SINT, sizeof(sint16), &sint);}
	void Write(const uint16& uint)			{WriteValue(VAR_TYPE_UINT, sizeof(uint16), &uint);}
	void Write(const sint32& sint)			{WriteValue(VAR_TYPE_SINT, sizeof(sint32), &sint);}
	void Write(const uint32& uint)			{WriteValue(VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	void Write(const time_t& time)		{sint64 uint = time; WriteValue(VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
	void Write(const sint64& sint)			{WriteValue(VAR_TYPE_SINT, sizeof(sint64), &sint);}
	void Write(const uint64& uint)			{WriteValue(VAR_TYPE_UINT, sizeof(uint64), &uint);}

	void Write(const float& val)			{WriteValue(VAR_TYPE_DOUBLE, sizeof(float), &val);}
	void Write(const double& val)			{WriteValue(VAR_TYPE_DOUBLE, sizeof(double), &val);}

	void Write(const std::string& str)		{WriteValue(VAR_TYPE_ASCII, str.length(), str.c_str());}
	void Write(const char* str)				{WriteValue(VAR_TYPE_ASCII, strlen(str), str);}
	void Write(const std::wstring& wstr, bool bUtf8 = false) {Write(wstr.c_str(), wstr.length(), bUtf8);}
	void Write(const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			WriteValue(VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
			WriteValue(VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
	}

	void Write(const std::vector<byte>& vec) {WriteValue(VAR_TYPE_BYTES, vec.size(), vec.data());}

	//////////////////////////////////////////////////////////////////////////////////////////////
	// IMap Read/Write

	bool ReadRawIMap(const std::function<void(uint32 Index, const CVariant& Data)>& cb) const;

	bool Find(uint32 Index, CVariant& Data) const;
	CVariant Find(uint32 Index) const {CVariant Data; Find(Index, Data); return Data;}

	void BeginIMap();
	void WriteValue(uint32 Index, EType Type, size_t Size, const void* Value);

	void WriteVariant(uint32 Index, const CVariant& Variant);

	void Write(uint32 Index, const CBuffer& Buffer)		{WriteValue(Index, VAR_TYPE_BYTES, Buffer.GetSize(), Buffer.GetBuffer());}

	template<typename T>
	void Write(uint32 Index, const T& List)
	{
		CVariant vList;
		vList.BeginList();
		for(auto I = List.begin(); I != List.end(); ++I)
			vList.Write(*I);
		vList.Finish();
		WriteVariant(Index, vList);
	}

	void Write(uint32 Index, const bool& b)				{WriteValue(Index, VAR_TYPE_SINT, sizeof(bool), &b);}
	void Write(uint32 Index, const sint8& sint)			{WriteValue(Index, VAR_TYPE_SINT, sizeof(sint8), &sint);}
	void Write(uint32 Index, const uint8& uint)			{WriteValue(Index, VAR_TYPE_UINT, sizeof(uint8), &uint);}
	void Write(uint32 Index, const sint16& sint)		{WriteValue(Index, VAR_TYPE_SINT, sizeof(sint16), &sint);}
	void Write(uint32 Index, const uint16& uint)		{WriteValue(Index, VAR_TYPE_UINT, sizeof(uint16), &uint);}
	void Write(uint32 Index, const sint32& sint)		{WriteValue(Index, VAR_TYPE_SINT, sizeof(sint32), &sint);}
	void Write(uint32 Index, const uint32& uint)		{WriteValue(Index, VAR_TYPE_UINT, sizeof(uint32), &uint);}
#ifndef WIN32
	// Note: gcc seams to use a 32 bit time_t, so we need a conversion here
	void Write(uint32 Index, const time_t& time)		{sint64 uint = time; WriteValue(Index, VAR_TYPE_UINT, sizeof(sint64), &uint);}
#endif
	void Write(uint32 Index, const sint64& sint)		{WriteValue(Index, VAR_TYPE_SINT, sizeof(sint64), &sint);}
	void Write(uint32 Index, const uint64& uint)		{WriteValue(Index, VAR_TYPE_UINT, sizeof(uint64), &uint);}

	void Write(uint32 Index, const float& val)			{WriteValue(Index, VAR_TYPE_DOUBLE, sizeof(float), &val);}
	void Write(uint32 Index, const double& val)			{WriteValue(Index, VAR_TYPE_DOUBLE, sizeof(double), &val);}

	void Write(uint32 Index, const std::string& str)	{WriteValue(Index, VAR_TYPE_ASCII, str.length(), str.c_str());}
	void Write(uint32 Index, const char* str)			{WriteValue(Index, VAR_TYPE_ASCII, strlen(str), str);}
	void Write(uint32 Index, const std::wstring& wstr, bool bUtf8 = false) {Write(Index, wstr.c_str(), wstr.length(), bUtf8);}
	void Write(uint32 Index, const wchar_t* wstr, size_t len, bool bUtf8 = false)
	{
		if (len == -1) len = wcslen(wstr);
		if (bUtf8) {
			char* pUtf8 = WCharToUtf8(wstr, len, &len);
			WriteValue(Index, VAR_TYPE_UTF8, len, pUtf8);
			free(pUtf8);
		} else
			WriteValue(Index, VAR_TYPE_UNICODE, len*sizeof(wchar_t), wstr);
	}

	void Write(uint32 Index, const std::vector<byte>& vec) {WriteValue(Index, VAR_TYPE_BYTES, vec.size(), vec.data());}


	//////////////////////////////////////////////////////////////////////////////////////////////
	// Common Read/Write

	void Finish();


protected:
	void					Assign(const CVariant& Variant);

	void					InitValue(EType Type, size_t Size, const void* Value, bool bTake = false);
	void					GetInt(size_t Size, void* Value) const;
	void					GetDouble(size_t Size, void* Value) const;

	static uint32			ReadHeader(const CBuffer* pPacket, EType* pType = NULL);
	static void				ToPacket(CBuffer* pPacket, EType Type, size_t uLength, const void* Value);

	enum EAccess : unsigned char
	{
		eReadWrite,
		eReadOnly,
		eWriteOnly,
		eDerived
	};

	struct SVariant
	{
		SVariant();
		~SVariant();

		void				Init(EType type, size_t size = 0, const void* payload = 0, bool bTake = false);
		SVariant*			Clone(bool Full = true) const;

		bool				IsRaw() const {return Type != VAR_TYPE_MAP && Type != VAR_TYPE_LIST && Type != VAR_TYPE_INDEX;}

		uint32				Count() const;

		const char*			Key(uint32 Index) const;
		CVariant&			At(const char* Name, size_t Len = -1) const;
		bool				Has(const char* Name) const;
		void				Remove(const char* Name);
		CVariant&			Insert(const char* Name, const CVariant& Variant);

		CVariant&			Append(const CVariant& Variant);
		CVariant&			At(uint32 Index) const;
		void				Remove(uint32 Index);
		
		uint32				Id(uint32 Index) const;
		bool				Has(uint32 Index) const;
		CVariant&			Insert(uint32 Index, const CVariant& Variant);

		void 				Alloc();
		void 				Free();

		EType				Type;
		uint32				Size;
		byte*				Payload;
		byte				Buffer[8];
		typedef _TVarIndex TIndex;
		mutable union UContainer
		{
			void*							Void;
			std::map<std::string, CVariant>*Map;
			std::vector<CVariant>*			List;
			std::map<TIndex, CVariant>*		Index;
			CBuffer*						Buffer;
		}									Container;
		std::map<std::string, CVariant>*	Map() const;
		std::vector<CVariant>*				List() const;
		std::map<TIndex, CVariant>*			IMap() const;
		void								MkPayload(CBuffer& Payload) const;
		EAccess				Access;

		mutable std::atomic<int> Refs;
	}*						m_Variant;

	static SVariant* 		Alloc();
	static void 			Free(SVariant* ptr);

	void					Attach(SVariant* Variant);
	const SVariant*			Val() const	{if(!m_Variant) ((CVariant*)this)->InitValue(VAR_TYPE_EMPTY,0,NULL); return m_Variant;}
	SVariant*				Val() {if(!m_Variant) InitValue(VAR_TYPE_EMPTY,0,NULL); else if(m_Variant->Refs > 1) Detach(); return m_Variant;}
	void					Detach();
};

__inline bool operator<(const CVariant &L, const CVariant &R) {return L.Compare(R) > 0;}

//////////////////////////////////////////////////////////////////////////////////////////////
// 

void WritePacket(const std::string& Name, const CVariant& Packet, CBuffer& Buffer);
void ReadPacket(const CBuffer& Buffer, std::string& Name, CVariant& Packet);
//bool StreamPacket(CBuffer& Buffer, std::string& Name, CVariant& Packet);

void TestVariant();