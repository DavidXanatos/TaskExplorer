#include <stdlib.h>
#include "SVariant.h"

#pragma warning(disable : 4100)

uint32 Variant_ReadSize(uchar** ptr, size_t size)
{
	uint32 len = (uint32)-1;
	uint8 type = (*ptr)[0];
	*ptr += 1;
	if (type & VAR_LEN_FIELD) 
	{
		if ((type & VAR_LEN_MASK) == VAR_LEN8 && size >= 1 + 1) {
			len = *(uint8*)*ptr;
			*ptr += 1;
		}
		else if ((type & VAR_LEN_MASK) == VAR_LEN16 && size >= 1 + 2) {
			len = *(uint16*)*ptr;
			*ptr += 2;
		}
		else if ((type & VAR_LEN_MASK) == VAR_LEN32 && size >= 1 + 4) {
			len = *(uint32*)*ptr;
			*ptr += 4;
		}
	}
	else if ((type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		len = 0;
	else if ((type & VAR_LEN_MASK) == VAR_LEN8)
		len = 1;
	else if ((type & VAR_LEN_MASK) == VAR_LEN16)
		len = 2;
	else if ((type & VAR_LEN_MASK) == VAR_LEN32)
		len = 4;
	else if ((type & VAR_LEN_MASK) == VAR_LEN64)
		len = 8;
	return len;
}

uint32 Variant_WriteSize(uchar** ptr, uint8 type, size_t size)
{
	uint32 hdr_size = 1;
	(*ptr)[0] = type & VAR_TYPE_MASK;
	if ((type & VAR_TYPE_MASK) == VAR_TYPE_EMPTY)
		;
	else if (size == 1)
		(*ptr)[0] |= VAR_LEN8;
	else if (size == 2)
		(*ptr)[0] |= VAR_LEN16;
	else if (size == 4)
		(*ptr)[0] |= VAR_LEN32;
	else if (size == 8)
		(*ptr)[0] |= VAR_LEN64;
	else
	{
		if (size >= USHRT_MAX) {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN32;
			*(uint32*)&(*ptr)[1] = (uint32)size;
			hdr_size += 4;
		}
		else if (size >= UCHAR_MAX) {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN16;
			*(uint16*)&(*ptr)[1] = (uint16)size;
			hdr_size += 2;
		}
		else {
			(*ptr)[0] |= VAR_LEN_FIELD | VAR_LEN8;
			(*ptr)[1] = (uint8)size;
			hdr_size += 1;
		}
	}
	*ptr += hdr_size;
	return hdr_size;
}

size_t Variant_FromBuffer(const byte* pBuffer, size_t uSize, PVARIANT out_var)
{
	if (uSize < 1 || uSize > ULONG_MAX)
		return 0;
	out_var->uType = pBuffer[0] & VAR_TYPE_MASK;
	out_var->pData = (uchar*)pBuffer;
	uint32 length = Variant_ReadSize(&out_var->pData, (uint32)uSize);
	if (length == -1 || length > (pBuffer + uSize) - out_var->pData)
		return 0;
	out_var->uSize = out_var->uMaxSize = length;
	return out_var->pData - pBuffer + length;
}

size_t Variant_FromPacket(const byte* pBuffer, size_t uSize, char* out_name, size_t max_name, PVARIANT out_var)
{
	if (uSize < 1)
		return 0;
	const byte* ptr = pBuffer;
	uint8 name_len = *ptr++;
	if (uSize < 1 + name_len || name_len > max_name - 1)
		return 0;
	memcpy(out_name, ptr, name_len);
	out_name[name_len] = 0;
	ptr += name_len;

	return Variant_FromBuffer(ptr, uSize - (1 + name_len), out_var);
}

int Variant_Find(const PVARIANT var, const char* name, PVARIANT out_var)
{
	size_t name_len = strlen(name);

	if (var->uSize == 0 || var->uType != VAR_TYPE_MAP)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		uint8 cur_len = *ptr++;
		if (ptr + cur_len >= end)
			break;
		char* cur_name = (char*)ptr;
		ptr += cur_len;
		if (name_len == cur_len && memcmp(cur_name, name, name_len) == 0)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

int Variant_Get(const PVARIANT var, uint32 index, PVARIANT out_var)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_INDEX)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		if (ptr + 4 >= end)
			break;
		uint32 cur_index = *(uint32*)ptr;
		ptr += 4;
		if (cur_index == index)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

int Variant_At(const PVARIANT var, int pos, PVARIANT out_var)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_LIST)
		return 0;

	uchar* ptr = var->pData;
	uchar* end = ptr + var->uSize;
	while (ptr < end)
	{
		if (pos-- == 0)
			return Variant_FromBuffer(ptr, end - ptr, out_var) != 0;
		ptr += Variant_ReadSize(&ptr, end - ptr);
	}

	return 0;
}

int Variant_Begin(const PVARIANT var, PVARIANT_IT it)
{
	if (var->uSize == 0 || var->uType != VAR_TYPE_LIST)
		return 0;

	it->ptr = var->pData;
	it->end = it->ptr + var->uSize;
	return 1;
}

int Variant_Next(PVARIANT_IT it, PVARIANT out_var)
{
	size_t read = Variant_FromBuffer(it->ptr, it->end - it->ptr, out_var);
	if (!read) 
		return 0;
	it->ptr += read;
	return 1;
}

int Variant_ToInt(const PVARIANT var, void* out, size_t size)
{
	if (var->uSize == 0 || (var->uType != VAR_TYPE_SINT && var->uType != VAR_TYPE_UINT))
		return 0;
	if (var->uSize > size)
	{
		for (size_t i = size; i < var->uSize; i++) {
			if (var->pData[i] != 0)
				return 0;
		}
	}

	if (var->uSize < size)
		memset((byte*)out + var->uSize, 0, size - var->uSize);
	memcpy(out, var->pData, var->uSize);
	return 1;
}

size_t Variant_RawBytesOfType(const PVARIANT var, byte** out, uint8 type1, uint8 type2)
{
	if (var->uSize == 0 || var->uType == VAR_TYPE_EMPTY || (var->uType != type1 && var->uType != type2))
		return 0;

	*out = var->pData;
	return var->uSize;
}

size_t Variant_RawBytes(const PVARIANT var, byte** out)
{
	return Variant_RawBytesOfType(var, out, VAR_TYPE_BYTES, VAR_TYPE_ASCII);
}

size_t Variant_RawWStr(const PVARIANT var, wchar_t** out)
{
	return Variant_RawBytesOfType(var, (byte**)out, VAR_TYPE_UNICODE, 0) / sizeof(wchar_t);
}

size_t Variant_ToBytes(const PVARIANT var, byte* out, size_t max_size)
{
	byte* ptr;
	size_t size = Variant_RawBytes(var, &ptr);
	if (size > max_size)
		return 0;

	memcpy(out, ptr, size);
	return size;
}

size_t Variant_ToAStr(const PVARIANT var, char* out, size_t max_count)
{
	size_t size = Variant_ToBytes(var, (byte*)out, max_count - 1);
	out[size] = 0;
	return size;
}

size_t Variant_ToWStr(const PVARIANT var, wchar_t* out, size_t max_count)
{
	wchar_t* ptr;
	size_t size = Variant_RawWStr(var, &ptr);
	if (size > (max_count - 1))
		return 0;

	memcpy(out, ptr, size * sizeof(wchar_t));
	out[size] = 0;
	return size;
}

void Variant_Init(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var)
{
	out_var->uMaxSize = (uint32)uMaxSize;
	out_var->pData = pBuffer;
	out_var->uSize = 0;
	out_var->uType = uType;
}

void Variant_Prepare(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var)
{
	// based on the buffer size pick the larget len field
	uMaxSize -= 1 + 4;
	uint32 hdr_size;
	if (uMaxSize >= USHRT_MAX)
		hdr_size = 1 + 4;
	else if (uMaxSize >= UCHAR_MAX)
		hdr_size = 1 + 2;
	else 
		hdr_size = 1 + 1;
	Variant_Init(uType, pBuffer ? &((byte*)pBuffer)[hdr_size] : NULL, pBuffer ? uMaxSize : 0, out_var);
}

size_t Variant_Finish(void* pBuffer, PVARIANT out_var)
{
	byte* out_buff = (byte*)pBuffer;
    if (out_buff && out_var->uType != VAR_TYPE_EMPTY) {
		size_t hdr_size;
		if (out_var->uType & VAR_STATIC_) {
			hdr_size = 1 + 1;
			out_buff[0] = (out_var->uType & VAR_TYPE_MASK) | VAR_LEN_FIELD | VAR_LEN8;
			out_buff[1] = (uint8)out_var->uSize;
			memcpy(&out_buff[hdr_size], &out_var->uPayload, out_var->uSize);
		} 
		else {
			out_buff[0] = (out_var->uType & VAR_TYPE_MASK);
			if (out_var->uMaxSize >= USHRT_MAX) {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN32;
				*(uint32*)&out_buff[1] = (uint32)out_var->uSize;
				hdr_size = 1 + 4;
			}
			else if (out_var->uMaxSize >= UCHAR_MAX) {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN16;
				*(uint16*)&out_buff[1] = (uint16)out_var->uSize;
				hdr_size = 1 + 2;
			}
			else {
				out_buff[0] |= VAR_LEN_FIELD | VAR_LEN8;
				out_buff[1] = (uint8)out_var->uSize;
				hdr_size = 1 + 1;
			}
		}
        return hdr_size + out_var->uSize;
    }
	return 0;
}

void Variant_PrepareEntry(PVARIANT var, uint8 type, PVARIANT out_var)
{
	uchar* ptr = var->pData + var->uSize;
	size_t len = var->uMaxSize - var->uSize;
	Variant_Prepare(type, ptr, len, out_var);
}

void Variant_FinishEntry(PVARIANT var, PVARIANT out_var)
{
	uchar* ptr = var->pData + var->uSize;
	size_t len = Variant_Finish(ptr, out_var);
	var->uSize += (uint32)len;
}

void* Variant_SInit(uint8 type, const void* in, size_t size, PVARIANT out_var)
{
	if (size > sizeof(out_var->uPayload))
		return NULL;
	out_var->uType = type | VAR_STATIC_;
	out_var->uSize = (uint32)size;
	out_var->uMaxSize = 0;
	memcpy(&out_var->uPayload, in, out_var->uSize);
	return &out_var->uPayload;
}

void* Variant_Set(uint8 type, const void* in, size_t size, PVARIANT out_var)
{
	if (out_var->uMaxSize < size)
		return NULL;
	out_var->uType = type;
	out_var->uSize = (uint32)size;
	memcpy(out_var->pData, in, size);
	return &out_var->uPayload;
}

size_t Variant_WriteRaw(uchar* ptr, uint8 type, const void* in, size_t size)
{
	uint32 hdr_size = Variant_WriteSize(&ptr, type, size);
	memcpy(ptr, in, size);
	return hdr_size + size;
}

void* Variant_InsertRaw(PVARIANT var, const char* name, uint8 type, const void* in, size_t size)
{
	size_t name_len = strlen(name);

	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_MAP;
	else if (var->uType != VAR_TYPE_MAP)
		return NULL;
	if (var->uMaxSize - var->uSize < 1 + name_len + 1 + size)
		return NULL;

	uchar* ptr = var->pData + var->uSize;
	*ptr++ = (uint8)name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;
	var->uSize += 1 + (uint32)name_len;
	
	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);;
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

int Variant_PrepareInsert(PVARIANT var, const char* name, uint8 type, PVARIANT out_var)
{
	if (!Variant_InsertRaw(var, name, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

int Variant_Insert(PVARIANT var, const char* name, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC_)
		return !!Variant_InsertRaw(var, name, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_InsertRaw(var, name, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

void* Variant_AddRaw(PVARIANT var, uint32 index, uint8 type, const void* in, size_t size)
{
	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_INDEX;
	else if (var->uType != VAR_TYPE_INDEX)
		return 0;
	if (var->uMaxSize - var->uSize < 4 + size)
		return 0;

	uchar* ptr = var->pData + var->uSize;
	*(uint32*)ptr = index;
	ptr += 4;
	var->uSize += 4;

	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

int Variant_PrepareAdd(PVARIANT var, uint32 index, uint8 type, PVARIANT out_var)
{
	if (!Variant_AddRaw(var, index, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

int Variant_Add(PVARIANT var, uint32 index, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC_)
		return !!Variant_AddRaw(var, index, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_AddRaw(var, index, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

void* Variant_AppendRaw(PVARIANT var, uint8 type, const void* in, size_t size)
{
	if (var->uType == VAR_TYPE_EMPTY)
		var->uType = VAR_TYPE_LIST;
	else if (var->uType != VAR_TYPE_LIST)
		return 0;
	if (var->uMaxSize - var->uSize < + size)
		return 0;

	uchar* ptr = var->pData + var->uSize;

	if (in) {
		size_t len = Variant_WriteRaw(ptr, type, in, size);
		var->uSize += (uint32)len;
		ptr += len - size;
	}
	return ptr;
}

int Variant_PrepareAppend(PVARIANT var, uint8 type, PVARIANT out_var)
{
	if (!Variant_AppendRaw(var, 0, NULL, 0))
		return 0;

	Variant_PrepareEntry(var, type, out_var);
	return 1;
}

int Variant_Append(PVARIANT var, const PVARIANT in_var)
{
	if (in_var->uType & VAR_STATIC_)
		return !!Variant_AppendRaw(var, in_var->uType & VAR_TYPE_MASK, &in_var->uPayload, in_var->uSize);
	return !!Variant_AppendRaw(var, in_var->uType & VAR_TYPE_MASK, in_var->pData, in_var->uSize);
}

size_t Variant_ToBuffer(const PVARIANT var, void* pBuffer, size_t uMaxSize)
{
	if (uMaxSize < 1 + 4 + var->uSize)
		return 0;

	if (var->uType & VAR_STATIC_)
		return Variant_WriteRaw(pBuffer, var->uType & VAR_TYPE_MASK, &var->uPayload, var->uSize);
	return Variant_WriteRaw(pBuffer, var->uType & VAR_TYPE_MASK, var->pData, var->uSize);
}

size_t Variant_ToPacket(char* name, const PVARIANT var, void* pBuffer, size_t uMaxSize)
{
	size_t name_len = strlen(name);

	if (uMaxSize < 1 + name_len || name_len > 0x7F)
		return 0;

	uchar* ptr = pBuffer;
	*ptr++ = (uint8)name_len;
	memcpy(ptr, name, name_len);
	ptr += name_len;

	return Variant_ToBuffer(var, ptr, uMaxSize - (1 + name_len));
}