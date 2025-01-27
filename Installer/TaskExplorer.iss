[Setup]
AppName=TaskExplorer
AppVerName=TaskExplorer v{#MyAppVersion}
AppId=TaskExplorer
AppVersion={#MyAppVersion}
AppPublisher=https://github.com/DavidXanatos/TaskExplorer
AppPublisherURL=https://github.com/DavidXanatos/TaskExplorer
AppMutex=TASKEXPLORER_MUTEX
DefaultDirName={pf}\TaskExplorer
DefaultGroupName=TaskExplorer
UninstallDisplayIcon={app}\x86\TaskExplorer.exe
OutputBaseFilename=TaskExplorer-v{#MyAppVersion}
Compression=lzma
ArchitecturesAllowed=x86 x64
ArchitecturesInstallIn64BitMode=x64
AllowNoIcons=yes
LicenseFile=license.txt
;WizardImageFile=WizardImage0.bmp
;WizardSmallImageFile=WizardSmallImage0.bmp

[Files]
Source: ".\Build\*"; DestDir: "{app}"; MinVersion: 0.0,5.0; Flags: recursesubdirs ignoreversion;


; other files
Source: "license.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 
;Source: "changelog.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 

[Icons]
Name: "{group}\TaskExplorer"; Filename: "{app}\x86\TaskExplorer.exe"; MinVersion: 0.0,5.0; 
Name: "{group}\License"; Filename: "{app}\license.txt"; MinVersion: 0.0,5.0; 
Name: "{group}\Uninstall TaskExplorer"; Filename: "{uninstallexe}"; MinVersion: 0.0,5.0; 
Name: "{userdesktop}\TaskExplorer"; Filename: "{app}\x86\TaskExplorer.exe"; Tasks: desktopicon; MinVersion: 0.0,5.0; 

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop icon"; MinVersion: 0.0,5.0; 

[Run]
Filename: {app}\x86\TaskExplorer.exe; Description: {cm:LaunchProgram}; Flags: nowait postinstall skipifsilent

[CustomMessages]
LaunchProgram=Start TaskExplorer now

[Languages]

