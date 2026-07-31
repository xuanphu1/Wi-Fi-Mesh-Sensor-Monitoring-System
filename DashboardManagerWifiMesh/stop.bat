@echo off
setlocal EnableDelayedExpansion

rem Relaunch with Administrator rights when Windows does not allow taskkill.
fltmc >NUL 2>&1
if errorlevel 1 (
    echo Administrator permission is required to stop the MSMS process tree.
    powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
      "Start-Process -FilePath '%~f0' -WorkingDirectory '%~dp0' -Verb RunAs"
    exit /b
)

cd /d "%~dp0"
set "STOPPED=0"
set "FAILED=0"

if exist ".msms-dev.pid" (
    set /p MSMS_DEV_PID=<".msms-dev.pid"
    echo Stopping MSMS Dashboard process tree PID !MSMS_DEV_PID!...
    taskkill /PID !MSMS_DEV_PID! /T /F >NUL 2>&1
    if not errorlevel 1 set "STOPPED=1"
    if errorlevel 1 set "FAILED=1"
    del /Q ".msms-dev.pid" >NUL 2>&1
)

rem Also handle orphaned servers whose listening socket has closed but whose
rem established WebSocket connections are still alive. Only inspect the local
rem endpoint (token 2), so browser clients connected to the port are not killed.
for %%G in (3000 9090) do (
    for /f "tokens=2,5" %%A in ('netstat -ano -p tcp ^| findstr /R /C:"^  TCP"') do (
        echo(%%A| findstr /E /L /C:":%%G" >NUL
        if not errorlevel 1 (
            echo Stopping remaining process on local port %%G, PID %%B...
            taskkill /PID %%B /T /F >NUL 2>&1
            if not errorlevel 1 set "STOPPED=1"
            if errorlevel 1 set "FAILED=1"
        )
    )
)

if "!FAILED!"=="1" (
    echo Some processes could not be stopped. Please accept the Administrator prompt.
) else if "!STOPPED!"=="1" (
    echo MSMS Dashboard stopped.
) else (
    echo MSMS Dashboard is not running.
)

endlocal
