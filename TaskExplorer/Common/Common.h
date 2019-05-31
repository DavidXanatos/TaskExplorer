#pragma once

QString FormatSize(quint64 Size);
QString	FormatTime(quint64 Time);


void GrayScale (QImage& Image);

QAction* MakeAction(QToolBar* pParent, const QString& IconFile, const QString& Text = "");
QMenu* MakeMenu(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QMenu* pParent, const QString& Text, const QString& IconFile = "");
QAction* MakeAction(QActionGroup* pGroup, QMenu* pParent, const QString& Text, const QVariant& Data);