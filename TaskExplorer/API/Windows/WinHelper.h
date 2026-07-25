#pragma once

//QVariantMap ResolveShortcut(const QString& LinkPath);

QPixmap LoadWindowsIcon(const QString& Path, quint32 Index);

QIcon LoadWindowsIconEx(const QString& Path, quint32 Index = 0);

bool PickWindowsIcon(QWidget* pParent, QString& Path, quint32& Index);

bool WindowsMoveFile(const QString& From, const QString& To);

bool OpenFileProperties(const QString& Path);

bool OpenFileFolder(const QString& Path);

bool CheckInternet();

QVariantList EnumNICs();

bool IsFullScreenMode();