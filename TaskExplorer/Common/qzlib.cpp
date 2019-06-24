#include "stdafx.h"
#include "qzlib.h"

#ifndef byte
#define byte unsigned char
#endif

QByteArray Pack(const QByteArray& Data)
{
	uLongf newsize = Data.size() + 300;
	QByteArray Buffer;
	Buffer.resize(newsize);
	int result = compress2((byte*)Buffer.data(),&newsize,(byte*)Data.data(),Data.size(),Z_BEST_COMPRESSION);
	Buffer.truncate(newsize);

	ASSERT(result == Z_OK);
	return Buffer;
}

QByteArray Unpack(const QByteArray& Data)
{
	QByteArray Buffer;
	uLongf newsize = Data.size()*10+300;
	uLongf unpackedsize = 0;
	int result = 0;
	do 
	{
		Buffer.resize(newsize);
		unpackedsize = newsize;
		result = uncompress((byte*)Buffer.data(),&unpackedsize,(byte*)Data.data(),Data.size());
		newsize *= 2; // size for the next try if needed
	} 
	while (result == Z_BUF_ERROR && newsize < Max(16*1024*1024, (uLongf)(Data.size()*100)));	// do not allow the unzip buffer to grow infinetly, 
																				// assume that no packetcould be originaly larger than the UnpackLimit nd those it must be damaged
	if (result == Z_OK)
		Buffer.truncate(unpackedsize);
	else
		Buffer.clear();
	return Buffer;
}

// gzip ////////////////////////////////////////////////

bool gzip_arr(QByteArray& in)
{
	QByteArray out;
	out.resize(in.size() + 16);
	z_stream zlibStreamStruct;
	zlibStreamStruct.zalloc = Z_NULL; // Set zalloc, zfree, and opaque to Z_NULL so
	zlibStreamStruct.zfree = Z_NULL; // that when we call deflateInit2 they will be
	zlibStreamStruct.opaque = Z_NULL; // updated to use default allocation functions.
	zlibStreamStruct.total_out = 0; // Total number of output bytes produced so far
	zlibStreamStruct.next_in = (Bytef*)in.data(); // Pointer to input bytes
	zlibStreamStruct.avail_in = in.size(); // Number
	int res = deflateInit2(&zlibStreamStruct, Z_BEST_COMPRESSION, Z_DEFLATED, (15+16), 8, Z_DEFAULT_STRATEGY);
	if (res!= Z_OK) return false;
	do {
		zlibStreamStruct.next_out = (Bytef*)out.data() + zlibStreamStruct.total_out;
		zlibStreamStruct.avail_out = out.size() - zlibStreamStruct.total_out;
		res = deflate(&zlibStreamStruct, Z_FINISH);
	} while ( res == Z_OK );
	deflateEnd(&zlibStreamStruct);
	out.truncate(zlibStreamStruct.total_out);
	if(out.size() < in.size())
	{
		in = out;
		return true;
	}
	return false;
}

static int gz_magic[2] = {0x1f, 0x8b}; /* gzip magic header */

/* gzip flag byte */
#define ASCII_FLAG   0x01 /* bit 0 set: file probably ascii text */
#define HEAD_CRC     0x02 /* bit 1 set: header CRC present */
#define EXTRA_FIELD  0x04 /* bit 2 set: extra field present */
#define ORIG_NAME    0x08 /* bit 3 set: original file name present */
#define COMMENT      0x10 /* bit 4 set: file comment present */
#define RESERVED     0xE0 /* bits 5..7: reserved */

int get_byte(QByteArray& zipped)
{
	if(zipped.isEmpty())
		return EOF;
	unsigned char c = zipped.at(0);
	zipped.remove(0,1);
	return c;
}

int check_header(z_stream *stream, QByteArray& zipped)
{
	int method; /* method byte */
	int flags;  /* flags byte */
	uInt len;
	int c;

	/* Check the gzip magic header */
	for(len = 0; len < 2; len++) {
		c = get_byte(zipped);
		if(c != gz_magic[len]) {
			if(len != 0) stream->avail_in++, stream->next_in--;
			if(c != EOF) {
				stream->avail_in++, stream->next_in--;
				//do not support transparent streams
				return stream->avail_in != 0 ? Z_DATA_ERROR : Z_STREAM_END;
			}
			return stream->avail_in != 0 ? Z_OK : Z_STREAM_END;
		}
	}
	method = get_byte(zipped);
	flags = get_byte(zipped);
	if(method != Z_DEFLATED || (flags & RESERVED) != 0)
		return Z_DATA_ERROR;

	/* Discard time, xflags and OS code: */
	for(len = 0; len < 6; len++) (void)get_byte(zipped);

	if((flags & EXTRA_FIELD) != 0) { /* skip the extra field */
		len  =  (uInt)get_byte(zipped);
		len += ((uInt)get_byte(zipped))<<8;
		/* len is garbage if EOF but the loop below will quit anyway */
		while(len-- != 0 && get_byte(zipped) != EOF) ;
	}
	if((flags & ORIG_NAME) != 0) { /* skip the original file name */
		while((c = get_byte(zipped)) != 0 && c != EOF) ;
	}
	if((flags & COMMENT) != 0) {   /* skip the .gz file comment */
		while((c = get_byte(zipped)) != 0 && c != EOF) ;
	}
	if((flags & HEAD_CRC) != 0) {  /* skip the header crc */
		for(len = 0; len < 2; len++) (void)get_byte(zipped);
	}
	//return Z_DATA_ERROR if we hit EOF?
	return Z_OK;
}

bool IsgZiped(const QByteArray& zipped)
{
	if(zipped.size() < 2 || (Byte)zipped.at(0) != gz_magic[0] || (Byte)zipped.at(1) != gz_magic[1])
		return false;
	return true;
}

QByteArray ungzip_arr(z_stream* &zS, QByteArray& zipped, bool bGZip, int iRecursion /*= 0*/)
{
	QByteArray unzipped;
	unzipped.resize(zipped.size() * 2 * (iRecursion + 1));

  	int err = Z_DATA_ERROR;
  	try
	{
	    if (zS == NULL)
	    {
		    zS = new z_stream;
    
		    zS->zalloc = (alloc_func)0;
		    zS->zfree = (free_func)0;
		    zS->opaque = (voidpf)0;
    
			if(bGZip)
			{
				err = inflateInit2(zS, -MAX_WBITS);
				if (err != Z_OK)
					return QByteArray();

				err = check_header(zS, zipped);
				if (err != Z_OK)
					return QByteArray();
			}
			else
			{
				err = inflateInit(zS);
				if (err != Z_OK)
					return QByteArray();
			}
		}

		if(iRecursion == 0)
		{
			zS->next_in	 = const_cast<Bytef*>((Byte*)zipped.data());
			zS->avail_in = zipped.size();
		}
    
	    zS->next_out = (Byte*)unzipped.data();
	    zS->avail_out = unzipped.size();

		uLong totalUnzipped = zS->total_out;

	    err = inflate(zS, Z_SYNC_FLUSH);
    
	    if (err == Z_STREAM_END)
	    {
		    err = inflateEnd(zS);
			if (err != Z_OK)
			    return QByteArray();

		    unzipped.truncate(zS->total_out - totalUnzipped);

			delete zS;
			zS = NULL;
	    }
	    else if (err == Z_OK)
		{
			unzipped.truncate(zS->total_out - totalUnzipped);

			if((zS->avail_out == 0) && (zS->avail_in != 0))
				unzipped.append(ungzip_arr(zS, zipped, bGZip, iRecursion + 1));
		}
  	}
  	catch (...){
		err = Z_DATA_ERROR;
		ASSERT(0);
	}

	if(err != Z_OK)
		return QByteArray();
	return unzipped;
}

void clear_z(z_stream* &zS)
{
	inflateEnd(zS);
	delete zS;
	zS = NULL;
}