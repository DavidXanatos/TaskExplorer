#include <QCoreApplication>
#include <QtCore>
#include <QDir>

#include "testchunks.h"


int main()
{
    QDir dir("logs");
    dir.setNameFilters(QStringList() << "*.*");
    dir.setFilter(QDir::Files);
    foreach(QString dirFile, dir.entryList())
        dir.remove(dirFile);

    QString logFilename = "logs/Summary.log";
    QFile outFile(logFilename);
    outFile.open(QIODevice::WriteOnly | QIODevice::Text);
    QTextStream sumLog(&outFile);

    TestChunks tc(sumLog, "overwrite", 0x4000, true);
    tc.overwrite(4379, 25);
    tc.overwrite(0, '.');
    tc.overwrite(0x50, '.');
    tc.overwrite(0x100, '.');
    tc.overwrite(0xfff, '.');
    tc.overwrite(0x1000, '.');
    tc.overwrite(0x1fff, '.');
    tc.overwrite(0x3000, '.');
    tc.overwrite(0x3fff, '.');
    tc.overwrite(0x2000, '.');
    tc.overwrite(0x2fff, '.');

    TestChunks tc2(sumLog, "insert", 0x4000, true);
    tc2.insert(0, 'E');        // 0
    tc2.insert(0x50, 'x');     // 1
    tc2.insert(0x100, 'x');    // 2
    tc2.insert(0x1002, 'L');   // 3
    tc2.insert(0x1004, 'E');   // 4
    tc2.insert(0x2004, 'L');   // 5
    tc2.insert(0x4005, 'L');   // 6
    tc2.insert(0x3007, 'E');   // 7
    tc2.insert(0x2008, 'E');   // 8
    tc2.insert(0x3008, 'L');   // 9

    TestChunks tc3(sumLog, "remove", 0x4000, true);
    tc3.removeAt(0);           // 0
    tc3.removeAt(0x50);        // f
    tc3.removeAt(0x100);       // e
    tc3.removeAt(0xffc);       // d
    tc3.removeAt(0xffc);       // c
    tc3.removeAt(0x1ffa);      // b
    tc3.removeAt(0x3ff9);      // a
    tc3.removeAt(0x2ffa);      // 9
    tc3.removeAt(0x2ff7);      // 8
    tc3.removeAt(0x1ff7);      // 7

    TestChunks tc4(sumLog, "random", 0x40000, true);
    tc4.random(1000);

    outFile.close();
    return 0;
}
