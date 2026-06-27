import asyncio
import websockets
import json
import random
import datetime
import sqlite3
import os
import time
from aiohttp import web
import aiohttp_cors

# --- DB Setup ---
DB_FILE = 'history.db'

def init_db():
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('''
        CREATE TABLE IF NOT EXISTS telemetry (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            ip TEXT,
            sensor TEXT,
            field TEXT,
            value REAL,
            time INTEGER
        )
    ''')
    
    # Check if empty
    c.execute('SELECT count(*) FROM telemetry')
    count = c.fetchone()[0]
    if count == 0:
        print("Database is empty. Generating 500 mock records...")
        now = int(time.time() * 1000)
        ips = ["192.168.1.101", "192.168.1.102"]
        
        for ip in ips:
            for i in range(500):
                t = now - (500 - i) * 60000 # back in time, 1 min intervals
                
                # BME280
                c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, 'BME280', 'Temperature', round(random.uniform(24.0, 28.0), 2), t))
                c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, 'BME280', 'Humidity', round(random.uniform(55.0, 65.0), 2), t))
                c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, 'BME280', 'Pressure', round(random.uniform(1000.0, 1015.0), 2), t))
                
                # AHT10
                c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, 'AHT10', 'Temperature', round(random.uniform(24.0, 28.0), 2), t))
                c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, 'AHT10', 'Humidity', round(random.uniform(55.0, 65.0), 2), t))
                
        conn.commit()
        print("Mock data generated.")
    c.execute('''
        CREATE TABLE IF NOT EXISTS firmware (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            object TEXT,
            version TEXT,
            size INTEGER,
            uploadedAt TEXT
        )
    ''')

    # Seed firmware if empty
    c.execute('SELECT count(*) FROM firmware')
    fw_count = c.fetchone()[0]
    if fw_count == 0:
        print("Firmware table is empty. Generating mock firmware...")
        now_iso = datetime.datetime.utcnow().isoformat() + "Z"
        fws = [
            ("Gateway", "v1.2.4", 3145728, now_iso), # 3MB
            ("ESP32", "v2.1.0", 2516582, now_iso), # ~2.4MB
            ("ESP8266", "v1.4.7", 1887436, now_iso), # ~1.8MB
            ("STM32", "v3.0.2", 3250585, now_iso), # ~3.1MB
            ("NRF52", "v1.9.5", 2097152, now_iso), # 2.0MB
        ]
        c.executemany('INSERT INTO firmware (object, version, size, uploadedAt) VALUES (?, ?, ?, ?)', fws)
        conn.commit()
        print("Mock firmware generated.")

    conn.close()

def save_to_db(ip, sensor, field, value):
    try:
        conn = sqlite3.connect(DB_FILE)
        c = conn.cursor()
        t = int(time.time() * 1000)
        c.execute('INSERT INTO telemetry (ip, sensor, field, value, time) VALUES (?, ?, ?, ?, ?)', (ip, sensor, field, value, t))
        conn.commit()
        conn.close()
    except Exception as e:
        print(f"DB Error: {e}")

# --- WebSocket Server ---

async def send_telemetry(websocket):
    print("WebSocket Client connected!")
    try:
        ip_index = 0
        ips = ["192.168.1.101", "192.168.1.102"]
        last_gw_status_time = time.time() - 5
        while True:
            ip = ips[ip_index % 2]
            ip_index += 1
            
            temp_bme = round(random.uniform(24.0, 26.0), 3)
            hum_bme = round(random.uniform(55.0, 65.0), 3)
            press_bme = round(random.uniform(1000.0, 1015.0), 3)
            
            temp_aht = round(temp_bme + random.uniform(-0.5, 0.5), 3)
            hum_aht = round(hum_bme + random.uniform(-2.0, 2.0), 3)

            if ip == "192.168.1.102":
                temp_bme += 2.0
                temp_aht += 2.0

            # Save to DB
            save_to_db(ip, 'BME280', 'Temperature', temp_bme)
            save_to_db(ip, 'BME280', 'Humidity', hum_bme)
            save_to_db(ip, 'BME280', 'Pressure', press_bme)
            save_to_db(ip, 'AHT10', 'Temperature', temp_aht)
            save_to_db(ip, 'AHT10', 'Humidity', hum_aht)

            payload_obj = {
                "v": 1,
                "packetloss": 0.00,
                "n": 2, 
                "i": ip, 
                "t": datetime.datetime.utcnow().isoformat() + "Z",
                "ver": "1.0.0",
                "err": [],
                "p": [
                    [1, 0, temp_bme, hum_bme, press_bme], 
                    [3, 13, temp_aht, hum_aht]            
                ]
            }

            payload_str = json.dumps(payload_obj, separators=(',', ':'))

            packet = {
                "type": "uart_rx",
                "len": len(payload_str),
                "payload": payload_str
            }

            await websocket.send(json.dumps(packet))
            print(f"WS Sent to {ip}: {temp_bme}°C, {hum_bme}%")
            
            # Send gateway_status every 5s
            now_sec = time.time()
            if now_sec - last_gw_status_time >= 5:
                last_gw_status_time = now_sec
                gw_payload = {
                    "type": "gateway_status",
                    "clientType": "esp32",
                    "cpu_load_percent": round(random.uniform(5.0, 25.0), 2),
                    "ram_used_kb": 128,
                    "ram_used_percent": round(random.uniform(40.0, 60.0), 2),
                    "battery_voltage_v": round(random.uniform(3.5, 4.2), 2),
                    "battery_percent": round(random.uniform(20.0, 100.0), 1),
                    "power_source": "battery",
                    "chip_temp_c": round(random.uniform(45.0, 55.0), 2),
                    "chip_temp_internal_supported": True,
                    "uptime_s": int(time.time()),
                    "wifi_ssid": "My_Home_Network",
                    "wifi_rssi": random.randint(-70, -40),
                    "sta_ip": "192.168.1.5",
                    "sta_gateway": "192.168.1.1"
                }
                await websocket.send(json.dumps(gw_payload))
                print("WS Sent gateway_status")

            await asyncio.sleep(1) 
    except websockets.exceptions.ConnectionClosed:
        print("WebSocket Client disconnected.")

# --- HTTP API Handlers ---

async def handle_health(request):
    return web.json_response({"ok": True, "service": "mock"})

async def handle_meta(request):
    return web.json_response({"ips": [{"ip": "192.168.1.101"}, {"ip": "192.168.1.102"}]})

async def handle_sensors(request):
    ip = request.query.get('ip', '')
    if ip in ["192.168.1.101", "192.168.1.102"]:
        return web.json_response({"ip": ip, "sensors": ["BME280", "AHT10"]})
    return web.json_response({"ip": ip, "sensors": []})

async def handle_series(request):
    ip = request.query.get('ip')
    sensor = request.query.get('sensor')
    limit = int(request.query.get('limit', 500))
    
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('''
        SELECT ip, sensor, field, value, time 
        FROM telemetry 
        WHERE ip = ? AND sensor = ? 
        ORDER BY time DESC LIMIT ?
    ''', (ip, sensor, limit))
    rows = c.fetchall()
    conn.close()
    
    items = []
    for r in rows:
        items.append({
            "ip": r[0],
            "sensor": r[1],
            "field": r[2],
            "value": r[3],
            "time": r[4]
        })
    
    return web.json_response({"items": items})

async def handle_firmware(request):
    conn = sqlite3.connect(DB_FILE)
    c = conn.cursor()
    c.execute('SELECT object, version, size, uploadedAt FROM firmware ORDER BY id ASC')
    rows = c.fetchall()
    conn.close()
    
    items = []
    for r in rows:
        items.append({
            "object": r[0],
            "version": r[1],
            "size": r[2],
            "uploadedAt": r[3]
        })
    return web.json_response({"firmware": items})

async def start_http_server():
    app = web.Application()
    
    # Setup CORS
    cors = aiohttp_cors.setup(app, defaults={
        "*": aiohttp_cors.ResourceOptions(
            allow_credentials=True,
            expose_headers="*",
            allow_headers="*",
        )
    })
    
    # Add routes and CORS
    cors.add(app.router.add_get('/api/v1/health', handle_health))
    cors.add(app.router.add_get('/api/v1/history/meta', handle_meta))
    cors.add(app.router.add_get('/api/v1/history/sensors', handle_sensors))
    cors.add(app.router.add_get('/api/v1/history/series', handle_series))
    cors.add(app.router.add_get('/api/v1/firmware', handle_firmware))
    
    runner = web.AppRunner(app)
    await runner.setup()
    site = web.TCPSite(runner, '0.0.0.0', 8080)
    print("Starting HTTP API on http://0.0.0.0:8080")
    await site.start()

# --- Main Entry ---

async def main():
    init_db()
    
    # Start HTTP
    await start_http_server()
    
    # Start WS
    print("Starting WebSocket Server on ws://0.0.0.0:8765")
    async with websockets.serve(send_telemetry, "0.0.0.0", 8765):
        await asyncio.Future()  # run forever

if __name__ == "__main__":
    asyncio.run(main())
