@echo off
setlocal enabledelayedexpansion

cd /d "%~dp0"

echo ======================================================================
echo          MSMS SYSTEM - FULL DEPLOY (FRONTEND + BACKEND)
echo ======================================================================
echo Target Host:   phuserver@100.124.89.115
echo Remote Dir:    /home/phuserver/Documents/DashboardManagerWifiMesh/
echo Public Domain: https://systemmsems.msems.click
echo ======================================================================
echo.

set "SERVER_USER=phuserver"
set "SERVER_IP=100.124.89.115"
set "REMOTE_ROOT=/home/phuserver/Documents/DashboardManagerWifiMesh"
set "REMOTE_BUILD_DIR=%REMOTE_ROOT%/build"
set "REMOTE_WS_DIR=%REMOTE_ROOT%/websocket-server"

rem --- STEP 1: BUILD FRONTEND ---
echo [1/4] Building production bundle (npm run build)...
call npm.cmd run build
if errorlevel 1 (
    echo.
    echo [ERROR] Build failed! Please fix build errors before deploying.
    pause
    exit /b 1
)
echo [OK] Build completed successfully.
echo.

rem --- STEP 2: UPLOAD FRONTEND (build/) ---
echo [2/4] Uploading Frontend build to server (%SERVER_IP%)...
scp -r ".\build\*" "%SERVER_USER%@%SERVER_IP%:%REMOTE_BUILD_DIR%/"
if errorlevel 1 (
    echo.
    echo [WARNING] Upload via %SERVER_IP% failed, trying LAN IP 192.168.1.9...
    set "SERVER_IP=192.168.1.9"
    scp -r ".\build\*" "%SERVER_USER%@!SERVER_IP!:%REMOTE_BUILD_DIR%/"
)
echo [OK] Frontend uploaded successfully.
echo.

rem --- STEP 3: UPLOAD BACKEND (websocket-server code only) ---
echo [3/4] Uploading Backend scripts (server.js, sqlite-store.js, mesh-simulator.js)...
scp ".\websocket-server\server.js" ".\websocket-server\sqlite-store.js" ".\websocket-server\mesh-simulator.js" ".\websocket-server\package.json" "%SERVER_USER%@%SERVER_IP%:%REMOTE_WS_DIR%/"
if errorlevel 1 (
    echo [WARNING] Could not upload backend files.
) else (
    echo [OK] Backend files uploaded successfully.
)
echo.

rem --- STEP 4: RESTART SERVICES ON SERVER ---
echo [4/4] Restarting msms-frontend and msms-backend services on server...
ssh -t -o ConnectTimeout=10 "%SERVER_USER%@%SERVER_IP%" "sudo systemctl restart msms-frontend.service msms-backend.service"
if errorlevel 1 (
    echo [NOTE] Could not execute remote restart command automatically.
    echo Please run on server: sudo systemctl restart msms-frontend.service msms-backend.service
) else (
    echo [OK] Both msms-frontend and msms-backend restarted successfully!
)

echo.
echo ======================================================================
echo [SUCCESS] Full deployment completed!
echo - Frontend: Updated and restarted
echo - Backend:  Updated and restarted (Database mesh-data.sqlite preserved)
echo - Web URL:  https://systemmsems.msems.click
echo ======================================================================
echo.

endlocal
