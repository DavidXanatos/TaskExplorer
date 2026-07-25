#pragma once

#include "Types.h"

class CBuffer
{
public:
	CBuffer()													{Init();}
	CBuffer(const CBuffer& Buffer);
	CBuffer(size_t uLength, bool bUsed = false);
	CBuffer(void* pBuffer, const size_t uSize, bool bDerived = false);
	CBuffer(const void* pBuffer, const size_t uSize, bool bDerived = false);
	virtual ~CBuffer();

	virtual void	Clear();

	CBuffer& operator=(const CBuffer &Buffer);

#ifdef USING_QT
	CBuffer(const QByteArray& Buffer, bool bDerive = false)		
	{
		Init(); 
		if(bDerive) 
			SetBuffer((byte*)Buffer.data(), Buffer.size(), true); 
		else
			CopyBuffer((byte*)Buffer.data(), Buffer.size()); 
	}
	virtual QByteArray	ToByteArray() const						{return QByteArray((char*)GetBuffer(), (int)GetSize());}
#endif

	virtual void	AllocBuffer(size_t uSize, bool bUsed = false, bool bFixed = true);

	virtual void	SetBuffer(void* pBuffer, size_t uSize, bool bDerived = false);
	virtual void	CopyBuffer(void* pBuffer, size_t uSize);
	virtual byte*	GetBuffer(bool bDetatch = false);
	virtual const byte*	GetBuffer() const						{return m_pBuffer;}

	virtual size_t	GetSize() const								{return m_uSize;}
	virtual bool	SetSize(size_t uSize, bool bExpend = false, size_t uPreAlloc = 0);

	virtual bool	IsValid() const								{return m_pBuffer != NULL;}
	virtual size_t	GetLength() const							{return m_uLength;}

	virtual int		Compare(const CBuffer& Buffer) const;
	virtual bool	CompareTo(const CBuffer& Buffer) const		{return Compare(Buffer) == 0;}

#ifdef HAS_ZLIB
	virtual bool	Pack();
	virtual bool	Unpack();
#endif

	///////////////////////////////
	// Read Write

	virtual void	SetReadOnly(bool bReadOnly = true)			{m_bReadOnly = bReadOnly;}
	virtual bool	IsReadOnly() const							{return m_bReadOnly;}
	virtual bool	IsDerived()	const							{return m_eType == eDerived;}

	virtual size_t	GetPosition() const							{return m_uPosition;}
	virtual bool	SetPosition(size_t uPosition) const;
	virtual byte*	GetData(size_t uLength = -1) const;
	virtual byte*	ReadData(size_t uLength = -1) const;
#ifdef USING_QT
	virtual QByteArray ReadQData(size_t uLength = -1) const		{return QByteArray((char*)ReadData(uLength), (int)(uLength == -1 ? GetSizeLeft() : uLength));}
#endif
	virtual byte*	SetData(const void* pData, size_t uLength);
	virtual void	WriteData(const void* pData, size_t uLength);
#ifdef USING_QT
	virtual void	WriteQData(const QByteArray& Buffer, int Size = 0){if(Size == 0) Size = Buffer.size(); ASSERT(Size <= Buffer.size()); WriteData(Buffer.data(),Size);}
#endif
	virtual size_t	GetSizeLeft() const							{ASSERT(m_uSize >= m_uPosition); return m_uSize - m_uPosition;}
	virtual size_t	GetLengthLeft() const						{ASSERT(m_uLength >= m_uSize); return m_uLength - m_uSize;}

	virtual byte*	GetData(size_t uOffset, size_t uLength) const;
	virtual byte*	SetData(size_t uOffset, void* pData, size_t uLength);
	virtual byte*	InsertData(size_t uOffset, void* pData, size_t uLength);
	virtual byte*	ReplaceData(size_t uOffset, size_t uOldLength, void* pData, size_t uNewLength);
	virtual bool	RemoveData(size_t uOffset, size_t uLength);
	virtual bool	AppendData(const void* pData, size_t uLength);
	virtual bool	ShiftData(size_t uOffset);

	template <class T>
	void			WriteValue(T Data, bool toBigEndian = false)
	{
		if(toBigEndian)
		{
			for (size_t i = sizeof(T); i > 0; i--)
				WriteData(((byte*)&Data) + i - 1, 1);
		}
		else
			WriteData(&Data, sizeof(T));
	}
	template <class T>
	T				ReadValue(bool fromBigEndian = false) const
	{
		if(fromBigEndian)
		{
			T Data;
			for (size_t i = sizeof(T); i > 0; i--)
			{
				byte* pByte = ReadData(1);
				ASSERT(pByte);
				if(pByte)
					*(((byte*)&Data) + i - 1) = *pByte;
				else
					return 0;
			}
			return Data;
		}
		else
		{
			byte* pData = ReadData(sizeof(T)); 
			ASSERT(pData);
			return pData ? *((T*)pData) : 0;
		}
	}

	enum EStrSet
	{
		eAscii,
		eUtf8_BOM,
		eUtf8
	};
	enum EStrLen
	{
		e8Bit,
		e16Bit,
		e32Bit,
	};

	void			WriteString(const std::wstring& String, EStrSet Set = eUtf8, EStrLen Len = e16Bit);
	void			WriteString(const std::string& String, EStrSet Set = eUtf8, EStrLen Len = e16Bit);
	std::wstring	ReadString(EStrSet Set = eUtf8, EStrLen Len = e16Bit) const;
	std::wstring	ReadString(EStrSet Set, size_t uRawLength) const;

protected:
	void			Init();
	bool			PrepareWrite(size_t uOffset, size_t uLength);

	byte*			m_pBuffer;
	size_t			m_uSize; // size of data
	size_t			m_uLength; // Length of the allocated memory

	mutable size_t	m_uPosition; // read/write position

	enum EType
	{
		eNormal,
		eFixed,
		eDerived,
	}				m_eType;
	bool			m_bReadOnly;
};

void WStrToAscii(std::string& dest, const std::wstring& src);
void WStrToUtf8(std::string& dest, const std::wstring& src);
char* WCharToUtf8(const wchar_t* str, size_t len, size_t* out_len);
void AsciiToWStr(std::wstring& dest, const std::string& src);
void Utf8ToWStr(std::wstring& dest, const std::string& src);

std::wstring ToHex(const byte* Data, size_t uSize);
CBuffer FromHex(std::wstring Hex);