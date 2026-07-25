pushd %1
for /R %%F in ("*.dll" "*.exe") do (
    call %~dp0.\kph-sign-file.cmd "%%~fF"
)
popd