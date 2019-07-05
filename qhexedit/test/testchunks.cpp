#include "testchunks.h"
#include <cstdlib>


TestChunks::TestChunks(QTextStream &log, QString tName, int size, bool random, int saveFile) : _chunks(nullptr)
{
    char hex[] = "0123456789abcdef";
    srand(0);
    for (int idx=0; idx < size; idx++)
    {
        if (random)
            _data += char(rand() % 0x100);
        else
            _data += hex[idx & 0x0f];
        _highlighted += char(0);
    }
    _copy = _data;
    _cData.setData(_copy);
    _chunks.setIODevice(_cData);
    _tCnt = 0;
    _log = &log;
    _tName = tName;
    _saveFile = saveFile;
    compare();
}

void TestChunks::random(int count)
{
    for (int idx=1; idx < count; idx++)
    {
        int action = rand() % 3;
        int pos = rand() % _data.size();
        char b = char(rand() % 0x100);
        switch (action)
        {
        case 0:
            removeAt(pos);
            break;
        case 1:
            insert(pos, b);
            break;
        case 2:
            overwrite(pos, b);
            break;
        }
    }
}

void TestChunks::insert(qint64 pos, char b)
{
    _data.insert((int)pos, b);
    _copy.insert((int)pos, char(0));
    _highlighted.insert((int)pos, 1);
    _chunks.insert(pos, b);
    compare();
}

void TestChunks::overwrite(qint64 pos, char b)
{
    _data[(int)pos] = b;
    _highlighted[(int)pos] = 1;
    _chunks.overwrite(pos, b);
    compare();
}

void TestChunks::removeAt(qint64 pos)
{
    _data.remove((int)pos, 1);
    _highlighted.remove((int)pos, 1);
    _chunks.removeAt(pos);
    compare();
}

void TestChunks::compare()
{
    QByteArray rHighLighted;
    QByteArray rData = _chunks.data(0, -1, &rHighLighted);
    bool error = false;

    if (rData != _data)
        error = true;
    if (rHighLighted != _highlighted)
        error = true;

    _tCnt += 1;

    int chunkSize = _chunks.chunkSize();
    QString tName = QString("logs/%1_%2_%3").arg(_tName).arg(_tCnt).arg(chunkSize);
    if (error || (_tCnt >= _saveFile))
    {
        QFile file1(tName + "_data.txt");
        file1.open(QIODevice::WriteOnly);
        file1.write(_data);
        file1.close();

        QFile file2(tName + "_highlighted.txt");
        file2.open(QIODevice::WriteOnly);
        file2.write(_highlighted);
        file2.close();

        QFile file3(tName + "_rData.txt");
        file3.open(QIODevice::WriteOnly);
        file3.write(rData);
        file3.close();

        QFile file4(tName + "_rHighlighted.txt");
        file4.open(QIODevice::WriteOnly);
        file4.write(rHighLighted);
        file4.close();
    }

    if (error)
    {
        qDebug() << "NOK " << tName;
        *_log << "NOK " << tName << "\n";
    }
    else
    {
        qDebug() << "OK " << tName;
        *_log << "OK " << tName << "\n";
    }

}
