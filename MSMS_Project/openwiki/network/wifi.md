# Wi-Fi Configuration

## Overview
The network system provides:
- Wi-Fi AP/STA modes
- Web configuration interface
- Captive portal for initial setup

## Key Components
1. **Wi-Fi Manager**
   - Handles AP/STA mode switching
   - Stores credentials in NVS

2. **Web Interface**
   - Served from SPIFFS
   - Configuration pages
   - Captive portal for initial setup

3. **Network Tasks**
   - Runs in background
   - Manages connections

## Configuration Flow
1. Device boots in AP mode
2. User connects to captive portal
3. Configures STA credentials
4. Device restarts in STA mode

## Files
- Web interface: `/main/web/` (SPIFFS)
- Wi-Fi code: `/component/network/wifi/`

## Mesh Networking
Planned feature - see `/INTEGRATE_ESP_MDF.md` for initial design