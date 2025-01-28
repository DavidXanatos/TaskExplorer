echo building %1

pushd %~dp0

mkdir %~dp0\Build_%1_%build_arch%
cd %~dp0\Build_%1_%build_arch%

%qt_path%\bin\qmake.exe %2 %qt_params%
%jom_path%\jom.exe -f Makefile.Release -j 8
REM IF %ERRORLEVEL% NEQ 0 goto :error

popd