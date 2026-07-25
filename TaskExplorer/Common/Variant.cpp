#include "stdafx.h"
#include "Variant.h"
//#include "../../zlib/zlib.h"
#include "Exception.h"
//#include "Strings.h"

CVariant::CVariant(EType Type)
{
	if(Type != VAR_TYPE_EMPTY)
		InitValue(Type, 0, NULL);
	else
		m_Variant = NULL;
}

CVariant::CVariant(const CVariant& Variant)
{
	m_Variant = NULL;
	Assign(Variant);
}

CVariant::~CVariant()
{
	Clear();
}

CVariant::SVariant* CVariant::Alloc()
{
	return new SVariant;
}

void CVariant::Free(SVariant* ptr)
{
	delete ptr;
}

void CVariant::Assign(const CVariant& Variant)
{
	const SVariant* VariantVal = Variant.Val();
	if(m_Variant && m_Variant->Access != eReadWrite)
		throw CException(L"variant access violation; Assign");
	if(VariantVal->Access == eDerived)
	{
		// Note: We can not asign buffer derivations, as the original variant may be removed at any time,
		//	those we must copy the buffer on accessey by value
		SVariant* VariantCopy = Alloc();
		VariantCopy->Type = VariantVal->Type;
		VariantCopy->Size = VariantVal->Size;
		VariantCopy->Alloc();
		memcpy(VariantCopy->Payload,VariantVal->Payload, VariantCopy->Size);
		VariantCopy->Access = eReadOnly;
		Attach(VariantCopy);
	}
	else
	{
		ASSERT(Variant.m_Variant);
		Attach(Variant.m_Variant);
	}
}

void CVariant::Clear()
{
	if(m_Variant && m_Variant->Refs.fetch_sub(1) == 1)
		Free(m_Variant);
	m_Variant = NULL;
}

CVariant CVariant::Clone(bool Full) const
{
	const SVariant* Variant = Val();

	CVariant NewVariant;
	NewVariant.Attach(Variant->Clone(Full));
	return NewVariant;
}

void CVariant::Attach(SVariant* Variant)
{
	if(m_Variant && m_Variant->Refs.fetch_sub(1) == 1)
		Free(m_Variant);
	m_Variant = Variant;
	m_Variant->Refs.fetch_add(1);
}

void CVariant::InitValue(EType Type, size_t Size, const void* Value, bool bTake)
{
	m_Variant = Alloc();
	m_Variant->Refs.fetch_add(1);

	m_Variant->Init(Type, Size, Value, bTake);
}

void CVariant::Detach()
{
	ASSERT(m_Variant);
	ASSERT(m_Variant->Refs > 1);
	SVariant* newVariant = m_Variant->Clone();
	if(m_Variant->Refs.fetch_sub(1) == 1)
		Free(m_Variant);
	m_Variant = newVariant;
	m_Variant->Refs.fetch_add(1);
}

void CVariant::Freeze()
{
	SVariant* Variant = Val();
	if(Variant->Access == eReadOnly)
		return;

	// Note: we must flly serialize and deserialize the variant in order to not store maps and std::list content multiple times in memory
	//	The CPU overhead should be howeever minimal as we imply parse on read for the dictionaries, 
	//	and we usually dont read the variant afterwards but put in on the socket ot save it to a file
	CBuffer Packet;
	ToPacket(&Packet);
	Packet.SetPosition(0);
	FromPacket(&Packet);
}

void CVariant::Unfreeze()
{
	SVariant* Variant = Val();
	if(Variant->Access == eReadWrite)
		return;

	Attach(Variant->Clone());
}

bool CVariant::IsFrozen() const
{
	const SVariant* Variant = Val();
	return Variant->Access != eReadWrite;
}

void CVariant::FromPacket(const CBuffer* pPacket, bool bDerived)
{
	Clear();
	SVariant* Variant = Val();

	Variant->Size = ReadHeader(pPacket, &Variant->Type);
	
	if(bDerived)
	{
		Variant->Access = eDerived;
		Variant->Payload = pPacket->ReadData(Variant->Size);
	}
	else
	{
		Variant->Access = eReadOnly;
//#ifdef HAS_ZLIB // deprecated
//		if((Type & VAR_LEN32) == 0) // is it packed
//		{
//			byte* pData = pPacket->ReadData(Variant->Size);
//
//			uLongf newsize = Variant->Size*10+300;
//			uLongf unpackedsize = 0;
//			int result = 0;
//			CBuffer Buffer;
//			do
//			{
//				Buffer.AllocBuffer(newsize, true);
//				unpackedsize = newsize;
//				result = uncompress(Buffer.GetBuffer(),&unpackedsize,pData,Variant->Size);
//				newsize *= 2; // size for the next try if needed
//			}
//			while (result == Z_BUF_ERROR && newsize < Max(MB2B(16), Variant->Size*100));	// do not allow the unzip buffer to grow infinetly,
//																					// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
//			if (result == Z_OK)
//			{
//				Variant->Size = unpackedsize;
//				Variant->Alloc();
//				memcpy(Variant->Payload, Buffer.GetBuffer(), Variant->Size);
//			}
//			else
//				return;
//		}
//		else
//#endif
		{
			Variant->Alloc();
			memcpy(Variant->Payload, pPacket->ReadData(Variant->Size), Variant->Size);
		}
	}
}

uint32 CVariant::ReadHeader(const CBuffer* pPacket, EType* pType)
{
	byte Type = pPacket->ReadValue<uint8>();
	ASSERT(Type != 0xFF);

	uint32 Size;
	if (Type & VAR_LEN_FIELD)
	{
		if ((Type & VAR_LEN_MASK) == VAR_LEN8)
			Size = pPacket->ReadValue<uint8>();
		else if ((Type & VAR_LEN_MASK) == VAR_LEN16)
			Size = pPacket->ReadValue<uint16>();
		else //if ((Type & VAR_LEN_MASK) == VAR_LEN32) // VAR_LEN32 or packed in eider case teh length is 32 bit 
			Size = pPacket->ReadValue<uint32>();
	}
	else if ((Type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		Size = 0;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN8)
		Size = 1;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN16)
		Size = 2;
	else if ((Type & VAR_LEN_MASK) == VAR_LEN32)
		Size = 4;
	else //if ((Type & VAR_LEN_MASK) == VAR_LEN64)
		Size = 8;

	if(pPacket->GetSizeLeft() < Size)
		throw CException(L"incomplete variant");

	if(pType) *pType = (EType)(Type & VAR_TYPE_MASK); // clear size flags

	return Size;
}

void CVariant::ToPacket(CBuffer* pPacket/*, bool bPack*/) const
{
	const SVariant* Variant = Val();

	CBuffer Payload;
	Variant->MkPayload(Payload);

//#ifdef HAS_ZLIB // deprecated
//	if(bPack)
//	{
//		ASSERT(Payload.GetSize() + 300 < 0xFFFFFFFF);
//		uLongf newsize = (uLongf)(Payload.GetSize() + 300);
//		CBuffer Buffer(newsize);
//		int result = compress2(Buffer.GetBuffer(),&newsize,Payload.GetBuffer(),(uLongf)Payload.GetSize(),Z_BEST_COMPRESSION);
//
//		if (result == Z_OK && Payload.GetSize() > newsize) // does the compression helped?
//		{
//			ASSERT(Variant->Type != 0xFF);
//			pPacket->WriteValue<uint8>(Variant->Type | VAR_LEN64);
//			pPacket->WriteValue<uint32>(newsize);
//			pPacket->WriteData(Buffer.GetBuffer(), newsize);
//			return;
//		}
//	}
//#endif
	
	ToPacket(pPacket, Variant->Type, Payload.GetSize(), Payload.GetBuffer());
}

void CVariant::ToPacket(CBuffer* pPacket, EType Type, size_t uLength, const void* Value)
{
	ASSERT(uLength < 0xFFFFFFFF);

	ASSERT((Type & ~VAR_TYPE_MASK) == 0);

	if(Type == VAR_TYPE_EMPTY)
		pPacket->WriteValue<uint8>(Type);
	else if (uLength == 1)
		pPacket->WriteValue<uint8>(Type | VAR_LEN8);
	else if (uLength == 2)
		pPacket->WriteValue<uint8>(Type | VAR_LEN16);
	else if (uLength == 4)
		pPacket->WriteValue<uint8>(Type | VAR_LEN32);
	else if (uLength == 8)
		pPacket->WriteValue<uint8>(Type | VAR_LEN64);
	else
	{
		if (uLength > USHRT_MAX) {
			pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN32);
			pPacket->WriteValue<uint32>((uint32)uLength);
		}
		else if (uLength > UCHAR_MAX) {
			pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN16);
			pPacket->WriteValue<uint16>((uint32)uLength);
		}
		else {
			pPacket->WriteValue<uint8>(Type | VAR_LEN_FIELD | VAR_LEN8);
			pPacket->WriteValue<uint8>((uint32)uLength);
		}
	}

	pPacket->WriteData(Value, uLength);
}

uint32 CVariant::Count() const
{
	const SVariant* Variant = Val();
	return Variant->Count();
}

bool CVariant::IsMap() const
{
	const SVariant* Variant = Val();
	return Variant->Type == VAR_TYPE_MAP;
}

const char* CVariant::Key(uint32 Index) const
{
	const SVariant* Variant = Val();
	return Variant->Key(Index);
}

std::wstring CVariant::WKey(uint32 Index) const
{
	std::wstring Name; 
	AsciiToWStr(Name, Key(Index));
	return Name;
}

CVariant& CVariant::At(const char* Name, size_t Len)
{
	SVariant* Variant = Val();
	if (Variant->Access != eReadWrite && !Variant->Has(Name))
		throw CException(L"variant access violation; Map Member: %.*S is not present", Len != -1 ? Len : strlen(Name), Name);
	if(Variant->Type != VAR_TYPE_MAP)
		Variant->Init(VAR_TYPE_MAP);
	return Variant->At(Name, Len);
}

CVariant CVariant::Get(const char* Name, const CVariant& Default) const
{
	if(!Has(Name))
		return Default;
	return At(Name);
}

const CVariant& CVariant::At(const char* Name, size_t Len) const
{
	const SVariant* Variant = Val();
	return Variant->At(Name, Len);
}

bool CVariant::Has(const char* Name) const
{
	const SVariant* Variant = Val();
	return Variant->Has(Name);
}

void CVariant::Remove(const char* Name)
{
	SVariant* Variant = Val();
	return Variant->Remove(Name);
}

bool CVariant::IsList() const
{
	const SVariant* Variant = Val();
	return Variant->Type == VAR_TYPE_LIST;
}

void CVariant::Insert(const char* Name, const CVariant& variant)
{
	SVariant* Variant = Val();
	if(Variant->Access != eReadWrite)
		throw CException(L"variant access violation; Map Insert");
	if(Variant->Type != VAR_TYPE_MAP)
		Variant->Init(VAR_TYPE_MAP);
	Variant->Insert(Name, variant);
}

void CVariant::Append(const CVariant& variant)
{
	SVariant* Variant = Val();
	if(Variant->Access != eReadWrite)
		throw CException(L"variant access violation; List Append");
	if(Variant->Type != VAR_TYPE_LIST)
		Variant->Init(VAR_TYPE_LIST);
	Variant->Append(variant);
}

const CVariant& CVariant::At(uint32 Index) const
{
	const SVariant* Variant = Val();
	return Variant->At(Index);
}

void CVariant::Remove(uint32 Index)
{
	SVariant* Variant = Val();
	return Variant->Remove(Index);
}

bool CVariant::IsIndex() const
{
	const SVariant* Variant = Val();
	return Variant->Type == VAR_TYPE_INDEX;
}

uint32 CVariant::Id(uint32 Index) const
{
	const SVariant* Variant = Val();
	return Variant->Id(Index);
}

CVariant& CVariant::At(uint32 Index)
{
	SVariant* Variant = Val();
	if(Variant->Access != eReadWrite && !Variant->Has(Index))
		throw CException(L"variant access violation; index Member: 0x%08X is not present", Index);
	if(Variant->Type != VAR_TYPE_INDEX && Variant->Type != VAR_TYPE_LIST)
		Variant->Init(VAR_TYPE_INDEX);
	return Variant->At(Index);
}

CVariant CVariant::Get(uint32 Index, const CVariant& Default) const
{
	if(!Has(Index))
		return Default;
	return At(Index);
}

bool CVariant::Has(uint32 Index) const
{
	const SVariant* Variant = Val();
	return Variant->Has(Index);
}

void CVariant::Insert(uint32 Index, const CVariant& variant)
{
	SVariant* Variant = Val();
	if(Variant->Access != eReadWrite)
		throw CException(L"variant access violation; Index Insert");
	if(Variant->Type != VAR_TYPE_INDEX)
		Variant->Init(VAR_TYPE_INDEX);
	Variant->Insert(Index, variant);
}

void CVariant::Merge(const CVariant& Variant)
{
	if(!IsValid()) // if this valiant is void we can merge with anything
		Assign(Variant);
	else if(Variant.IsList() && IsList())
	{
		std::vector<CVariant>* pList = Variant.m_Variant->List();
		for (auto I = pList->begin(); I != pList->end(); ++I)
			Append(*I);
	}
	else if(Variant.IsMap() && IsMap())
	{
		std::map<std::string, CVariant>* pMap = Variant.m_Variant->Map();
		for (auto I = pMap->begin(); I != pMap->end(); ++I)
			Insert(I->first.c_str(), I->second);
	}
	else if (Variant.IsIndex() && IsIndex())
	{
		std::map<SVariant::TIndex, CVariant>* pMap = Variant.m_Variant->IMap();
		for (auto I = pMap->begin(); I != pMap->end(); ++I)
			Insert(I->first.u, I->second);
	}
	else
		throw CException(L"Variant could not be merged, type mismatch");
}


void CVariant::GetInt(size_t Size, void* Value) const
{
	const SVariant* Variant = Val();
	if(Variant->Type == VAR_TYPE_EMPTY)
	{
		memset(Value, 0, Size);
		return;
	}

	if(Variant->Type != VAR_TYPE_SINT && Variant->Type != VAR_TYPE_UINT)
		throw CException(L"Int Value can not be pulled form a incomatible type");
	if (Size < Variant->Size)
	{
		for (size_t i = Size; i < Variant->Size; i++) {
			if (Variant->Payload[i] != 0)
				throw CException(L"Int Value pulled is shorter than int value present");
		}
	}

	if (Size <= Variant->Size)
		memcpy(Value, Variant->Payload, Size);
	else {
		memcpy(Value, Variant->Payload, Variant->Size);
		memset((byte*)Value + Variant->Size, 0, Size - Variant->Size);
	}
}

void CVariant::GetDouble(size_t Size, void* Value) const
{
	const SVariant* Variant = Val();

	if(Variant->Type != VAR_TYPE_DOUBLE)
		throw CException(L"double Value can not be pulled form a incomatible type");
	if(Size != Variant->Size)
		throw CException(L"double Value pulled is shorter than int value present");

	memcpy(Value, Variant->Payload, Size);
}

std::string CVariant::ToString() const
{
	const SVariant* Variant = Val();
	std::string str;
	if(Variant->Type == VAR_TYPE_ASCII || Variant->Type == VAR_TYPE_BYTES)
		str = std::string((char*)Variant->Payload, Variant->Size);
	else if(Variant->Type == VAR_TYPE_UTF8 || Variant->Type == VAR_TYPE_UNICODE)
	{
		std::wstring wstr = ToWString();
		WStrToAscii(str, wstr);
	}
	else if(Variant->Type != VAR_TYPE_EMPTY)
		throw CException(L"std::string Value can not be pulled form a incomatible type");
	return str;
}

std::wstring CVariant::ToWString() const
{
	const SVariant* Variant = Val();
	std::wstring wstr;
	if(Variant->Type == VAR_TYPE_UNICODE)
		wstr = std::wstring((wchar_t*)Variant->Payload, Variant->Size/sizeof(wchar_t));
	else if(Variant->Type == VAR_TYPE_ASCII || Variant->Type == VAR_TYPE_BYTES)
	{
		std::string str = std::string((char*)Variant->Payload, Variant->Size);
		AsciiToWStr(wstr, str);
	}
	else if(Variant->Type == VAR_TYPE_UTF8)
	{
		std::string str = std::string((char*)Variant->Payload, Variant->Size);
		Utf8ToWStr(wstr, str);
	}
	else if(Variant->Type != VAR_TYPE_EMPTY)
		throw CException(L"std::string Value can not be pulled form a incomatible type");
	return wstr;
}

CVariant::EType CVariant::GetType() const
{
	const SVariant* Variant = Val();
	return Variant->Type;
}

bool CVariant::IsValid() const
{
	const SVariant* Variant = Val();
	return Variant->Type != VAR_TYPE_EMPTY;
}

int	CVariant::Compare(const CVariant &R) const
{
	// we compare only actual payload, content not the declared type or auxyliary informations
	CBuffer l;
	Val()->MkPayload(l);
	CBuffer r;
	R.Val()->MkPayload(r);
	return l.Compare(r); 
}

///////////////////////////////////////////////////////////////////////////////////////////
// Internal Stuff

CVariant::SVariant::SVariant()
{
	Type = VAR_TYPE_EMPTY;
	Size = 0;
	Payload = NULL;
	Access = eReadWrite;
	Refs = 0;
	Container.Void = NULL;
}

CVariant::SVariant::~SVariant()
{
	if (Access == eWriteOnly)
		delete Container.Buffer;
	else {
		switch (Type)
		{
		case VAR_TYPE_MAP:		delete Container.Map; break;
		case VAR_TYPE_LIST:		delete Container.List; break;
		case VAR_TYPE_INDEX:	delete Container.Index; break;
		}
	}
	if (Access != eDerived)
		Free();
}

void CVariant::SVariant::Alloc()
{
	ASSERT(Payload == NULL);
	if (Size <= sizeof(Buffer))
		Payload = Buffer;
	Payload = (byte*)malloc(Size);
}

void CVariant::SVariant::Free()
{
	if (Payload != Buffer)
		free(Payload);
}

void CVariant::SVariant::Init(EType type, size_t size, const void* payload, bool bTake)
{
	ASSERT(Access == eReadWrite);
	ASSERT(Type == VAR_TYPE_EMPTY);
	ASSERT(size < 0xFFFFFFFF); // Note: one single variant can not be bigegr than 4 GB

	Type = type;
	Size = (uint32)size;
	if(bTake)
		Payload = (byte*)payload;
	else if(Size)
	{
		Alloc();
		if(payload)
			memcpy(Payload, payload, Size);
		else
			memset(Payload, 0, Size);
	}
	else
		Payload = NULL;

	switch(Type)
	{
		case VAR_TYPE_MAP:		Container.Map = new std::map<std::string, CVariant>;	break;
		case VAR_TYPE_LIST:		Container.List = new std::vector<CVariant>;				break;
		case VAR_TYPE_INDEX:	Container.Index = new std::map<TIndex, CVariant>;		break;
	}
}

std::map<std::string, CVariant>* CVariant::SVariant::Map() const
{
	if (Access == eWriteOnly) 
		throw new CException(L"Variant is being writen to");
	if(Container.Map == NULL)
	{
		CBuffer Packet(Payload, Size, true);
		Container.Map = new std::map<std::string, CVariant>;
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			uint8 Len = Packet.ReadValue<uint8>();
			if(Packet.GetSizeLeft() < Len)
				throw CException(L"invalid variant name");
			std::string Name = std::string((char*)Packet.ReadData(Len), Len);

			CVariant Temp;
			Temp.FromPacket(&Packet,true);

			// Note: this wil fail if the entry is already in the std::map
			Container.Map->insert(std::map<std::string, CVariant>::value_type(Name.c_str(), Temp));
		}
	}
	return Container.Map;
}

std::vector<CVariant>* CVariant::SVariant::List() const
{
	if (Access == eWriteOnly) 
		throw new CException(L"Variant is being writen to");
	if(Container.List == NULL)
	{
		CBuffer Packet(Payload, Size, true);
		Container.List = new std::vector<CVariant>;
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			CVariant Temp;
			Temp.FromPacket(&Packet,true);

			Container.List->push_back(Temp);
		}
	}
	return Container.List;
}

std::map<CVariant::SVariant::TIndex, CVariant>* CVariant::SVariant::IMap() const
{
	if (Access == eWriteOnly) 
		throw new CException(L"Variant is being writen to");
	if(Container.Index == NULL)
	{
		CBuffer Packet(Payload, Size, true);
		Container.Index = new std::map<TIndex, CVariant>;
		for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Size; )
		{
			uint32 Id = Packet.ReadValue<uint32>();
			
			CVariant Temp;
			Temp.FromPacket(&Packet,true);

			// Note: this wil fail if the entry is already in the std::map
			Container.Index->insert(std::map<TIndex, CVariant>::value_type(TIndex{ Id }, Temp));
		}
	}
	return Container.Index;
}

void CVariant::SVariant::MkPayload(CBuffer& Buffer) const
{
	if(Access == eReadWrite) // write the packet
	{
		switch(Type)
		{
			case VAR_TYPE_MAP:
			{
				std::map<std::string, CVariant>* pMap = Map();
				for (auto I = pMap->begin(); I != pMap->end(); ++I)
				{
					const std::string& sKey = I->first;
					ASSERT(sKey.length() < 0xFF);
					uint8 Len = (uint8)sKey.length();
					Buffer.WriteValue<uint8>(Len);
					Buffer.WriteData(sKey.c_str(), Len);

					I->second.ToPacket(&Buffer);
				}
				break;
			}
			case VAR_TYPE_LIST:
			{
				std::vector<CVariant>* pList = List();
				for (auto I = pList->begin(); I != pList->end(); ++I)
				{
					I->ToPacket(&Buffer);
				}
				break;
			}
			case VAR_TYPE_INDEX:
			{
				std::map<TIndex, CVariant>* pMap = IMap();
				for (auto I = pMap->begin(); I != pMap->end(); ++I)
				{
					Buffer.WriteValue<uint32>(I->first.u);

					I->second.ToPacket(&Buffer);
				}
				break;
			}
			default:
				Buffer.SetBuffer(Payload, Size, true);
		}
	}
	else
		Buffer.SetBuffer(Payload, Size, true);
}


CVariant::SVariant* CVariant::SVariant::Clone(bool Full) const
{
	SVariant* Variant = CVariant::Alloc();
	Variant->Type = Type;
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			if (Container.Map == NULL) break;
			Variant->Container.Map = new std::map<std::string, CVariant>;
			std::map<std::string, CVariant>* pMap = Map();
			for (auto I = pMap->begin(); I != pMap->end(); ++I)
				Variant->Insert(I->first.c_str(), Full ? I->second.Clone() : I->second);
			return Variant;
		}
		case VAR_TYPE_LIST:
		{
			if (Container.List == NULL) break;
			Variant->Container.List = new std::vector<CVariant>;
			std::vector<CVariant>* pList = List();
			for (auto I = pList->begin(); I != pList->end(); ++I)
				Variant->Append(Full ? I->Clone() : *I);
			return Variant;
		}
		case VAR_TYPE_INDEX:
		{
			if (Container.Index == NULL) break;
			Variant->Container.Index = new std::map<TIndex, CVariant>;
			std::map<TIndex, CVariant>* pMap = IMap();
			for (auto I = pMap->begin(); I != pMap->end(); ++I)
				Variant->Insert(I->first.u, Full ? I->second.Clone() : I->second);
			return Variant;
		}
	}
	if(Size > 0)
	{
		Variant->Size = Size;
		Variant->Alloc();
		memcpy(Variant->Payload, Payload, Variant->Size);
	}
	return Variant;
}

uint32 CVariant::SVariant::Count() const
{
	switch(Type)
	{
		case VAR_TYPE_MAP:		return (uint32)Map()->size();
		case VAR_TYPE_LIST:		return (uint32)List()->size();
		case VAR_TYPE_INDEX:	return (uint32)IMap()->size();
		default:		return 0;
	}
}

const char* CVariant::SVariant::Key(uint32 Index) const
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			std::map<std::string, CVariant>* pMap = Map();
			if(Index >= pMap->size())
				throw CException(L"Index out of bound");

			std::map<std::string, CVariant>::iterator I = pMap->begin();
			while(Index--)
				I++;
			return I->first.c_str();
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default:		throw CException(L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::Insert(const char* Name, const CVariant& Variant)
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			std::pair<std::map<std::string, CVariant>::iterator, bool> Ret = Map()->insert(std::map<std::string, CVariant>::value_type(Name, Variant));
			if(!Ret.second)
				Ret.first->second.Attach(Variant.m_Variant);
			return Ret.first->second;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default:		throw CException(L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::At(const char* Name, size_t Len) const
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			std::string sName;
			sName.assign(Name, Len != -1 ? Len : strlen(Name));
			std::map<std::string, CVariant>* pMap = Map();
			std::map<std::string, CVariant>::iterator I = pMap->find(sName);
			if(I == pMap->end())
				I = pMap->insert(std::map<std::string, CVariant>::value_type(sName, CVariant())).first;
			return I->second;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default:		throw CException(L"Not a Map Variant");
	}
}

bool CVariant::SVariant::Has(const char* Name) const
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			std::map<std::string, CVariant>* pMap = Map();
			return pMap->find(Name) != pMap->end();
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default:		return false;
	}
}

void CVariant::SVariant::Remove(const char* Name)
{
	switch(Type)
	{
		case VAR_TYPE_MAP:
		{
			std::map<std::string, CVariant>* pMap = Map();
			std::map<std::string, CVariant>::iterator I = pMap->find(Name);
			if(I != pMap->end())
				pMap->erase(I);
			break;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_INDEX:
		default:		throw CException(L"Not a Map Variant");
	}
}

CVariant& CVariant::SVariant::Append(const CVariant& Variant)
{
	switch(Type)
	{
		case VAR_TYPE_LIST:
		{
			std::vector<CVariant>* pList = List();
			pList->push_back(Variant);
			return pList->back();
		}
		case VAR_TYPE_MAP:
		case VAR_TYPE_INDEX:
		default:		throw CException(L"Not a List Variant");
	}
}

CVariant& CVariant::SVariant::At(uint32 Index) const
{
	switch(Type)
	{
		case VAR_TYPE_LIST:
		{
			std::vector<CVariant>* pList = List();
			if(Index >= pList->size())
				throw CException(L"Index out of bound");
			return (*pList)[Index];
		}
		case VAR_TYPE_INDEX:
		{
			std::map<TIndex, CVariant>* pMap = IMap();
			std::map<TIndex, CVariant>::iterator I = pMap->find(TIndex{ Index });
			if(I == pMap->end())
				I = pMap->insert(std::map<TIndex, CVariant>::value_type(TIndex{ Index }, CVariant())).first;
			return I->second;
		}
		case VAR_TYPE_MAP:
		default:		throw CException(L"Not a List/Index Variant");
	}
}

void CVariant::SVariant::Remove(uint32 Index)
{
	switch(Type)
	{
		case VAR_TYPE_LIST:
		{
			std::vector<CVariant>* pList = List();
			if(Index >= pList->size())
				throw CException(L"Index out of bound");
			std::vector<CVariant>::iterator I = pList->begin();
			while(Index--)
				I++;
			pList->erase(I);
		}
		case VAR_TYPE_INDEX:
		{
			std::map<TIndex, CVariant>* pMap = IMap();
			std::map<TIndex, CVariant>::iterator I = pMap->find(TIndex{ Index });
			if(I != pMap->end())
				pMap->erase(I);
			break;
		}
		case VAR_TYPE_MAP:
		default:		throw CException(L"Not a List/Index Variant");
	}
}

uint32 CVariant::SVariant::Id(uint32 Index) const
{
	switch(Type)
	{
		case VAR_TYPE_INDEX:
		{
			std::map<TIndex, CVariant>* pMap = IMap();
			if(Index >= pMap->size())
				throw CException(L"Index out of bound");

			std::map<TIndex, CVariant>::iterator I = pMap->begin();
			while(Index--)
				I++;
			return I->first.u;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_MAP:
		default:		throw CException(L"Not a Map Variant");
	}
}

bool CVariant::SVariant::Has(uint32 Index) const
{
	switch(Type)
	{
		case VAR_TYPE_INDEX:
		{
			std::map<TIndex, CVariant>* pMap = IMap();
			return pMap->find(TIndex{ Index }) != pMap->end();
		}
		case VAR_TYPE_LIST:
		{
			std::vector<CVariant>* pList = List();
			return Index < pList->size();
		}
		case VAR_TYPE_MAP:
		default:		return false;
	}
}

CVariant& CVariant::SVariant::Insert(uint32 Index, const CVariant& Variant)
{
	switch(Type)
	{
		case VAR_TYPE_INDEX:
		{
			std::pair<std::map<TIndex, CVariant>::iterator, bool> Ret = IMap()->insert(std::map<TIndex, CVariant>::value_type(TIndex{ Index }, Variant));
			if(!Ret.second)
				Ret.first->second.Attach(Variant.m_Variant);
			return Ret.first->second;
		}
		case VAR_TYPE_LIST:
		case VAR_TYPE_MAP:
		default:		throw CException(L"Not a Map Variant");
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Map Read/Write

bool CVariant::ReadRawMap(const std::function<void(const SVarName& Name, const CVariant& Data)>& cb) const
{
	const SVariant* Variant = Val();
	if (Variant->Type != VAR_TYPE_MAP) 
		return false;

	if (Variant->Container.Map) { // in case this variant has already been parsed
		for (auto I = Variant->Container.Map->begin(); I != Variant->Container.Map->end(); ++I)
			cb(SVarName{ I->first.c_str(), I->first.length() }, I->second);
		return true;
	}

	CBuffer Packet(Variant->Payload, Variant->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Variant->Size; )
	{
		uint8 Len = Packet.ReadValue<uint8>();
		if(Packet.GetSizeLeft() < Len)
			throw CException(L"invalid variant name");
		//std::string Name = std::string((char*)Packet.ReadData(Len), Len);
		SVarName Name;
		Name.Buf = (char*)Packet.ReadData(Len);
		Name.Len = Len;

		CVariant Temp;
		Temp.FromPacket(&Packet,true);

		cb(Name, Temp);
	}
	return true;
}

bool CVariant::Find(const char* sName, CVariant& Data) const
{
	const SVariant* Variant = Val();
	if (Variant->Type != VAR_TYPE_MAP) 
		return false;

	if (Variant->Container.Map) { // in case this variant has already been parsed
		if (!Has(sName))
			return false;
		Data = At(sName);
		return true;
	}

	size_t uLen = strlen(sName);

	CBuffer Packet(Variant->Payload, Variant->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Variant->Size; )
	{
		uint8 Len = Packet.ReadValue<uint8>();
		if(Packet.GetSizeLeft() < Len)
			throw CException(L"invalid variant name");

		SVarName Name;
		Name.Buf = (char*)Packet.ReadData(Len);
		Name.Len = Len;
		if (Name.Len == uLen && memcmp(sName, Name.Buf, Name.Len) == 0) {
			Data.FromPacket(&Packet, true);
			return true;
		}

		Packet.ReadData(ReadHeader(&Packet));
	}
	return false;
}

void CVariant::BeginMap()
{
	Clear();

	m_Variant = Alloc();
	m_Variant->Refs.fetch_add(1);
	m_Variant->Access = eWriteOnly;
	m_Variant->Type = VAR_TYPE_MAP;
	m_Variant->Container.Buffer = new CBuffer();
}

void CVariant::WriteValue(const char* Name, EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_MAP)
		throw new CException(L"Variant is not a map");

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	m_Variant->Container.Buffer->WriteValue<uint8>(Len);
	m_Variant->Container.Buffer->WriteData(Name, Len);

	ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}

void CVariant::WriteVariant(const char* Name, const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_MAP)
		throw new CException(L"Variant is not a map");

	ASSERT(strlen(Name) < 0xFF);
	uint8 Len = (uint8)strlen(Name);
	m_Variant->Container.Buffer->WriteValue<uint8>(Len);
	m_Variant->Container.Buffer->WriteData(Name, Len);

	Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// List Read/Write

bool CVariant::ReadRawList(const std::function<void(const CVariant& Data)>& cb) const
{
	const SVariant* Variant = Val();
	if (Variant->Type != VAR_TYPE_LIST) 
		return false;

	if (Variant->Container.List) { // in case this variant has already been parsed
		for (auto I = Variant->Container.List->begin(); I != Variant->Container.List->end(); ++I)
			cb(*I);
		return true;
	}

	CBuffer Packet(Variant->Payload, Variant->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Variant->Size; )
	{
		CVariant Temp;
		Temp.FromPacket(&Packet,true);

		cb(Temp);
	}
	return true;
}

void CVariant::BeginList()
{
	Clear();

	m_Variant = Alloc();
	m_Variant->Refs.fetch_add(1);
	m_Variant->Access = eWriteOnly;
	m_Variant->Type = VAR_TYPE_LIST;
	m_Variant->Container.Buffer = new CBuffer();
}

void CVariant::WriteValue(EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_LIST)
		throw new CException(L"Variant is not a list");

	ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}
	
void CVariant::WriteVariant(const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_LIST)
		throw new CException(L"Variant is not a list");

	Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// IMap Read/Write

bool CVariant::ReadRawIMap(const std::function<void(uint32 Index, const CVariant& Data)>& cb) const
{
	const SVariant* Variant = Val();
	if (Variant->Type != VAR_TYPE_MAP) 
		return false;

	if (Variant->Container.Index) { // in case this variant has already been parsed
		for (auto I = Variant->Container.Index->begin(); I != Variant->Container.Index->end(); ++I)
			cb(I->first.u, I->second);
		return true;
	}

	CBuffer Packet(Variant->Payload, Variant->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Variant->Size; )
	{
		uint32 Index = Packet.ReadValue<uint32>();
		
		CVariant Temp;
		Temp.FromPacket(&Packet,true);

		cb(Index, Temp);
	}
	return true;
}



bool CVariant::Find(uint32 uIndex, CVariant& Data) const
{
	const SVariant* Variant = Val();
	if (Variant->Type != VAR_TYPE_MAP) 
		return false;

	if (Variant->Container.Index) { // in case this variant has already been parsed
		if (!Has(uIndex))
			return false;
		Data = At(uIndex);
		return true;
	}

	CBuffer Packet(Variant->Payload, Variant->Size, true);
	for(size_t Pos = Packet.GetPosition(); Packet.GetPosition() - Pos < Variant->Size; )
	{
		uint32 Index = Packet.ReadValue<uint32>();

		if (Index == uIndex) {
			Data.FromPacket(&Packet, true);
			return true;
		}

		Packet.ReadData(ReadHeader(&Packet));
	}
	return false;
}

void CVariant::BeginIMap()
{
	Clear();

	m_Variant = Alloc();
	m_Variant->Refs.fetch_add(1);
	m_Variant->Access = eWriteOnly;
	m_Variant->Type = VAR_TYPE_MAP;
	m_Variant->Container.Buffer = new CBuffer();
}

void CVariant::WriteValue(uint32 Index, EType Type, size_t Size, const void* Value)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_MAP)
		throw new CException(L"Variant is not a imap");

	m_Variant->Container.Buffer->WriteValue<uint32>(Index);

	ToPacket(m_Variant->Container.Buffer, Type, Size, Value);
}

void CVariant::WriteVariant(uint32 Index, const CVariant& Variant)
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");
	if (m_Variant->Type != VAR_TYPE_MAP)
		throw new CException(L"Variant is not a imap");

	m_Variant->Container.Buffer->WriteValue<uint32>(Index);

	Variant.ToPacket(m_Variant->Container.Buffer);
}

//////////////////////////////////////////////////////////////////////////////////////////////
// Common Read/Write

void CVariant::Finish()
{
	if (m_Variant->Access != eWriteOnly) 
		throw new CException(L"Variant is not being writen to");

	m_Variant->Access = eReadOnly;
	m_Variant->Size = (uint32)m_Variant->Container.Buffer->GetSize();
	m_Variant->Payload = m_Variant->Container.Buffer->GetBuffer(true);
	delete m_Variant->Container.Buffer;
	m_Variant->Container.Buffer = NULL;
}

//////////////////////////////////////////////////////////////////////////////////////////////
// 

void WritePacket(const std::string& Name, const CVariant& Packet, CBuffer& Buffer)
{
	ASSERT(Name.length() < 0xFF);
	uint8 Len = (uint8)Name.length();
	ASSERT(Len < 0x7F);
	Buffer.WriteValue<uint8>(Len);							// [uint8 - NameLen]
	Buffer.WriteData((byte*)Name.c_str(), Name.length());	// [bytes - Name]
	Packet.ToPacket(&Buffer);								// [uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]
}

void ReadPacket(const CBuffer& Buffer, std::string& Name, CVariant& Packet)
{
	uint8 Len = *((uint8*)Buffer.ReadData(sizeof(uint8)));
	Len &= 0x7F;
	char* pName = (char*)Buffer.ReadData(Len);
	Name = std::string(pName, Len);
	Packet.FromPacket(&Buffer);
}

//bool StreamPacket(CBuffer& Buffer, std::string& Name, CVariant& Packet)
//{
//	// [uint8 - NameLen][bytes - Name][uint8 Type][uint32/uint16/uint8 - PaylaodLen][bytes - Paylaod]([uint32 - CRC CheckSumm])
//
//	Buffer.SetPosition(0);
//
//	uint8 Len = *((uint8*)Buffer.GetData(sizeof(uint8)));
//	Len &= 0x7F;
//	char* pName = (char*)Buffer.GetData(Len);
//	if(!pName)
//		return false; // incomplete header
//
//	uint8* pType = ((uint8*)Buffer.GetData(sizeof(uint8) + Len, sizeof(uint8)));
//	if(pType == NULL)
//		return false; // incomplete header
//
//	void* pSize; // = (uint32*)Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));
//	if((*pType & CVariant::ELen32) == CVariant::ELen8)
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint8));
//	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint16));
//	else
//		pSize = Buffer.GetData(sizeof(uint8) + Len + sizeof(uint8), sizeof(uint32));
//
//	if(pSize == NULL)
//		return false; // incomplete header
//
//	size_t Size; // = sizeof(uint8) + sizeof(uint32) + *pSize;
//	if((*pType & CVariant::ELen32) == CVariant::ELen8)
//		Size = sizeof(uint8) + sizeof(uint8) + *((uint8*)pSize);
//	else if((*pType & CVariant::ELen32) == CVariant::ELen16)
//		Size = sizeof(uint8) + sizeof(uint16) + *((uint16*)pSize);
//	else
//		Size = sizeof(uint8) + sizeof(uint32) + *((uint32*)pSize);
//
//	if(Buffer.GetSizeLeft() < Size)
//		return false; // incomplete data
//
//	Name = std::string(pName, Len);
//	Packet.FromPacket(&Buffer);
//	Buffer.ShiftData(sizeof(uint8) + Len + Size);
//
//	return true;
//}