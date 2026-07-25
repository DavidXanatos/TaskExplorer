#pragma once

// Variant type and len field defs

#define VAR_TYPE_MASK		0b00011111

//#ifdef __cplusplus
//typedef enum _VAR_TYPE : unsigned char
//#else
typedef enum _VAR_TYPE
//#endif
{
		VAR_TYPE_EMPTY	  = 0b00000000,	// An Empty Variant - special case has no payload and no length field
		VAR_TYPE_MAP	  = 0b00000001,	// A Dictionary (Map by Name)
		VAR_TYPE_LIST	  = 0b00000010,	// A List
		VAR_TYPE_INDEX	  = 0b00000011,	// A Index (Map by uint32)							
						//	0b00000100,
						//	0b00000101,
						//	0b00000110,
		VAR_TYPE_BYTES	  = 0b00000111,	// Binary BLOB of arbitrary length
		VAR_TYPE_ASCII	  = 0b00001000,	// String ASCII Encoded
		VAR_TYPE_UTF8	  = 0b00001001,	// String UTF8 Encoded or ASCII
		VAR_TYPE_UNICODE  = 0b00001010,	// String UTF16 Encoded
						//	0b00001011,
		VAR_TYPE_UINT	  = 0b00001100,	// Unsigned Integer 8 16 32 or 64 bit
		VAR_TYPE_SINT	  = 0b00001101,	// Signed Integer 8 16 32 or 64 bit
		VAR_TYPE_DOUBLE	  = 0b00001110,	// Floating point Number 32 or 64 bit precision
						//	0b00001111,
		VAR_TYPE_CUSTOM	  = 0b00010000,
						//  0b0001....
		VAR_TYPE_INVALID  = 0b00011111

} VAR_TYPE, *PVAR_TYPE;

#define VAR_LEN_FIELD		0b00100000	// indicate dedicated length field after type byte

#define VAR_LEN_MASK		0b11000000

#define VAR_LEN8			0b00000000
#define VAR_LEN16			0b01000000
#define VAR_LEN32			0b10000000
#define VAR_LEN64			0b11000000	// not valid for length field length, variants are limited to 4GB


// Internal defs

#define VAR_STATIC_			0b11000000