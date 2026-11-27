@echo off
rem Aerondel - compile the C++ game module for Win64.
rem Default: FAST incremental Development Editor build (day-to-day).
rem Usage:
rem   Build.bat                 incremental Editor (default)
rem   Build.bat rebuild         clean rebuild Editor
rem   Build.bat full            same as rebuild
rem   Build.bat game            incremental game target (Aerondel)
rem   Build.bat rebuild game    clean rebuild game target
rem Override engine path:  set UE_ROOT=D:\Epic\UE_5.8
rem Packaging is separate: Tools\package_win64.bat (still refuses a partial World).
setlocal
if "%UE_ROOT%"=="" set "UE_ROOT=C:\Program Files\Epic Games\UE_5.8"
set "UBT=%UE_ROOT%\Engine\Build\BatchFiles\Build.bat"
set "PROJECT=%~dp0Aerondel.uproject"
set "TARGET=AerondelEditor"
set "CONFIG=Development"
set "EXTRA=-WaitMutex"

if not exist "%UBT%" (
  echo [Aerondel] Build.bat not found at "%UBT%". Set UE_ROOT to your Unreal 5.8 install folder.
  exit /b 1
)
if not exist "%PROJECT%" (
  echo [Aerondel] Project not found at "%PROJECT%".
  exit /b 1
)

:parse
if "%~1"=="" goto run
if /I "%~1"=="rebuild" set "EXTRA=-WaitMutex -Rebuild" & shift & goto parse
if /I "%~1"=="full" set "EXTRA=-WaitMutex -Rebuild" & shift & goto parse
if /I "%~1"=="game" set "TARGET=Aerondel" & shift & goto parse
if /I "%~1"=="editor" set "TARGET=AerondelEditor" & shift & goto parse
echo [Aerondel] Unknown arg "%~1". Use: rebuild ^| full ^| game ^| editor
exit /b 1

:run
echo [Aerondel] %TARGET% Win64 %CONFIG% %EXTRA%
rem MaxParallelActions=1: Aaron's ~8 GB laptop OOMs with many parallel cl.exe / ShaderCompileWorkers.
call "%UBT%" %TARGET% Win64 %CONFIG% -Project="%PROJECT%" -MaxParallelActions=1 %EXTRA%
set "RC=%ERRORLEVEL%"
if not "%RC%"=="0" (
  echo [Aerondel] Compile FAILED with code %RC%.
  exit /b %RC%
)
echo [Aerondel] Compile OK.
endlocal
