#pragma once
#include "Variant.h"

//
// We need to extend CVariant with a bunch of helper functions,
// we do that by deriving it and wher needed hard casting CVariant to XVariant
// hence booth must have the same representation in memory
// that is XVariant must not have any members
//

class XVariant : public CVariant
{
public:

	//
	// Pass through contructors
	//

	XVariant(EType Type = VAR_TYPE_EMPTY) : CVariant(Type) {}
	XVariant(const CVariant& Variant) : CVariant(Variant) {}

	template<typename T>
	XVariant(const T& val)								: CVariant(val) {}

	XVariant(const std::wstring& wstr, bool bUtf8 = false) : CVariant(wstr, bUtf8) {}
	XVariant(const wchar_t* wstr, size_t len, bool bUtf8 = false)	: CVariant(wstr, len, bUtf8) {}

	//
	// Qt Types
	//

	XVariant(const QString& String, bool bUtf8 = false) : CVariant(String.toStdWString(), bUtf8) {}
	QString AsQStr() const
	{
		EType Type = GetType();
		if (Type == VAR_TYPE_ASCII || Type == VAR_TYPE_BYTES || Type == VAR_TYPE_UTF8 || Type == VAR_TYPE_UNICODE)
			return ToQString();
		else if (Type == VAR_TYPE_DOUBLE)
			return QString::number(To<double>());
		else if (Type == VAR_TYPE_SINT)
			return QString::number(To<sint32>());
		else if (Type == VAR_TYPE_UINT)
			return QString::number(To<uint32>());
		else
			return QString();
	}

	XVariant(const QByteArray& Bytes)					: CVariant((byte*)Bytes.data(), Bytes.size(), VAR_TYPE_BYTES) {}
	QByteArray AsQBytes() const							{const SVariant* Variant = Val(); if (Variant->IsRaw()) { return QByteArray((char*)Variant->Payload, Variant->Size); } return QByteArray();}

	//
	// Overwriten getters/setters
	//

	XVariant&				At(const char* Name)		{return *(XVariant*)&CVariant::At(Name);}
	const XVariant&			At(const char* Name) const	{return *(const XVariant*)&CVariant::At(Name);}
	XVariant&				At(uint32 Index)			{return *(XVariant*)&CVariant::At(Index);}
	const XVariant&			At(uint32 Index) const		{return *(const XVariant*)&CVariant::At(Index);}

	XVariant				Get(const char* Name, const CVariant& Default = CVariant()) const {return CVariant::Get(Name, Default);}
	XVariant				Get(uint32 Index, const CVariant& Default = CVariant()) const {return CVariant::Get(Index, Default);}

	template<typename T>
	XVariant(const QList<T>& List) : CVariant() {
		BeginList();
		foreach(const QString& Item, List)
			Write(Item);
		Finish();
	}

	XVariant(const QStringList& List) : CVariant() {
		BeginList();
		foreach(const QString& Item, List)
			Write(Item.toStdWString());
		Finish();
	}

	template<typename T>
	QSet<T>				AsQSet() const {
		QSet<T> List;
		ReadRawList([&](const CVariant& vData) {
			List.insert(vData.To<T>());
		});
		return List;
	}

	QSet<QString>		AsQSet() const {
		QSet<QString> List;
		ReadRawList([&](const CVariant& vData) {
			const XVariant& Data = *(XVariant*)&vData;
			List.insert(Data.AsQStr());
		});
		return List;
	}

	template<typename T>
	QList<T>				AsQList() const {
		QList<T> List;
		ReadRawList([&](const CVariant& vData) {
			List.append(vData.To<T>());
		});
		return List;
	}

	QStringList				AsQList() const {
		QStringList List;
		ReadRawList([&](const CVariant& vData) {
			const XVariant& Data = *(XVariant*)&vData;
			List.append(Data.AsQStr());
		});
		return List;
	}

	XVariant& operator[](const char* Name)				{return *(XVariant*)&CVariant::At(Name);}
	const XVariant& operator[](const char* Name) const	{return *(const XVariant*)&CVariant::At(Name);}
	XVariant& operator[](uint32 Index)					{return *(XVariant*)&CVariant::At(Index);}
	const XVariant& operator[](uint32 Index)	const	{return *(const XVariant*)&CVariant::At(Index);}

	XVariant Find(const char* Name) const {CVariant Data; CVariant::Find(Name, Data); return Data;}
	XVariant Find(uint32 Index) const {CVariant Data; CVariant::Find(Index, Data); return Data;}

	//
	// Qt Conversion
	//

	enum QFlags
	{
		eNone = 0,
		eUtf8 = 1,
	};

	bool					FromQVariant(const QVariant& qVariant, QFlags Flags = eNone);
	QVariant				ToQVariant() const;

protected:
	QString					ToQString() const;
};