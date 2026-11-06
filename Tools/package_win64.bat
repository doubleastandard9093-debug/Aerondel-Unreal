@echo off
rem Aerondel - package a Win64 Development build (cook + stage + pak) for a weak laptop GPU/CPU.
rem Usage: double-click, or run from any folder:  Tools\package_win64.bat
rem Override paths:  set UE_ROOT=D:\Epic\UE_5.8  before running.
rem Day-to-day C++ compile is root Build.bat (incremental by default). This script always cooks.
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

rem Refuse to package a World that is only the small test or has no gameplay placements (see Content/Python/aerondel_full_world.py).
rem Override (not recommended):  set AERONDEL_ALLOW_PARTIAL=1
set "MARK=C:\Dev\Aerondel-Unreal\Saved\AerondelWorldBuild.json"
if not "%AERONDEL_ALLOW_PARTIAL%"=="1" (
  powershell -NoProfile -Command "$m = Get-Content -Raw '%MARK%' -ErrorAction Stop | ConvertFrom-Json; if ($m.mode -ne 'full' -or $m.nodes -lt 1 -or $m.mobs -lt 1) { Write-Host ('[Aerondel] World build is mode=' + $m.mode + ' nodes=' + $m.nodes + ' mobs=' + $m.mobs); exit 2 }" || (
    echo [Aerondel] The World map is missing, partial ^(small test^) or has no trees/rocks/fishing/monsters.
    echo [Aerondel] In the editor run  Aerondel ^> 0. BUILD FULL GAME WORLD  ^(or 4. Build World Blockout^), save, close the editor, then rerun this.
    exit /b 2
  )
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
