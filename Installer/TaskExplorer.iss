; Use commandline to populate:
; ISCC.exe /ORelease TaskExplorer.iss /DMyAppVersion=%Version%
;

#define MyAppName       "TaskExplorer"
#define CurrentYear     GetDateTimeString('yyyy','','')
; #define MyAppVersion    "1.6.0"
#define MyAppAuthor     "DavidXanatos (xanasoft.com)"
#define MyAppCopyright  "(c) 2019-" + CurrentYear + " " + MYAppAuthor
#define MyAppURL        "https://github.com/DavidXanatos/TaskExplorer"

#include "Languages.iss"


[Setup]
AppId={#MyAppName}

AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}

VersionInfoDescription={#MyAppName} installer
VersionInfoProductName={#MyAppName}
VersionInfoVersion={#MyAppVersion}
VersionInfoCopyright={#MyAppCopyright}

AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

AppCopyright={#MyAppCopyright}

UninstallDisplayName={#MyAppName} {#MyAppVersion}
UninstallDisplayIcon={app}\TaskExplorer.exe
AppPublisher={#MyAppAuthor}

AppMutex=TASKEXPLORER_MUTEX
DefaultDirName={pf}\{#MyAppName}
DefaultGroupName={#MyAppName}
Uninstallable=not IsPortable
OutputBaseFilename={#MyAppName}-v{#MyAppVersion}
Compression=lzma
ArchitecturesAllowed=x86 x64 arm64
ArchitecturesInstallIn64BitMode=x64 arm64
AllowNoIcons=yes
AlwaysRestart=no
LicenseFile=.\Resources\license.txt
UsedUserAreasWarning=no
SetupIconFile=TaskExplorerInstall.ico
;SignTool=sha256

; Handled in code section as always want DirPage for portable mode.
DisableDirPage=no

; Allow /CURRENTUSER to be used with /PORTABLE=1 to avoid admin requirement.
PrivilegesRequiredOverridesAllowed=commandline


[Tasks]
Name: "DesktopIcon"; Description: "{cm:CreateDesktopIcon}"; MinVersion: 0.0,5.0; Check: not IsPortable
;Name: "AutoStartEntry"; Description: "{cm:AutoStartProgram,{#MyAppName}}"; MinVersion: 0.0,5.0; Check: not IsPortable


[Files]
Source: ".\Build\*"; DestDir: "{app}"; MinVersion: 0.0,5.0; Flags: recursesubdirs ignoreversion;

; other files
;Source: "license.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 
;Source: "changelog.txt"; DestDir: "{app}"; MinVersion: 0.0,5.0; 

; Only if portable.
Source: ".\TaskExplorer.ini"; DestDir: "{app}\x64"; Flags: ignoreversion onlyifdoesntexist; Check: IsPortable

[Icons]
Name: "{group}\TaskExplorer"; Filename: "{app}\TaskExplorer.exe"; MinVersion: 0.0,5.0; 
;Name: "{group}\{cm:License}"; Filename: "{app}\license.txt"; MinVersion: 0.0,5.0; 
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"; MinVersion: 0.0,5.0; 
Name: "{userdesktop}\TaskExplorer"; Filename: "{app}\TaskExplorer.exe"; Tasks: desktopicon; MinVersion: 0.0,5.0; 


[INI]
; Set language.
Filename: "{localappdata}\{#MyAppName}\{#MyAppName}.ini"; Section: "Options"; Key: "UiLanguage"; String: "{code:AppLanguage|{language}}"; Check: (not IsPortable) and (not IsUpgrade)
Filename: "{app}\{#MyAppName}.ini"; Section: "Options"; Key: "UiLanguage"; String: "{code:AppLanguage|{language}}"; Check: IsPortable


[InstallDelete]
; Remove deprecated files at install time.
Type: filesandordirs; Name: "{app}\translations"


[Registry]
; Autostart App.
;Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; ValueName: "TaskExplorer_AutoRun"; ValueType: string; ValueData: """{app}\TaskExplorer.exe"" -autorun"; Flags: uninsdeletevalue; Tasks: AutoStartEntry


[Run]
; Start TaskExplorer.
Filename: "{app}\TaskExplorer.exe"; Parameters: ""; Description: "Start TaskExplorer"; StatusMsg: "Launch TaskExplorer..."; Flags: postinstall nowait runascurrentuser; Check: IsOpenApp
;Filename: "{app}\TaskExplorer.exe"; Parameters: "-autorun"; StatusMsg: "Launch TaskExplorer..."; Flags: runasoriginaluser nowait; Check: not IsPortable


[UninstallDelete]
Type: dirifempty; Name: "{app}"
Type: dirifempty; Name: "{localappdata}\{#MyAppName}"


[Messages]
; Include with commandline /? message.
HelpTextNote=/PORTABLE=1%nEnable portable mode.%n


[Code]
var
  CustomPage: TInputOptionWizardPage;
  
  IsInstalled: Boolean;
  Portable: Boolean;

  // The server service, if it was running when setup started and has been
  // stopped so its files could be replaced. Only then is it started again.
  StoppedService: String;


function IsPortable(): Boolean;
begin

  // Return True or False for the value of Check.
  if (ExpandConstant('{param:portable|0}') = '1') or Portable then begin
    Result := True;
  end;
end;

//function InstallX64: Boolean;
//begin
//  Result := Is64BitInstallMode and (ProcessorArchitecture = paX64);
//end;
//
//function InstallARM64: Boolean;
//begin
//  Result := Is64BitInstallMode and (ProcessorArchitecture = paARM64);
//end;
//
//function InstallOtherArch: Boolean;
//begin
//  Result := not InstallX64 and not InstallARM64;
//end;

function IsOpenApp(): Boolean;
begin

  // Return True or False for the value of Check.
  if (ExpandConstant('{param:open_agent|0}') = '1') or ((not IsPortable) and (not WizardSilent)) then begin
    Result := True;
  end;
end;

function IsUpgrade(): Boolean;
var
  S: String;
  InnoSetupReg: String;
  AppPathName: String;
begin

  // Detect if already installed.
  // Source: https://stackoverflow.com/a/30568071
  InnoSetupReg := ExpandConstant('Software\Microsoft\Windows\CurrentVersion\Uninstall\{#SetupSetting("AppName")}_is1');
  AppPathName := 'Inno Setup: App Path';

  Result := RegQueryStringValue(HKLM, InnoSetupReg, AppPathName, S) or
            RegQueryStringValue(HKCU, InnoSetupReg, AppPathName, S);
end;


function AppLanguage(Language: String): String;
begin

  // Language values for TaskExplorer.ini.
  case Lowercase(Language) of
    'english': Result := 'en';
    'chinesesimplified': Result := 'zh_CN';
    'chinesetraditional': Result := 'zh_TW';
    'dutch': Result := 'nl';
    'french': Result := 'fr';
    'german': Result := 'de';
    'hungarian': Result := 'hu';
    'italian': Result := 'it';
    'japanese': Result := 'ja';
    'korean': Result := 'ko';
    'polish': Result := 'pl';
    'brazilianportuguese': Result := 'pt_BR';
    'portuguese': Result := 'pt_PT';
    'russian': Result := 'ru';
    'spanish': Result := 'es';
    'swedish': Result := 'sv_SE';
    'turkish': Result := 'tr';
    'ukrainian': Result := 'uk';
    'vietnamese': Result := 'vi';
  end;
end;


function SystemLanguage(Dummy: String): String;
begin

  // Language identifier for the System Eventlog messages.
  Result := IntToStr(GetUILanguage());
end;

procedure UpdateStatus(OutputProgressPage: TOutputProgressWizardPage; Text: String; Percentage: Integer);
begin

  // Called by ShutdownApp() to update status or progress.
  if IsUninstaller() then
    UninstallProgressForm.StatusLabel.Caption := Text
  else begin
    OutputProgressPage.SetProgress(Percentage, 100);
    OutputProgressPage.SetText(Text, '');
  end;

  // Output status information to log.
  Log('Debug: ' + Text);
end;


function ShutdownApp(): Boolean;
var
  ExecRet: Integer;
  StatusText: String;
  OutputProgressPage: TOutputProgressWizardPage;
begin

  // todo

  Result := True;
end;


//////////////////////////////////////////////////////
// The server service
//
// TaskServer runs as a service, and for as long as it does it holds
// TaskServer.exe open - along with the driver library it loaded. Replacing
// those under a running daemon either fails outright or leaves the machine
// needing a reboot, so it is stopped before the files are copied and started
// again once they are in place.
//
// Which service, from the daemon's own configuration file. The instance name is
// configurable and *is* the service name, so it cannot be a constant here - but
// the daemon writes that name into the same file it reads it back from, and
// asking that file is one line.
//

const
  SC_MANAGER_CONNECT    = $0001;
  SERVICE_QUERY_STATUS  = $0004;
  SERVICE_START         = $0010;
  SERVICE_STOP          = $0020;

  SERVICE_CONTROL_STOP  = $00000001;

  SERVICE_STOPPED       = $00000001;
  SERVICE_STOP_PENDING  = $00000003;
  SERVICE_RUNNING       = $00000004;

  SERVICE_WAIT_MS       = 30000;

type
  TServiceStatus = record
    dwServiceType: Cardinal;
    dwCurrentState: Cardinal;
    dwControlsAccepted: Cardinal;
    dwWin32ExitCode: Cardinal;
    dwServiceSpecificExitCode: Cardinal;
    dwCheckPoint: Cardinal;
    dwWaitHint: Cardinal;
  end;

function OpenSCManager(lpMachineName, lpDatabaseName: String; dwDesiredAccess: Cardinal): THandle;
  external 'OpenSCManagerW@advapi32.dll stdcall';

function OpenService(hSCManager: THandle; lpServiceName: String; dwDesiredAccess: Cardinal): THandle;
  external 'OpenServiceW@advapi32.dll stdcall';

function CloseServiceHandle(hSCObject: THandle): Boolean;
  external 'CloseServiceHandle@advapi32.dll stdcall';

function QueryServiceStatus(hService: THandle; var lpServiceStatus: TServiceStatus): Boolean;
  external 'QueryServiceStatus@advapi32.dll stdcall';

function ControlService(hService: THandle; dwControl: Cardinal; var lpServiceStatus: TServiceStatus): Boolean;
  external 'ControlService@advapi32.dll stdcall';

function StartService(hService: THandle; dwNumServiceArgs: Cardinal; lpServiceArgVectors: Cardinal): Boolean;
  external 'StartServiceW@advapi32.dll stdcall';


function ServerServiceName(): String;
begin
  Result := GetIniString('Server', 'Name', '',
    ExpandConstant('{commonappdata}\Xanasoft\TaskExplorer\TaskServer.ini'));

  // A blank field is somebody leaving it alone rather than asking for a service
  // with no name, which is the same reading CServerSetup::ServiceNameFor gives
  // it. No file at all says the same thing.
  if Result = '' then
    Result := 'TaskExplorerServer';

  Log('Service: the configured server instance is "' + Result + '".');
end;

// Kept for reference: finding the daemon by what it runs rather than by what the
// configuration says it is called. It reads every service's ImagePath and takes
// the ones naming the TaskServer.exe in this directory, which also catches
// several instances at once and ignores a daemon installed somewhere else. That
// is more than this needs.
//
// function FindServerServices(): TArrayOfString;
// var
//   Names: TArrayOfString;
//   ImagePath, Binary: String;
//   I, Count: Integer;
// begin
//   SetArrayLength(Result, 0);
//
//   Binary := Lowercase(AddBackslash(ExpandConstant('{app}')) + 'TaskServer.exe');
//
//   if not RegGetSubkeyNames(HKLM, 'SYSTEM\CurrentControlSet\Services', Names) then
//   begin
//     Log('Service: could not enumerate the service list.');
//     exit;
//   end;
//
//   for I := 0 to GetArrayLength(Names) - 1 do
//   begin
//     if RegQueryStringValue(HKLM, 'SYSTEM\CurrentControlSet\Services\' + Names[I], 'ImagePath', ImagePath) then
//     begin
//       if Pos(Binary, Lowercase(ImagePath)) > 0 then
//       begin
//         Count := GetArrayLength(Result);
//         SetArrayLength(Result, Count + 1);
//         Result[Count] := Names[I];
//       end;
//     end;
//   end;
// end;


function StopServerService(const Name: String; var WasRunning: Boolean): Boolean;
var
  hSCM, hSvc: THandle;
  Status: TServiceStatus;
  Waited: Integer;
begin
  Result := False;
  WasRunning := False;

  hSCM := OpenSCManager('', 'ServicesActive', SC_MANAGER_CONNECT);
  if hSCM = 0 then
  begin
    Log('Service: could not open the service manager.');
    exit;
  end;

  hSvc := OpenService(hSCM, Name, SERVICE_QUERY_STATUS or SERVICE_STOP);
  if hSvc = 0 then
  begin
    // Gone between the enumeration and here, or not ours to stop. Either way
    // there is nothing holding a file.
    Log('Service: "' + Name + '" could not be opened, skipping.');
    CloseServiceHandle(hSCM);
    Result := True;
    exit;
  end;

  if QueryServiceStatus(hSvc, Status) then
  begin
    if Status.dwCurrentState = SERVICE_STOPPED then
    begin
      // Already stopped, and deliberately so as far as this setup knows. It
      // will not be started again afterwards.
      Result := True;
    end
    else
    begin
      WasRunning := True;
      Log('Service: stopping "' + Name + '".');

      if Status.dwCurrentState <> SERVICE_STOP_PENDING then
        ControlService(hSvc, SERVICE_CONTROL_STOP, Status);

      // The request only asks; the process still has to come down and release
      // what it had open, and only then is the file free to be replaced.
      Waited := 0;
      while Waited < SERVICE_WAIT_MS do
      begin
        if not QueryServiceStatus(hSvc, Status) then
          break;
        if Status.dwCurrentState = SERVICE_STOPPED then
          break;
        Sleep(250);
        Waited := Waited + 250;
      end;

      Result := (Status.dwCurrentState = SERVICE_STOPPED);
      if Result then
        Log('Service: "' + Name + '" stopped.')
      else
        Log('Service: "' + Name + '" did not stop within ' + IntToStr(SERVICE_WAIT_MS div 1000) + ' seconds.');
    end;
  end
  else
    Log('Service: could not query "' + Name + '".');

  CloseServiceHandle(hSvc);
  CloseServiceHandle(hSCM);
end;


function StartServerService(const Name: String): Boolean;
var
  hSCM, hSvc: THandle;
begin
  Result := False;

  hSCM := OpenSCManager('', 'ServicesActive', SC_MANAGER_CONNECT);
  if hSCM = 0 then
    exit;

  hSvc := OpenService(hSCM, Name, SERVICE_START);
  if hSvc <> 0 then
  begin
    Result := StartService(hSvc, 0, 0);
    if Result then
      Log('Service: "' + Name + '" started again.')
    else
      Log('Service: could not start "' + Name + '" again.');
    CloseServiceHandle(hSvc);
  end;

  CloseServiceHandle(hSCM);
end;


function StopServerDaemon(var Message: String): Boolean;
var
  Name: String;
  WasRunning: Boolean;
begin
  Message := '';
  StoppedService := '';

  Name := ServerServiceName();
  Result := StopServerService(Name, WasRunning);

  if not Result then
    // Carrying on from here would mean copying over files the daemon still has
    // open, which fails or defers to a reboot. Better to say so.
    Message := 'The TaskExplorer server service "' + Name + '" could not be stopped.' + #13#10 +
               'Stop it manually and run setup again.'
  else if WasRunning then
    StoppedService := Name;
end;


procedure StartServerDaemon();
begin
  if StoppedService <> '' then
    StartServerService(StoppedService);

  StoppedService := '';
end;


//////////////////////////////////////////////////////
// Installation Events
//


function NextButtonClick(CurPageID: Integer): Boolean;
var
  ExecRet: Integer;
begin

  // Get mode setting from Custom page and set path for the Dir page.
  if CurPageID = CustomPage.ID then begin
    Portable := not (CustomPage.SelectedValueIndex = 0);
    // WizardForm.DirEdit.Text := InstallPath('');

    // No Start Menu folder setting on Ready page if portable.
    if Portable then begin
      WizardForm.NoIconsCheck.Checked := True;
    end else begin
      WizardForm.NoIconsCheck.Checked := False;
    end;
  end;

  // Shutdown service, driver and processes as ready to install.
  if ((CurPageID = wpReady) and (not IsPortable())) then
  begin

    // Stop processes.
    Result := ShutdownApp();
    exit;
  end;

  Result := True;
end;


function ShouldSkipPage(PageID: Integer): Boolean;
begin

  // Skip Custom page and Group page if portable.
  if PageID = CustomPage.ID then begin
    if ExpandConstant('{param:portable|0}') = '1' then
      Result := True;
  end else if PageID = wpSelectDir then begin
    if not IsPortable and IsUpgrade then
      Result := True;
  end else if PageID = wpSelectProgramGroup then begin
    if IsPortable then
      Result := True;
  end;
end;


procedure OpenLink(Sender: TObject);
var
    ErrCode: integer;
begin
  ShellExec('open', 'http://xanasoft.com/', '', '', SW_SHOW, ewNoWait, ErrCode);
end;

procedure AddFooterLink();
var
  FooterLinkLabel: TLabel;
begin
  // Create a new label in the footer area (far-left aligned)
  FooterLinkLabel := TLabel.Create(WizardForm);
  FooterLinkLabel.Parent := WizardForm;
  FooterLinkLabel.Caption := 'Visit our website!';
  FooterLinkLabel.Font.Style := [fsUnderline]; // Underline for hyperlink effect
  FooterLinkLabel.Font.Color := clBlue; // Blue color like a hyperlink
  FooterLinkLabel.Cursor := crHand; // Hand cursor to indicate it's clickable
  FooterLinkLabel.OnClick := @OpenLink; // Attach click event

  // Position the label **fully left-aligned**
  FooterLinkLabel.Left := 10; // Place it at the far left side
  FooterLinkLabel.Top := WizardForm.NextButton.Top + 5; // Align with footer height
  FooterLinkLabel.AutoSize := True;
end;


procedure InitializeWizard();
begin

  // Create the custom page.
  // Source: https://timesheetsandstuff.wordpress.com/2008/06/27/the-joy-of-part-2/
  CustomPage := CreateInputOptionPage(wpLicense,
                                      CustomMessage('CustomPageLabel1'),
                                      CustomMessage('CustomPageLabel2'),
                                      CustomMessage('CustomPageLabel3'), True, False);

  if IsInstalled = True then begin
    CustomPage.Add(CustomMessage('CustomPageUpgradeMode'));
  end else begin
    CustomPage.Add(CustomMessage('CustomPageInstallMode'));
  end;

  CustomPage.Add(CustomMessage('CustomPagePortableMode'));
  
  // Default to Normal Installation if not argument /PORTABLE=1.
  if ExpandConstant('{param:portable|0}') = '1' then begin
    WizardForm.NoIconsCheck.Checked := True;
    CustomPage.SelectedValueIndex := 1;
  end else begin
    CustomPage.SelectedValueIndex := 0;
  end;
  
  AddFooterLink(); // Add the footer link at wizard initialization
  
end;


function InitializeSetup(): Boolean;
var
  Version: TWindowsVersion;
  UninstallString: String;
begin

  // Require Windows 7 or later.
  GetWindowsVersionEx(Version);

  if (Version.NTPlatform = False) or (Version.Major < 6) then
  begin
    SuppressibleMsgBox(CustomMessage('RequiresWin7OrLater'), mbError, MB_OK, MB_OK);
    Result := False;
    exit;
  end;

  Result := True;
end;


procedure CurStepChanged(CurStep: TSetupStep);
begin

  // The files are in place: start what was stopped to put them there. Only what
  // was actually running - a daemon somebody had deliberately stopped stays
  // stopped.
  if CurStep = ssPostInstall then
    StartServerDaemon();

end;


//////////////////////////////////////////////////////
// Uninstallation Events
//


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExecRet: Integer;
  Message: String;
begin

  // Before the uninstallation.
  if (CurUninstallStep <> usUninstall) then
    exit;

  // Shutdown service, driver and processes.
  if (ShutdownApp() = False) then
  begin
    Abort();
    exit;
  end;

  // The server service, if one is running out of this directory. Not started
  // again afterwards, for obvious reasons - this is the same call the install
  // makes, and what it stops here is what would otherwise keep the files it is
  // about to remove open.
  StopServerDaemon(Message);

end;




//////////////////////////////////////////////////////
// kte.dll handling
//

procedure PrepareOneDllForUpdate(const FileName: String);
var
  AppDir, DllPath, OldPath: String;
  DeletedOld, DeletedDll, Renamed: Boolean;
begin
  AppDir := ExpandConstant('{app}');
  DllPath := AddBackslash(AppDir) + FileName;
  OldPath := AddBackslash(AppDir) + ChangeFileExt(FileName, '.old');

  try
    { Step 1: try to delete existing *.old }
    if FileExists(OldPath) then
    begin
      DeletedOld := DeleteFile(OldPath);
      if DeletedOld then
        Log('Deleted old backup: ' + OldPath)
      else
        Log('Could not delete old backup (will apply fallback): ' + OldPath);
    end
    else
      DeletedOld := True; { nothing to delete = OK }

    { Step 2: if *.old could NOT be deleted, delete live *.dll outright }
    if not DeletedOld then
    begin
      if FileExists(DllPath) then
      begin
        DeletedDll := DeleteFile(DllPath);
        if DeletedDll then
          Log('Fallback: deleted live DLL because .old was locked: ' + DllPath)
        else
          Log('Fallback failed: could not delete live DLL: ' + DllPath);
      end
      else
        DeletedDll := True; { already absent = OK }
    end;

    { Step 3: if *.dll still exists and *.old is gone, try rename dll -> old }
    if FileExists(DllPath) then
    begin
      if not FileExists(OldPath) then
      begin
        Renamed := RenameFile(DllPath, OldPath);
        if Renamed then
          Log('Renamed "' + DllPath + '" to "' + OldPath + '"')
        else
          Log('Rename failed (DLL may be held without delete-share): "' + DllPath + '" -> "' + OldPath + '"');
      end
      else
        Log('Cannot rename: backup still present: ' + OldPath);
    end;

  except
    Log('Exception in PrepareOneDllForUpdate for "' + FileName + '" (continuing).');
  end;
end;

function PrepareToInstall(var NeedsRestart: Boolean): String;
begin
  { The daemon first: it is holding TaskServer.exe, and the driver library the
    step below is about to move out of the way with it. Returning a message here
    stops the install before a single file has been touched. }
  if not StopServerDaemon(Result) then
    exit;

  { Handle both DLLs before file copy }
  PrepareOneDllForUpdate('AMD64\kte.dll');
  PrepareOneDllForUpdate('ARM64\kte.dll');
end;