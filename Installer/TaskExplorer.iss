#define MyAppName       "TaskExplorer"
;#define MyAppVersion   "1.6.0"
#define MyAppAuthor     "DavidXanatos"
#define CurrentYear     GetDateTimeString('yyyy','','')
#define MyAppCopyright  "(c) 2019-" + CurrentYear + " " + MYAppAuthor
#define MyAppURL         "https://github.com/DavidXanatos/TaskExplorer"

[Setup]
AppId={#MyAppName}

AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}

VersionInfoDescription={#MyAppName} installer
VersionInfoProductName={#MyAppName}
VersionInfoVersion={#MyAppVersion}

AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

AppCopyright={#MyAppCopyright}

UninstallDisplayName=T{#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\x86\TaskExplorer.exe
AppPublisher={#MyAppAuthor}

ShowLanguageDialog=yes
UsePreviousLanguage=no
LanguageDetectionMethod=uilanguage

AppMutex=TASKEXPLORER_MUTEX
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
OutputBaseFilename={#MyAppName}-v{#MyAppVersion}
Compression=lzma
ArchitecturesAllowed=x86 x64
ArchitecturesInstallIn64BitMode=x64
AllowNoIcons=yes
LicenseFile=license.txt
;WizardImageFile=WizardImage0.bmp
;WizardSmallImageFile=WizardSmallImage0.bmp

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl";
Name: "german";  MessagesFile: "compiler:Languages\German.isl";
Name: "italian"; MessagesFile: "compiler:Languages\Italian.isl";
Name: "polish";  MessagesFile: "compiler:Languages\Polish.isl";

[CustomMessages]
english.LaunchProgram=Start TaskExplorer now
english.License=License
english.Uninstall=Uninstall TaskExplorer
english.DesktopIcon=Create a &desktop icon

italian.LaunchProgram=Esegui TaskExplorer
italian.License=Licenza
italian.Uninstall=Disinstalla TaskExplorer
italian.DesktopIcon=Crea un'icona sul &desktop

[Files]
Source: ".\Build\*"; DestDir: "{app}"; MinVersion: 0.0,5.0; Flags: recursesubdirs ignoreversion;


; other files
Source: "license.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 
;Source: "changelog.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 

[Icons]
Name: "{group}\TaskExplorer"; Filename: "{app}\x86\TaskExplorer.exe"; MinVersion: 0.0,5.0; 
Name: "{group}\{cm:License}"; Filename: "{app}\license.txt"; MinVersion: 0.0,5.0; 
Name: "{group}\{cm:Uninstall}"; Filename: "{uninstallexe}"; MinVersion: 0.0,5.0; 
Name: "{userdesktop}\TaskExplorer"; Filename: "{app}\x86\TaskExplorer.exe"; Tasks: desktopicon; MinVersion: 0.0,5.0; 

[Tasks]
Name: "desktopicon"; Description: "{cm:DesktopIcon}"; MinVersion: 0.0,5.0; 

[Run]
Filename: {app}\x86\TaskExplorer.exe; Description: {cm:LaunchProgram}; Flags: nowait postinstall skipifsilent

