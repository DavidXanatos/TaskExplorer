#pragma once
#include "../../zlib/zlib.h"


QByteArray Pack(const QByteArray& Data);
QByteArray Unpack(const QByteArray& Data);

bool gzip_arr(QByteArray& in);
bool IsgZiped(const QByteArray& zipped);
QByteArray ungzip_arr(z_stream* &zS, QByteArray& zipped, bool bGZip = true, int iRecursion = 0);

void clear_z(z_stream* &zS);
