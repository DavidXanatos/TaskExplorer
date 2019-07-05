#ifndef TESTASBYTEARRAY_H
#define TESTASBYTEARRAY_H

#include <QBuffer>
#include <QByteArray>
#include <QString>
#include <QTextStream>

#include "../src/chunks.h"

class TestChunks
{
public:
    TestChunks(QTextStream &log, QString tName, int size, bool random=true, int saveFile=0x7fffffff);
    void insert(qint64 pos, char b);
    void overwrite(qint64 pos, char b);
    void removeAt(qint64 pos);
    void random(int count);
    void compare();


private:
    QByteArray _data, _highlighted, _copy;
    QBuffer _cData;
    Chunks _chunks;
    int _tCnt;
    QString _tName;
    int _saveFile;
    QTextStream *_log;
};

#endif // TESTASBYTEARRAY_H
