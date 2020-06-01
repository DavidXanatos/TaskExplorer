#pragma once
#include "../../zlib/zlib.h"


#include "../mischelpers_global.h"


QByteArray MISCHELPERS_EXPORT Pack(const QByteArray& Data);
QByteArray MISCHELPERS_EXPORT Unpack(const QByteArray& Data);

bool MISCHELPERS_EXPORT gzip_arr(QByteArray& in);
bool MISCHELPERS_EXPORT IsgZiped(const QByteArray& zipped);
QByteArray MISCHELPERS_EXPORT ungzip_arr(z_stream* &zS, QByteArray& zipped, bool bGZip = true, int iRecursion = 0);

void MISCHELPERS_EXPORT clear_z(z_stream* &zS);
