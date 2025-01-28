#!define VERSION "0.0.1"
!define QT "C:\Qt\5.5\mingw492_32\bin\"
!define BUILD_DIR "C:\dev\qhexedit\build\release\"
!define TRANS_DIR "C:\dev\qhexedit\example\translations\"
!define OUTFILE_NAME "C:\dev\qhexedit\build\QHexEdit.exe"
!define INSTALLATIONNAME "QHexEdit"

;--------------------------------
;Include Modern UI

  !include "MUI2.nsh"

;--------------------------------
;General

  ;Name and file
  Name ${INSTALLATIONNAME}
  OutFile ${OUTFILE_NAME}

  SetCompressor lzma

  ;Default installation folder
  InstallDir "$PROGRAMFILES\QHexEdit"

  ;Get installation folder from registry if available
  InstallDirRegKey HKCU "Software\${INSTALLATIONNAME}" ""

  ;Request application privileges for Windows Vista
  !define MULTIUSER_EXECUTIONLEVEL Highest
  !define MULTIUSER_MUI
  !define MULTIUSER_INSTALLMODE_COMMANDLINE
  !include MultiUser.nsh

;--------------------------------
;Interface Settings

#  !define MUI_HEADERIMAGE
#  !define MUI_HEADERIMAGE_BITMAP "cm_header.bmp"
#  !define MUI_HEADERIMAGE_UNBITMAP "cm_un-header.bmp"
#  !define MUI_WELCOMEFINISHPAGE_BITMAP "cm_wizard.bmp"
#  !define MUI_UNWELCOMEFINISHPAGE_BITMAP "cm_un-wizard.bmp"
#  !define MUI_ICON "myIcons.ico"
#  !define MUI_UNICON "unicon.ico"

  !define MUI_ABORTWARNING

  ;Show all languages, despite user's codepage
#  !define MUI_LANGDLL_ALLLANGUAGES

;--------------------------------
;Language Selection Dialog Settings

  ;Remember the installer language
  !define MUI_LANGDLL_REGISTRY_ROOT "SHCTX"
  !define MUI_LANGDLL_REGISTRY_KEY "Software\${INSTALLATIONNAME}"
  !define MUI_LANGDLL_REGISTRY_VALUENAME "Installer Language"

  ;Start Menu Folder Page Configuration
  !define MUI_STARTMENUPAGE_REGISTRY_ROOT "SHCTX"
  !define MUI_STARTMENUPAGE_REGISTRY_KEY "Software\${INSTALLATIONNAME}"
  !define MUI_STARTMENUPAGE_REGISTRY_VALUENAME "Start Menu Folder"

;--------------------------------
;Pages
  !insertmacro MUI_PAGE_WELCOME
#  !insertmacro MUI_PAGE_LICENSE $(license)
  !insertmacro MUI_PAGE_COMPONENTS
  !insertmacro MULTIUSER_PAGE_INSTALLMODE
  !insertmacro MUI_PAGE_DIRECTORY
  !insertmacro MUI_PAGE_INSTFILES
  !insertmacro MUI_PAGE_FINISH

  !insertmacro MUI_UNPAGE_WELCOME
  !insertmacro MUI_UNPAGE_CONFIRM
  !insertmacro MUI_UNPAGE_INSTFILES
  !insertmacro MUI_UNPAGE_FINISH

;--------------------------------
;Languages

  !insertmacro MUI_LANGUAGE "English" ;first language is the default Language
  !insertmacro MUI_LANGUAGE "German"

;--------------------------------
;Reserve Files

  ;If you are using solid compression, files that are required before
  ;the actual installation should be stored first in the data block,
  ;because this will make your installer start faster.

  !insertmacro MUI_RESERVEFILE_LANGDLL

;--------------------------------
;Installer Sections

Section ""
  SetOutPath $INSTDIR
  File ${BUILD_DIR}qhexedit.exe
  File /r ${BUILD_DIR}*.dll
  File ${TRANS_DIR}*.qm
  WriteUninstaller $INSTDIR\uninstall.exe
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALLATIONNAME}" "DisplayName" "QHexEdit Installer"
  WriteRegStr SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALLATIONNAME}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALLATIONNAME}" "NoModify" 1
  WriteRegDWORD SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALLATIONNAME}" "NoRepair" 1
  WriteRegStr SHCTX "Software\${INSTALLATIONNAME}" "" $INSTDIR
SectionEnd

Section $(startmenu) Startmenu
  CreateDirectory "$SMPROGRAMS\${INSTALLATIONNAME}"
  CreateShortCut "$SMPROGRAMS\${INSTALLATIONNAME}\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\${INSTALLATIONNAME}\QHexEdit.lnk" "$INSTDIR\qhexedit.exe" "" "$INSTDIR\qhexedit.exe" 0
SectionEnd

Section /o $(desktop) Desktop
  CreateShortCut "$DESKTOP\QHexEdit.lnk" "$INSTDIR\qhexedit.exe" "" "$INSTDIR\qhexedit.exe" 0
SectionEnd

;--------------------------------
;Installer Functions

Function .onInit
  !insertmacro MULTIUSER_INIT
  !insertmacro MUI_LANGDLL_DISPLAY
FunctionEnd

;--------------------------------
;Descriptions

#LicenseLangString license ${LANG_ENGLISH} license.rtf
#LicenseLangString license ${LANG_GERMAN} license_de.rtf

LangString startmenu ${LANG_ENGLISH} "Add to Start Menu"
LangString startmenu ${LANG_GERMAN} "Eintrag ins Startmenü"
LangString DESC_Startmenu ${LANG_ENGLISH} "Add an Entry to the Start Menu"
LangString DESC_Startmenu ${LANG_GERMAN} "Einen Eintrag ins Startmenü hinzufügen"

LangString desktop ${LANG_ENGLISH} "Add a Desktop Icon"
LangString desktop ${LANG_GERMAN} "Zum Desktop hinzufügen"
LangString DESC_Desktop ${LANG_ENGLISH} "Add an Icon to the Desktop"
LangString DESC_Desktop ${LANG_GERMAN} "Einen Eintrag zum Desktop hinzufügen"

!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
   !insertmacro MUI_DESCRIPTION_TEXT ${Startmenu} $(DESC_Startmenu)
   !insertmacro MUI_DESCRIPTION_TEXT ${Desktop} $(DESC_Desktop)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

;--------------------------------
;Uninstaller Section

Section "Uninstall"
  DeleteRegKey SHCTX "Software\Microsoft\Windows\CurrentVersion\Uninstall\${INSTALLATIONNAME}"
  DeleteRegKey SHCTX "Software\${INSTALLATIONNAME}"
  RMDir /r $INSTDIR
  Delete "$SMPROGRAMS\${INSTALLATIONNAME}\Uninstall.lnk"
  Delete "$SMPROGRAMS\${INSTALLATIONNAME}\QHexEdit.lnk"
  Delete "$DESKTOP\QHexEdit.lnk"
  RMDir "$SMPROGRAMS\${INSTALLATIONNAME}"
SectionEnd

;--------------------------------
;Uninstaller Functions

Function un.onInit
  !insertmacro MULTIUSER_UNINIT
  !insertmacro MUI_UNGETLANGUAGE
FunctionEnd
