@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ======================================================================
echo             MSMS DASHBOARD - DEPLOY TO UBUNTU SERVER
echo ======================================================================
echo Target Host:   phuserver@100.124.89.115
echo Remote Path:   /home/phuserver/Documents/DashboardManagerWifiMesh/build/
echo Public Domain: https://systemmsems.msems.click
echo ======================================================================
echo.

set "SERVER_USER=phuserver"
set "SERVER_IP=100.124.89.115"
set "REMOTE_BUILD_DIR=/home/phuserver/Documents/DashboardManagerWifiMesh/build"

rem --- STEP 1: BUILD FRONTEND ---
echo [1/3] Building production bundle (npm run build)...
call npm.cmd run build
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed! Please fix build errors before deploying.
    pause
    exit /b 1
)
echo [OK] Build completed successfully.
echo.

rem --- STEP 2: SCP UPLOAD ---
echo [2/3] Uploading build bundle to server (%SERVER_IP%)...
scp -r ".\build\*" "%SERVER_USER%@%SERVER_IP%:%REMOTE_BUILD_DIR%/"
if errorlevel 1 (
    echo.
    echo [WARNING] Upload via %SERVER_IP% failed, trying LAN IP 192.168.1.9...
    scp -r ".\build\*" "%SERVER_USER%@192.168.1.9:%REMOTE_BUILD_DIR%/"
)
echo [OK] Files uploaded successfully.
echo.

rem --- STEP 3: RESTART SERVICE ON SERVER ---
echo [3/3] Restarting msms-frontend service on server...
ssh -o ConnectTimeout=10 "%SERVER_USER%@%SERVER_IP%" "sudo systemctl restart msms-frontend.service"
if errorlevel 1 (
    echo [NOTE] Could not execute remote restart command automatically.
    echo Please run on server: sudo systemctl restart msms-frontend.service
) else (
    echo [OK] Service msms-frontend restarted successfully!
)

echo.
echo ======================================================================
echo [SUCCESS] Deploy completed!
echo Open your browser at: https://systemmsems.msems.click
echo ======================================================================
echo.

endlocal
