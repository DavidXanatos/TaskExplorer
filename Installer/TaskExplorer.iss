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
Name: "{group}\{cm:UninstallProgram}"; Filename: "{uninstallexe}"; MinVersion: 0.0,5.0; 
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


end;


//////////////////////////////////////////////////////
// Uninstallation Events
//


procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  ExecRet: Integer;
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
  Result := '';
  { Handle both DLLs before file copy }
  PrepareOneDllForUpdate('AMD64\kte.dll');
  PrepareOneDllForUpdate('ARM64\kte.dll');
end;