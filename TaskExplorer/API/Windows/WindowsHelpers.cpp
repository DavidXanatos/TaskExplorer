#include "stdafx.h"
#include "WindowsAPI.h"
#include <Shlwapi.h>

static QString expandEnvStrings(const QString &command)
{
    wchar_t commandValue[2 * MAX_PATH] = {0};
    DWORD returnValue = ExpandEnvironmentStringsW(command.toStdWString().c_str(), commandValue, 2 * MAX_PATH - 1);
    if (returnValue)
        return QString::fromWCharArray(commandValue);
    return command;
}

QString GetFileName(QString FilePath)
{
	int pos = FilePath.lastIndexOf("\\");
	return FilePath.mid(pos + 1);
}

QString GetPathFromCmdLine(QString CommandLine)
{
	if (CommandLine[0] == '"')
	{
		int pos = CommandLine.indexOf('"', 1);
		if (pos != -1)
			return CommandLine.mid(1, pos - 1);
	}
	else
	{
		int pos = CommandLine.indexOf(' ');
		if (pos != -1)
			return CommandLine.mid(0, pos);
	}
    return CommandLine;
}

QString pathAppend(const QString& path1, const QString& path2)
{
    return QDir::cleanPath(path1 + QDir::separator() + path2).replace("/", "\\");
}

QString FindInPath(QString filePath)
{
	if(filePath.length() > 2 * MAX_PATH - 1)
		return QString();

	wchar_t pathValue[2 * MAX_PATH] = {0};
	filePath.toWCharArray(pathValue);
	if (PathFindOnPath(pathValue, NULL))
		return QString::fromWCharArray(pathValue);
	return QString();
}

QString FindExeInPath(QString filePath)
{
	QString fullPath = FindInPath(filePath );
	if (!fullPath.isEmpty())
		fullPath;
	return FindInPath(filePath + ".exe");
}

QString GetPathFromCmd(QString CommandLine, quint32 ProcessID, QString FileName/*, DateTime timeStamp*/, quint32 ParentID)
{
	if (CommandLine.isEmpty())
		return QString();

	QString filePath = GetPathFromCmdLine(CommandLine);

	// apparently some processes can be started without a exe name in the command line WTF, anyhow:
	if (GetFileName(filePath).compare(FileName, Qt::CaseInsensitive) == 0 
	&& (GetFileName(filePath) + ".exe").compare(FileName, Qt::CaseInsensitive) == 0)
		filePath = FileName;
            
	// https://reverseengineering.stackexchange.com/questions/3798/c-question-marks-in-paths
	// \?? is a "fake" prefix which refers to per-user Dos devices
	if (filePath.indexOf("\\??\\") == 0)
		filePath = filePath.mid(4);

	filePath = expandEnvStrings(filePath);
					
	if (!PathIsRelativeW(filePath.toStdWString().c_str()))
		return filePath;

	CProcessPtr pParent = theAPI->GetProcessByID(ParentID);
	if (pParent.isNull())
		return QString();

    // check relative paths based on the parrent process working directory
    QString workingDir = pParent->GetWorkingDirectory();
    if (!workingDir.isEmpty())
    {
        QString curPath = pathAppend(workingDir, filePath);
        /*if (filePath[0] == '.')
            curPath = Path.GetFullPath(curPath);*/

        if (QFile::exists(curPath))
            return curPath;
        if (QFile::exists(curPath + ".exe"))
            return curPath + ".exe";
    }

    // if everythign else fails, try to find the process binary using the environment path variable
	return FindExeInPath(filePath);
}