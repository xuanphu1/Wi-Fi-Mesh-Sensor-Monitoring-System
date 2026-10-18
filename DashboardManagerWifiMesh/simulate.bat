@echo off
setlocal

cd /d "%~dp0"

set "WS_TARGET=%~1"
if "%WS_TARGET%"=="" set "WS_TARGET=wss://systemmsems.msems.click/ws"

echo ======================================================================
echo           MSMS MULTI-GATEWAY ^& MESH NETWORK SIMULATOR
echo ======================================================================
echo Generating virtual Gateways, Root nodes, and Mesh sensor telemetry...
echo Target: %WS_TARGET%
echo.
echo Press Ctrl+C at any time to stop the simulation.
echo ======================================================================
echo.

node ".\websocket-server\mesh-simulator.js" "%WS_TARGET%"

pause
