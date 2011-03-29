@if exist "%ProgramFiles%/Microsoft Visual Studio 9.0/VC/bin/vcvars32.bat" call "%ProgramFiles%/Microsoft Visual Studio 9.0/VC/bin/vcvars32.bat"
@if exist "%ProgramFiles(x86)%/Microsoft Visual Studio 9.0/VC/bin/vcvars32.bat" call "%ProgramFiles(x86)%/Microsoft Visual Studio 9.0/VC/bin/vcvars32.bat"
cd /d %~dp0
devenv polyakf.sln /Build Release
cd Release
polyakf.exe
pause