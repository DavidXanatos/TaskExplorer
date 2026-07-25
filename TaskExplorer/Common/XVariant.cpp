#include "stdafx.h"
#include "XVariant.h"
#include "Exception.h"


void XVariantTest()
{

	XVariant test;
	test["test"] = 123;

	XVariant num = test["test"];
	auto snum = test["test"].AsQStr();
}

QString XVariant::ToQString() const
{
	const SVariant* Variant = Val();
	if (Variant->Type == VAR_TYPE_UNICODE)
		return QString::fromWCharArray((wchar_t*)Variant->Payload, Variant->Size / sizeof(wchar_t));
	else if (Variant->Type == VAR_TYPE_ASCII || Variant->Type == VAR_TYPE_BYTES)
		return QString::fromLatin1((char*)Variant->Payload, Variant->Size);
	else if(Variant->Type == VAR_TYPE_UTF8)
		return QString::fromUtf8((char*)Variant->Payload, Variant->Size);
	else if(Variant->Type != VAR_TYPE_EMPTY)
		throw CException(L"QString Value can not be pulled form a incomatible type");
	return QString();
}

bool XVariant::FromQVariant(const QVariant& qVariant, QFlags Flags)
{
	Clear();
	SVariant* Variant = Val();

	switch(qVariant.type())
	{
		case QVariant::Map:
		{
			QVariantMap Map = qVariant.toMap();
			Variant->Type = VAR_TYPE_MAP;
			Variant->Container.Map = new std::map<std::string, CVariant>;
			for(QVariantMap::iterator I = Map.begin(); I != Map.end(); ++I)
			{
				std::string Name = I.key().toStdString();

				XVariant Temp;
				Temp.FromQVariant(I.value(), Flags);
				Variant->Insert(Name.c_str(), Temp);
			}
			break;
		}
		case QVariant::List:
		case QVariant::StringList:
		{
			QVariantList List = qVariant.toList();
			Variant->Type = VAR_TYPE_LIST;
			Variant->Container.List = new std::vector<CVariant>;
			for(QVariantList::iterator I = List.begin(); I != List.end(); ++I)
			{
				XVariant Temp;
				Temp.FromQVariant(*I, Flags);
				Variant->Append(Temp);
			}
			break;
		}
		case QVariant::Hash:
		{
			QVariantHash Map = qVariant.toHash();
			Variant->Type = VAR_TYPE_MAP;
			Variant->Container.Map = new std::map<std::string, CVariant>;
			for(QVariantHash::iterator I = Map.begin(); I != Map.end(); ++I)
			{
				std::string Name = I.key().toStdString();
				if (Name.length() > 4) {
					Q_ASSERT(0);
					Name.resize(4);
				}
				uint32 Index = 0;
				memcpy(&Index, Name.c_str(), Name.size());

				XVariant Temp;
				Temp.FromQVariant(I.value(), Flags);
				Variant->Insert(Index, Temp);
			}
			break;
		}

		case QVariant::Bool:
		{
			bool Value = qVariant.toBool();
			Variant->Init(VAR_TYPE_UINT, sizeof(Value), &Value);
			break;
		}
		//case QVariant::Char:
		case QVariant::Int:
		{
			sint32 Value = qVariant.toInt();
			Variant->Init(VAR_TYPE_SINT, sizeof (sint32), &Value);
			break;
		}
		case QVariant::UInt:
		{
			uint32 Value = qVariant.toUInt();
			Variant->Init(VAR_TYPE_UINT, sizeof (sint32), &Value);
			break;
		}
		case QVariant::LongLong:
		{
			sint64 Value = qVariant.toLongLong();
			Variant->Init(VAR_TYPE_SINT, sizeof(Value), &Value);
			break;
		}
		case QVariant::ULongLong:
		{
			uint64 Value = qVariant.toULongLong();
			Variant->Init(VAR_TYPE_UINT, sizeof(Value), &Value);
			break;
		}
		case QVariant::Double:
		{
			double Value = qVariant.toDouble();
			Variant->Init(VAR_TYPE_DOUBLE, sizeof(Value), &Value);
			break;
		}
		case QVariant::String:
		{
			if (Flags & eUtf8) {
				QByteArray Value = qVariant.toString().toUtf8();
				Variant->Init(VAR_TYPE_UTF8, Value.size(), Value.data());
			}
			else {
				std::wstring Value = qVariant.toString().toStdWString();
				Variant->Init(VAR_TYPE_UNICODE, Value.size()*sizeof(wchar_t), Value.data());
			}
			break;
		}
		case QVariant::ByteArray:
		{
			QByteArray Value = qVariant.toByteArray();
			Variant->Init(VAR_TYPE_BYTES, Value.size(), Value.data());
			break;
		}
		//case QVariant::BitArray:
		//case QVariant::Date:
		//case QVariant::Time:
		//case QVariant::DateTime:
		//case QVariant::Url:
		default:
			ASSERT(0);
			return false;
	}
	return true;
}

QVariant XVariant::ToQVariant() const
{
	const SVariant* Variant = Val();

	switch(Variant->Type)
	{
		case VAR_TYPE_MAP:
		{
			QVariantMap Map;
			std::map<std::string, CVariant>* pMap = m_Variant->Map();
			for (auto I = pMap->begin(); I != pMap->end(); ++I)
			{
				QString Name = QString::fromStdString(I->first);
				Map[Name] = ((XVariant*)&I->second)->ToQVariant();
			}
			return Map;
		}
		case VAR_TYPE_LIST:
		{
			QVariantList List;
			std::vector<CVariant>* pList = m_Variant->List();
			for (auto I = pList->begin(); I != pList->end(); ++I)
				List.append(((XVariant*)&*I)->ToQVariant());
			return List;
		}
		case VAR_TYPE_INDEX:
		{
			QVariantHash Map;
			std::map<SVariant::TIndex, CVariant>* pMap = m_Variant->IMap();
			for (auto I = pMap->begin(); I != pMap->end(); ++I)
			{
				QString Name = QString::fromStdString(std::string(I->first.c, 4));
				Map[Name] = ((XVariant*)&I->second)->ToQVariant();
			}
			return Map;
		}

		case VAR_TYPE_BYTES:
			return QByteArray((char*)Variant->Payload, Variant->Size);

		case VAR_TYPE_UNICODE:
			return QString::fromWCharArray((wchar_t*)Variant->Payload, Variant->Size / sizeof(wchar_t));
		case VAR_TYPE_UTF8:
			return QString::fromUtf8((char*)Variant->Payload, Variant->Size);
		case VAR_TYPE_ASCII:
			return QString::fromLatin1((char*)Variant->Payload, Variant->Size);

		case VAR_TYPE_SINT:
			if(Variant->Size <= sizeof(sint8))
				return sint8(*this);
			else if(Variant->Size <= sizeof(sint16))
				return sint16(*this);
			else if(Variant->Size <= sizeof(sint32))
				return sint32(*this);
			else if(Variant->Size <= sizeof(uint64))
				return sint64(*this);
			else
				return QByteArray((char*)Variant->Payload, Variant->Size);
		case VAR_TYPE_UINT:
			if(Variant->Size <= sizeof(uint8))
				return uint8(*this);
			else if(Variant->Size <= sizeof(uint16))
				return uint16(*this);
			else if(Variant->Size <= sizeof(uint32))
				return uint32(*this);
			else if(Variant->Size <= sizeof(uint64))
				return uint64(*this);
			else
				return QByteArray((char*)Variant->Payload, Variant->Size);

		case VAR_TYPE_DOUBLE:
			return double(*this);

		case VAR_TYPE_EMPTY:
		default:
			return QVariant();
	}
}