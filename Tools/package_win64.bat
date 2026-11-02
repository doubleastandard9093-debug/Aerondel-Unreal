@echo off
rem Aerondel - package a Win64 Development build (cook + stage + pak) for a weak laptop GPU/CPU.
rem Usage: double-click, or run from any folder:  Tools\package_win64.bat
rem Override paths:  set UE_ROOT=D:\Epic\UE_5.8  before running.
setlocal
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "PROJECT=C:\Dev\Aerondel-Unreal\Aerondel.uproject"
set "OUT=C:\Dev\AerondelBuild"
set "UAT=%UE_ROOT%\Engine\Build\BatchFiles\RunUAT.bat"

if not exist "%UAT%" (
  echo [Aerondel] RunUAT not found at "%UAT%". Set UE_ROOT to your Unreal 5.8 install folder.
  exit /b 1
)
if not exist "%PROJECT%" (
  echo [Aerondel] Project not found at "%PROJECT%".
  exit /b 1
)

echo [Aerondel] Packaging Win64 Development to %OUT% ... this takes a while; close the editor first.
call "%UAT%" BuildCookRun ^
  -project="%PROJECT%" ^
  -noP4 -utf8output ^
  -platform=Win64 -clientconfig=Development ^
  -build -cook -stage -pak -iostore -package -archive ^
  -archivedirectory="%OUT%" ^
  -nocompileeditor -unattended ^
  -MaxParallelActions=1
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  echo [Aerondel] Packaging FAILED with code %RC%. Log: %APPDATA%\Unreal Engine\AutomationTool\Logs
  exit /b %RC%
)
echo [Aerondel] Done. Run %OUT%\Windows\Aerondel.exe
endlocal
