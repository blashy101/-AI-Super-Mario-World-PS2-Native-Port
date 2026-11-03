@echo off
setlocal
pushd "%~dp0"
if "%~1"=="" (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools\Start_Windows.ps1"
) else (
  powershell.exe -NoProfile -ExecutionPolicy Bypass -File "tools\Start_Windows.ps1" -RomPath "%~1"
)
set "SMW_BUILD_RESULT=%errorlevel%"
echo.
if not "%SMW_BUILD_RESULT%"=="0" echo Setup/build did not complete. See the message above and output logs.
popd
pause
exit /b %SMW_BUILD_RESULT%
