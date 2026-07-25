#pragma once

typedef unsigned char		uchar;
typedef unsigned char		uint8;
typedef	  signed char		sint8;

typedef unsigned short		uint16;
typedef	  signed short		sint16;

typedef unsigned int		uint32;
typedef	  signed int		sint32;

#ifndef WIN32
#define __int64 long long
#endif

typedef unsigned __int64	uint64;
typedef   signed __int64	sint64;

typedef	unsigned char		byte;

typedef	unsigned int		UINT;

#define SEC2MS(s)	(s*1000)
#define MIN2MS(m)	SEC2MS(m*60)
#define HR2MS(h)	MIN2MS(h*60)
#define DAY2MS(d)	HR2MS(d*24)

#define SEC(sec)	(sec)
#define MIN2S(m)	(m*60)
#define HR2S(h)		MIN2S(h*60)
#define DAY2S(d)	HR2S(d*24)

#define KB2B(kb)	(kb*1024)
#define MB2B(mb)	KB2B(mb*1024)
#define GB2B(gb)	MB2B(gb*1024)
#define TB2B(tb)	GB2B(tb*1024)
#define PB2B(pb)	TB2B(pb*1024)

#define MAX_FLOAT	0x001FFFFFFFFFFFFF

#pragma warning(disable : 4251)