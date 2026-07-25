set version=1.8.0

set inno_path=%~dp0.\InnoSetup
mkdir %~dp0.\Output

"%inno_path%\ISCC.exe" /O%~dp0.\Output %~dp0.\TaskExplorer.iss /DMyAppVersion=%version%

pause