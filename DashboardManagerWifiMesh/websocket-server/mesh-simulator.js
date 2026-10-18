/**
 * MSMS Mesh Network & Multi-Gateway Simulator
 * Simulates multiple Gateways, Root nodes, and child Mesh nodes with realistic sensor telemetry.
 * Can be run standalone (node mesh-simulator.js) or embedded into server.js via API.
 */

const WebSocket = require("ws");

const SIMULATED_GATEWAYS = [
  {
    gw_id: "GW-DL-01",
    name: "Gateway 1 - Nhà Màng Đà Lạt",
    location: "Đà Lạt - Lô A1",
    mesh_id: "MESH-DALAT-01",
    root_mac: "30:AE:A4:07:0B:01",
    ip: "192.168.1.101",
    rssi: -58,
    nodes: [
      {
        mac: "30:AE:A4:07:0B:01",
        name: "Root Node 1",
        meshLevel: 0,
        sensorType: 0, // BME280 (temp, pressure, hum)
        baseTemp: 22.5,
        baseHum: 78.0,
      },
      {
        mac: "24:6F:28:B2:1A:01",
        name: "Node 1.1 (Cảm biến môi trường)",
        meshLevel: 1,
        sensorType: 3, // DHT22 (temp, hum)
        baseTemp: 23.0,
        baseHum: 76.5,
      },
      {
        mac: "24:6F:28:B2:1A:02",
        name: "Node 1.2 (Lưu lượng nước)",
        meshLevel: 1,
        sensorType: 0, // BME280
        baseTemp: 21.8,
        baseHum: 80.0,
      },
      {
        mac: "24:6F:28:B2:1A:03",
        name: "Node 1.3 (Bụi mịn PM2.5)",
        meshLevel: 2,
        sensorType: 2, // PMS7003 (pm1, pm2.5, pm10)
        basePm25: 18.0,
      },
      {
        mac: "24:6F:28:B2:1A:04",
        name: "Node 1.4 (Khí CO2)",
        meshLevel: 2,
        sensorType: 1, // MH-Z14A (co2, temp)
        baseCo2: 450,
      },
    ],
  },
  {
    gw_id: "GW-HN-02",
    name: "Gateway 2 - Thủy Canh Hà Nội",
    location: "Hà Nội - Khu B2",
    mesh_id: "MESH-HANOI-02",
    root_mac: "30:AE:A4:08:CC:02",
    ip: "192.168.1.102",
    rssi: -64,
    nodes: [
      {
        mac: "30:AE:A4:08:CC:02",
        name: "Root Node 2",
        meshLevel: 0,
        sensorType: 0, // BME280
        baseTemp: 29.5,
        baseHum: 68.0,
      },
      {
        mac: "24:6F:28:C3:2B:01",
        name: "Node 2.1 (Nhiệt ẩm BME)",
        meshLevel: 1,
        sensorType: 0, // BME280
        baseTemp: 30.2,
        baseHum: 65.0,
      },
      {
        mac: "24:6F:28:C3:2B:02",
        name: "Node 2.2 (SHT30)",
        meshLevel: 1,
        sensorType: 4, // AHT10
        baseTemp: 29.8,
        baseHum: 70.0,
      },
      {
        mac: "24:6F:28:C3:2B:03",
        name: "Node 2.3 (Bụi laser)",
        meshLevel: 2,
        sensorType: 2, // PMS7003
        basePm25: 35.0,
      },
      {
        mac: "24:6F:28:C3:2B:04",
        name: "Node 2.4 (Khí CO2)",
        meshLevel: 3,
        sensorType: 1, // MH-Z14A
        baseCo2: 520,
      },
    ],
  },
  {
    gw_id: "GW-GL-03",
    name: "Gateway 3 - Khí Tượng Đồi Gió",
    location: "Gia Lai - Trạm Khí Tượng",
    mesh_id: "MESH-GIALAI-03",
    root_mac: "30:AE:A4:09:DD:03",
    ip: "192.168.1.103",
    rssi: -71,
    nodes: [
      {
        mac: "30:AE:A4:09:DD:03",
        name: "Root Node 3",
        meshLevel: 0,
        sensorType: 0, // BME280
        baseTemp: 25.0,
        baseHum: 72.0,
      },
      {
        mac: "24:6F:28:D4:3C:01",
        name: "Node 3.1 (Áp suất khí quyển)",
        meshLevel: 1,
        sensorType: 0, // BME280
        baseTemp: 24.5,
        baseHum: 74.0,
      },
      {
        mac: "24:6F:28:D4:3C:02",
        name: "Node 3.2 (Bụi ngoại cảnh)",
        meshLevel: 1,
        sensorType: 2, // PMS7003
        basePm25: 12.0,
      },
    ],
  },
];

let simulatorInterval = null;
let uptimeCounter = 1200;
let seqCounter = 1;

function generateNodePayload(gw, node) {
  seqCounter += 1;
  const jitter = (Math.random() - 0.5) * 1.5;
  const now = new Date();
  const pad = (n) => String(n).padStart(2, "0");
  const t = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}T${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;

  let portRow = [];
  if (node.sensorType === 0) {
    // BME280: temp, pressure, hum
    const temp = Number((node.baseTemp + jitter).toFixed(2));
    const press = Number((1013.25 + (Math.random() - 0.5) * 3).toFixed(1));
    const hum = Number((node.baseHum + jitter * 2).toFixed(1));
    portRow = [1, 0, temp, press, hum];
  } else if (node.sensorType === 1) {
    // MH-Z14A: co2, temp
    const co2 = Math.round(node.baseCo2 + (Math.random() - 0.5) * 40);
    const temp = Number((26.5 + jitter).toFixed(1));
    portRow = [1, 1, co2, temp];
  } else if (node.sensorType === 2) {
    // PMS7003: pm1, pm2.5, pm10
    const pm25 = Math.max(5, Math.round(node.basePm25 + (Math.random() - 0.5) * 8));
    const pm1 = Math.round(pm25 * 0.65);
    const pm10 = Math.round(pm25 * 1.4);
    portRow = [1, 2, pm1, pm25, pm10];
  } else {
    // DHT22 / AHT10: temp, hum
    const temp = Number((node.baseTemp + jitter).toFixed(2));
    const hum = Number((node.baseHum + jitter * 2).toFixed(1));
    portRow = [1, node.sensorType, temp, hum];
  }

  const packetloss = Number((Math.random() * 1.5).toFixed(1));
  const batteryPercent = node.baseBattery != null
    ? Math.max(10, Math.min(100, Math.round(node.baseBattery + (Math.random() - 0.5) * 4)))
    : Math.round(75 + Math.random() * 23);
  const chargingStatus = node.meshLevel === 0 ? 1 : (batteryPercent < 40 ? 1 : 0);

  return {
    v: 1,
    seq: seqCounter,
    n: node.meshLevel,
    M: node.mac,
    i: node.mac,
    t,
    ver: "1.0.1",
    b: batteryPercent,
    c: chargingStatus,
    err: ["0x0000"],
    packetloss,
    gw_id: gw.gw_id,
    gw_name: gw.name,
    mesh_id: gw.mesh_id,
    location: gw.location,
    root_mac: gw.root_mac,
    gateway: {
      gw_id: gw.gw_id,
      name: gw.name,
      location: gw.location,
      mesh_id: gw.mesh_id,
      root_mac: gw.root_mac,
      ip: gw.ip,
    },
    p: [portRow],
  };
}

function generateGatewayStatus(gw) {
  uptimeCounter += 3;
  const cpu = Math.round(15 + Math.random() * 25);
  const ram = Math.round(45 + Math.random() * 10);
  const batteryV = Number((3.95 + (Math.random() - 0.5) * 0.1).toFixed(2));

  return {
    type: "gateway_status",
    clientType: "esp32",
    gw_id: gw.gw_id,
    mesh_id: gw.mesh_id,
    name: gw.name,
    location: gw.location,
    mac: gw.root_mac,
    sta_mac: gw.root_mac,
    sta_ip: gw.ip,
    cpu_load_percent: cpu,
    ram_used_percent: ram,
    ram_used_kb: Math.round(ram * 3.2),
    battery_voltage_v: batteryV,
    battery_percent: Math.round(((batteryV - 3.4) / 0.8) * 100),
    power_source: "battery",
    chip_temp_internal_supported: true,
    chip_temp_c: Number((42.5 + (Math.random() - 0.5) * 2).toFixed(1)),
    uptime_s: uptimeCounter,
    wifi_ssid: "MSMS_MESH_NET",
    wifi_rssi: gw.rssi + Math.round((Math.random() - 0.5) * 4),
  };
}

/**
 * Start simulated data stream directly into store and broadcast to clients
 */
function startEmbeddedSimulator(store, broadcast) {
  if (simulatorInterval) return false;

  console.log("[simulator] Starting embedded Multi-Gateway Mesh Simulator...");

  // Send initial Gateway status & registration
  SIMULATED_GATEWAYS.forEach(async (gw) => {
    try {
      if (store && typeof store.upsertGateway === "function") {
        await store.upsertGateway({
          gw_id: gw.gw_id,
          name: gw.name,
          location: gw.location,
          root_mac: gw.root_mac,
          mesh_id: gw.mesh_id,
          ip: gw.ip,
          status: "online",
          last_seen: new Date().toISOString(),
          notes: "Trạm giả lập tự động",
        });
      }
    } catch (e) {}
  });

  simulatorInterval = setInterval(async () => {
    const receivedAt = new Date();

    for (const gw of SIMULATED_GATEWAYS) {
      // 1. Send gateway status packet
      const gwStatus = generateGatewayStatus(gw);
      if (store && typeof store.persistGatewayStatus === "function") {
        try {
          await store.persistGatewayStatus(gwStatus, gw.ip, receivedAt);
        } catch (e) {}
      }
      if (broadcast) broadcast(gwStatus);

      // 2. Pick 1-2 random nodes from this gateway to emit telemetry
      const count = Math.min(2, gw.nodes.length);
      const shuffled = [...gw.nodes].sort(() => 0.5 - Math.random());
      const selectedNodes = shuffled.slice(0, count);

      for (const node of selectedNodes) {
        const payload = generateNodePayload(gw, node);
        if (store && typeof store.persistMeshPacket === "function") {
          try {
            await store.persistMeshPacket(payload, gw.ip, receivedAt);
          } catch (e) {}
        }
        if (broadcast) broadcast(payload);
      }
    }
  }, 3500);

  return true;
}

function stopEmbeddedSimulator() {
  if (simulatorInterval) {
    clearInterval(simulatorInterval);
    simulatorInterval = null;
    console.log("[simulator] Embedded Multi-Gateway Mesh Simulator stopped.");
    return true;
  }
  return false;
}

function getSimulatorStatus() {
  return {
    active: Boolean(simulatorInterval),
    gatewaysCount: SIMULATED_GATEWAYS.length,
    totalNodesCount: SIMULATED_GATEWAYS.reduce((acc, g) => acc + g.nodes.length, 0),
    gateways: SIMULATED_GATEWAYS.map((g) => ({
      gw_id: g.gw_id,
      name: g.name,
      location: g.location,
      mesh_id: g.mesh_id,
      nodeCount: g.nodes.length,
    })),
  };
}

// Standalone execution via CLI: node mesh-simulator.js [wss://systemmsems.msems.click/ws]
if (require.main === module) {
  const wsTarget = process.argv[2] || "wss://systemmsems.msems.click/ws";
  let sendTimer = null;

  function connectAndStream() {
    console.log(`[simulator-standalone] Connecting to ${wsTarget}...`);
    const ws = new WebSocket(wsTarget, {
      rejectUnauthorized: false,
    });

    ws.on("open", () => {
      console.log(`[simulator-standalone] Connected to ${wsTarget}! Streaming simulated multi-mesh packets...`);
      uptimeCounter = 100;

      if (sendTimer) clearInterval(sendTimer);
      sendTimer = setInterval(() => {
        if (ws.readyState !== WebSocket.OPEN) return;
        SIMULATED_GATEWAYS.forEach((gw) => {
          // Send gateway status
          const gwStatus = generateGatewayStatus(gw);
          ws.send(JSON.stringify(gwStatus));

          // Send node packets
          gw.nodes.forEach((node) => {
            const payload = generateNodePayload(gw, node);
            ws.send(JSON.stringify(payload));
          });
        });
        console.log(`[simulator-standalone] Sent burst of telemetry for ${SIMULATED_GATEWAYS.length} gateways at ${new Date().toLocaleTimeString()}`);
      }, 3000);
    });

    ws.on("error", (err) => {
      console.error(`[simulator-standalone] Connection error:`, err.message || err);
    });

    ws.on("close", () => {
      if (sendTimer) {
        clearInterval(sendTimer);
        sendTimer = null;
      }
      console.log(`[simulator-standalone] Disconnected from server. Retrying in 5s...`);
      setTimeout(connectAndStream, 5000);
    });
  }

  connectAndStream();
}

module.exports = {
  SIMULATED_GATEWAYS,
  startEmbeddedSimulator,
  stopEmbeddedSimulator,
  getSimulatorStatus,
};
