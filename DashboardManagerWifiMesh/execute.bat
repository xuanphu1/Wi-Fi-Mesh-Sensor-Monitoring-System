@echo off
setlocal

cd /d "%~dp0"

if exist ".msms-dev.pid" (
    set /p MSMS_DEV_PID=<".msms-dev.pid"
    tasklist /FI "PID eq %MSMS_DEV_PID%" 2>NUL | findstr /R /C:" %MSMS_DEV_PID% " >NUL
    if not errorlevel 1 (
        echo MSMS Dashboard is already running. PID: %MSMS_DEV_PID%
        echo Run stop.bat before starting another instance.
        exit /b 1
    )
    del /Q ".msms-dev.pid" >NUL 2>&1
)

echo Starting MSMS Dashboard...
echo Database: SQLite
echo Frontend: http://localhost:3000
echo WebSocket: ws://localhost:9090/ws

powershell.exe -NoProfile -ExecutionPolicy Bypass -Command ^
  "$env:DB_TYPE='sqlite'; $p = Start-Process -FilePath 'cmd.exe' -ArgumentList '/k','title MSMS Dashboard Dev & npm.cmd run dev' -WorkingDirectory '%~dp0' -PassThru; Set-Content -LiteralPath '%~dp0.msms-dev.pid' -Value $p.Id -Encoding ascii; Write-Host ('Started process tree PID: ' + $p.Id)"

if errorlevel 1 (
    echo Failed to start MSMS Dashboard.
    exit /b 1
)

echo.
echo Startup command sent successfully.
echo Keep the new "MSMS Dashboard Dev" window open while using the system.
endlocal
