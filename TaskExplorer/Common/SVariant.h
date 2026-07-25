#pragma once

#include "..\Types.h"

#include "VariantDefs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct _VARIANT
{
	uint8  uType;
	uint32 uSize;
	uint32 uMaxSize;
	union {
		uchar* pData;
		uint64 uPayload;
	};
} VARIANT, * PVARIANT;

typedef struct _VARIANT_IT
{
	uchar* ptr;
	uchar* end;
} VARIANT_IT, * PVARIANT_IT;

size_t Variant_FromBuffer(const byte* pBuffer, size_t uSize, PVARIANT out_var);
size_t Variant_FromPacket(const byte* pBuffer, size_t uSize, char* out_name, size_t max_name, PVARIANT out_var);

int Variant_Find(const PVARIANT var, const char* name, PVARIANT out_var);
int Variant_Get(const PVARIANT var, uint32 index, PVARIANT out_var);
int Variant_At(const PVARIANT var, int pos, PVARIANT out_var);
int Variant_Begin(const PVARIANT var, PVARIANT_IT it);
int Variant_Next(PVARIANT_IT it, PVARIANT out_var);

int Variant_ToInt(const PVARIANT var, void* out, size_t size);
__inline uint8  Variant_ToUInt8(const PVARIANT var, uint8 def_val)								{ uint8  val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline uint16 Variant_ToUInt16(const PVARIANT var, uint16 def_val)							{ uint16 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline uint32 Variant_ToUInt32(const PVARIANT var, uint32 def_val)							{ uint32 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline uint64 Variant_ToUInt64(const PVARIANT var, uint64 def_val)							{ uint64 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline sint8  Variant_ToSInt8(const PVARIANT var, sint8 def_val)								{ sint8  val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline sint16 Variant_ToSInt16(const PVARIANT var, sint16 def_val)							{ sint16 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline sint32 Variant_ToSInt32(const PVARIANT var, sint32 def_val)							{ sint32 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }
__inline sint64 Variant_ToSInt64(const PVARIANT var, sint64 def_val)							{ sint64 val; if (Variant_ToInt(var, &val, sizeof(val))) return val; return def_val; }

size_t Variant_RawBytes(const PVARIANT var, byte** out);
__inline size_t Variant_RawAStr(const PVARIANT var, char** out)									{ return Variant_RawBytes(var, (byte**)out); }
size_t Variant_RawWStr(const PVARIANT var, wchar_t** out);
size_t Variant_ToBytes(const PVARIANT var, byte* out, size_t max_size);
size_t Variant_ToAStr(const PVARIANT var, char* out, size_t max_count);
size_t Variant_ToWStr(const PVARIANT var, wchar_t* out, size_t max_count);

__inline int Variant_FindInt(const PVARIANT var, const char* name, void* out, size_t size)		{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_ToInt(&out_var, out, size); }
__inline uint8  Variant_FindUInt8(const PVARIANT var, const char* name, uint8 def_val)			{ uint8  val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline uint16 Variant_FindUInt16(const PVARIANT var, const char* name, uint16 def_val)		{ uint16 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline uint32 Variant_FindUInt32(const PVARIANT var, const char* name, uint32 def_val)		{ uint32 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline uint64 Variant_FindUInt64(const PVARIANT var, const char* name, uint64 def_val)		{ uint64 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline sint8  Variant_FindSInt8(const PVARIANT var, const char* name, sint8 def_val)			{ sint8  val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline sint16 Variant_FindSInt16(const PVARIANT var, const char* name, sint16 def_val)		{ sint16 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline sint32 Variant_FindSInt32(const PVARIANT var, const char* name, sint32 def_val)		{ sint32 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }
__inline sint64 Variant_FindSInt64(const PVARIANT var, const char* name, sint64 def_val)		{ sint64 val; if (Variant_FindInt(var, name, &val, sizeof(val))) return val; return def_val; }

__inline int Variant_GetInt(const PVARIANT var, uint32 index, void* out, size_t size)			{ VARIANT out_var; if (!Variant_Get(var, index, &out_var)) return 0; return Variant_ToInt(&out_var, out, size); }
__inline uint8  Variant_GetUInt8(const PVARIANT var, uint32 index, uint8 def_val)				{ uint8  val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline uint16 Variant_GetUInt16(const PVARIANT var, uint32 index, uint16 def_val)				{ uint16 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline uint32 Variant_GetUInt32(const PVARIANT var, uint32 index, uint32 def_val)				{ uint32 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline uint64 Variant_GetUInt64(const PVARIANT var, uint32 index, uint64 def_val)				{ uint64 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline sint8  Variant_GetSInt8(const PVARIANT var, uint32 index, sint8 def_val)				{ sint8  val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline sint16 Variant_GetSInt16(const PVARIANT var, uint32 index, sint16 def_val)				{ sint16 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline sint32 Variant_GetSInt32(const PVARIANT var, uint32 index, sint32 def_val)				{ sint32 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }
__inline sint64 Variant_GetSInt64(const PVARIANT var, uint32 index, sint64 def_val)				{ sint64 val; if (Variant_GetInt(var, index, &val, sizeof(val))) return val; return def_val; }

__inline int Variant_IntAt(const PVARIANT var, int pos, void* out, size_t size)					{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return 0; return Variant_ToInt(&out_var, out, size); }
__inline uint8  Variant_UInt8At(const PVARIANT var, int pos, uint8 def_val)						{ uint8  val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline uint16 Variant_UInt16At(const PVARIANT var, int pos, uint16 def_val)					{ uint16 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline uint32 Variant_UInt32At(const PVARIANT var, int pos, uint32 def_val)					{ uint32 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline uint64 Variant_UInt64At(const PVARIANT var, int pos, uint64 def_val)					{ uint64 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline sint8  Variant_SInt8At(const PVARIANT var, int pos, sint8 def_val)						{ sint8  val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline sint16 Variant_SInt16At(const PVARIANT var, int pos, sint16 def_val)					{ sint16 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline sint32 Variant_SInt32At(const PVARIANT var, int pos, sint32 def_val)					{ sint32 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }
__inline sint64 Variant_SInt64At(const PVARIANT var, int pos, sint64 def_val)					{ sint64 val; if (Variant_IntAt(var, pos, &val, sizeof(val))) return val; return def_val; }

__inline size_t Variant_FindRawBytes(const PVARIANT var, const char* name, byte** out)			{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_RawBytes(&out_var, out); }
__inline size_t Variant_FindRawAStr(const PVARIANT var, const char* name, char** out)			{ return Variant_FindRawBytes(var, name, (byte**)out); }
__inline size_t Variant_FindRawWStr(const PVARIANT var, const char* name, wchar_t** out)		{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_RawWStr(&out_var, out); }
__inline size_t Variant_FindBytes(const PVARIANT var, const char* name, byte* out, size_t max_size)		{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_ToBytes(&out_var, out, max_size); }
__inline size_t Variant_FindAStr(const PVARIANT var, const char* name, char* out, size_t max_size)		{ return Variant_FindBytes(var, name, (byte*)out, max_size); }
__inline size_t Variant_FindWStr(const PVARIANT var, const char* name, wchar_t* out, size_t max_size)	{ VARIANT out_var; if (!Variant_Find(var, name, &out_var)) return 0; return Variant_ToWStr(&out_var, out, max_size); }

__inline size_t Variant_RawBytesAt(const PVARIANT var, int pos, byte** out)						{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return 0; return Variant_RawBytes(&out_var, out); }
__inline size_t Variant_RawAStrAt(const PVARIANT var, int pos, char** out)						{ return Variant_RawBytesAt(var, pos, (byte**)out); }
__inline size_t Variant_RawWStrAt(const PVARIANT var, int pos, wchar_t** out)					{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return 0; return Variant_RawWStr(&out_var, out); }
__inline size_t Variant_BytesAt(const PVARIANT var, int pos, byte* out, size_t max_size)		{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return 0; return Variant_ToBytes(&out_var, out, max_size); }
__inline size_t Variant_AStrAt(const PVARIANT var, int pos, char* out, size_t max_size)			{ return Variant_BytesAt(var, pos, (byte*)out, max_size); }
__inline size_t Variant_WStrAt(const PVARIANT var, int pos, wchar_t* out, size_t max_size)		{ VARIANT out_var; if (!Variant_At(var, pos, &out_var)) return 0; return Variant_ToWStr(&out_var, out, max_size); }

void Variant_Init(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var);

void Variant_Prepare(uint8 uType, void* pBuffer, size_t uMaxSize, PVARIANT out_var);
size_t Variant_Finish(void* pBuffer, PVARIANT out_var);

void* Variant_SInit(uint8 type, const void* in, size_t size, PVARIANT out_var);
__inline uint8*  Variant_FromUInt8(uint8 in, PVARIANT out_var)									{ return (uint8*) Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
__inline uint16* Variant_FromUInt16(uint16 in, PVARIANT out_var)								{ return (uint16*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
__inline uint32* Variant_FromUInt32(uint32 in, PVARIANT out_var)								{ return (uint32*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
__inline uint64* Variant_FromUInt64(uint64 in, PVARIANT out_var)								{ return (uint64*)Variant_SInit(VAR_TYPE_UINT, &in, sizeof(in),out_var); }
__inline sint8*  Variant_FromSInt8(sint8 in, PVARIANT out_var)									{ return (sint8*) Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
__inline sint16* Variant_FromSInt16(sint16 in, PVARIANT out_var)								{ return (sint16*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
__inline sint32* Variant_FromSInt32(sint32 in, PVARIANT out_var)								{ return (sint32*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
__inline sint64* Variant_FromSInt64(sint64 in, PVARIANT out_var)								{ return (sint64*)Variant_SInit(VAR_TYPE_SINT, &in, sizeof(in),out_var); }
void* Variant_Set(uint8 type, const void* in, size_t size, PVARIANT out_var);
__inline byte* Variant_FromBytes(const byte* in, size_t size, PVARIANT out_var)					{ return (byte*)Variant_Set(VAR_TYPE_BYTES, in, size, out_var); }
__inline char* Variant_FromAStr(const char* in, size_t count, PVARIANT out_var)					{ return (char*)Variant_Set(VAR_TYPE_ASCII, in, count, out_var); }
__inline wchar_t* Variant_FromWStr(const wchar_t* in, size_t count, PVARIANT out_var)			{ return (wchar_t*)Variant_Set(VAR_TYPE_UNICODE, in, count * sizeof(wchar_t), out_var); }

void* Variant_InsertRaw(PVARIANT var, const char* name, uint8 type, const void* in, size_t size);
__inline uint8*  Variant_InsertUInt8(PVARIANT var, const char* name, uint8 in)					{ return (uint8*) Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint16* Variant_InsertUInt16(PVARIANT var, const char* name, uint16 in)				{ return (uint16*)Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint32* Variant_InsertUInt32(PVARIANT var, const char* name, uint32 in)				{ return (uint32*)Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint64* Variant_InsertUInt64(PVARIANT var, const char* name, uint64 in)				{ return (uint64*)Variant_InsertRaw(var, name, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline sint8*  Variant_InsertSInt8(PVARIANT var, const char* name, uint8 in)					{ return (sint8*) Variant_InsertRaw(var, name, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint16* Variant_InsertSInt16(PVARIANT var, const char* name, uint16 in)				{ return (sint16*)Variant_InsertRaw(var, name, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint32* Variant_InsertSInt32(PVARIANT var, const char* name, uint32 in)				{ return (sint32*)Variant_InsertRaw(var, name, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint64* Variant_InsertSInt64(PVARIANT var, const char* name, uint64 in)				{ return (sint64*)Variant_InsertRaw(var, name, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline byte* Variant_InsertBytes(PVARIANT var, const char* name, const byte* in, size_t size)	{ return (byte*)Variant_InsertRaw(var, name, VAR_TYPE_BYTES, in, size); }
__inline char* Variant_InsertAStr(PVARIANT var, const char* name, const char* in, size_t count)	{ return (char*)Variant_InsertRaw(var, name, VAR_TYPE_ASCII, in, count); }
__inline wchar_t* Variant_InsertWStr(PVARIANT var, const char* name, const wchar_t* in, size_t count) { return (wchar_t*)Variant_InsertRaw(var, name, VAR_TYPE_UNICODE, in, count * sizeof(wchar_t)); }
int Variant_PrepareInsert(PVARIANT var, const char* name, uint8 type, PVARIANT out_var);

void* Variant_AddRaw(PVARIANT var, uint32 index, uint8 type, const void* in, size_t size);
__inline uint8*  Variant_AddUInt8(PVARIANT var, uint32 index, uint8 in)							{ return (uint8*) Variant_AddRaw(var, index, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint16* Variant_AddUInt16(PVARIANT var, uint32 index, uint16 in)						{ return (uint16*)Variant_AddRaw(var, index, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint32* Variant_AddUInt32(PVARIANT var, uint32 index, uint32 in)						{ return (uint32*)Variant_AddRaw(var, index, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint64* Variant_AddUInt64(PVARIANT var, uint32 index, uint64 in)						{ return (uint64*)Variant_AddRaw(var, index, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline sint8*  Variant_AddSInt8(PVARIANT var, uint32 index, uint8 in)							{ return (sint8*) Variant_AddRaw(var, index, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint16* Variant_AddSInt16(PVARIANT var, uint32 index, uint16 in)						{ return (sint16*)Variant_AddRaw(var, index, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint32* Variant_AddSInt32(PVARIANT var, uint32 index, uint32 in)						{ return (sint32*)Variant_AddRaw(var, index, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint64* Variant_AddSInt64(PVARIANT var, uint32 index, uint64 in)						{ return (sint64*)Variant_AddRaw(var, index, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline byte* Variant_AddBytes(PVARIANT var, uint32 index, const byte* in, size_t size)		{ return (byte*)Variant_AddRaw(var, index, VAR_TYPE_BYTES, in, size); }
__inline char* Variant_AddAStr(PVARIANT var, uint32 index, const char* in, size_t count)		{ return (char*)Variant_AddRaw(var, index, VAR_TYPE_ASCII, in, count); }
__inline wchar_t* Variant_AddWStr(PVARIANT var, uint32 index, const wchar_t* in, size_t count)	{ return (wchar_t*)Variant_AddRaw(var, index, VAR_TYPE_UNICODE, in, count * sizeof(wchar_t)); }
int Variant_PrepareAdd(PVARIANT var, uint32 index, uint8 type, PVARIANT out_var);

void* Variant_AppendRaw(PVARIANT var, uint8 type, const void* in, size_t size);
__inline uint8*   Variant_AppendUInt8(PVARIANT var, uint8 in)									{ return (uint8*) Variant_AppendRaw(var, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint16* Variant_AppendUInt16(PVARIANT var, uint16 in)									{ return (uint16*)Variant_AppendRaw(var, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint32* Variant_AppendUInt32(PVARIANT var, uint32 in)									{ return (uint32*)Variant_AppendRaw(var, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline uint64* Variant_AppendUInt64(PVARIANT var, uint64 in)									{ return (uint64*)Variant_AppendRaw(var, VAR_TYPE_UINT, &in, sizeof(in)); }
__inline sint8*  Variant_AppendSInt8(PVARIANT var, uint8 in)									{ return (sint8*) Variant_AppendRaw(var, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint16* Variant_AppendSInt16(PVARIANT var, uint16 in)									{ return (sint16*)Variant_AppendRaw(var, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint32* Variant_AppendSInt32(PVARIANT var, uint32 in)									{ return (sint32*)Variant_AppendRaw(var, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline sint64* Variant_AppendSInt64(PVARIANT var, uint64 in)									{ return (sint64*)Variant_AppendRaw(var, VAR_TYPE_SINT, &in, sizeof(in)); }
__inline byte* Variant_AppendBytes(PVARIANT var, const byte* in, size_t size)					{ return (byte*)Variant_AppendRaw(var, VAR_TYPE_BYTES, in, size); }
__inline char* Variant_AppendAStr(PVARIANT var, const char* in, size_t count)					{ return (char*)Variant_AppendRaw(var, VAR_TYPE_ASCII, in, count); }
__inline wchar_t* Variant_AppendWStr(PVARIANT var, const wchar_t* in, size_t count)				{ return (wchar_t*)Variant_AppendRaw(var, VAR_TYPE_UNICODE, in, count * sizeof(wchar_t)); }
int Variant_PrepareAppend(PVARIANT var, uint8 type, PVARIANT out_var);

void Variant_FinishEntry(PVARIANT var, PVARIANT out_var);

int Variant_Insert(PVARIANT var, const char* name, const PVARIANT in_var);
int Variant_Add(PVARIANT var, uint32 index, const PVARIANT in_var);
int Variant_Append(PVARIANT var, const PVARIANT in_var);

size_t Variant_ToBuffer(const PVARIANT var, void* pBuffer, size_t uMaxSize);
size_t Variant_ToPacket(char* name, const PVARIANT var, void* pBuffer, size_t uMaxSize);


#ifdef __cplusplus
}
#endif