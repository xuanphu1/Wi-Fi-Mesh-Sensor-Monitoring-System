export const mockRootStatus = {
  rootId: "ROOT-01",
  ap: {
    ssid: "ROOT_AP",
    clients: 3,
    ip: "192.168.4.1",
    channel: 6,
    uptimeSec: 13842,
  },
  system: {
    lastUpdateIso: new Date().toISOString(),
    firmwareVersion: "1.0.0",
    heapFree: 153216,
    rssi: -38,
  },
};

export const mockNodes = [
  {
    id: "NODE-001",
    name: "Living room node",
    location: "Living room",
    type: "ENV",
    online: true,
    rssi: -61,
    lastSeenIso: new Date(Date.now() - 8_000).toISOString(),
    firmwareVersion: "0.9.4",
    batteryPct: 92,
  },
  {
    id: "NODE-002",
    name: "Kitchen node",
    location: "Kitchen",
    type: "ENV",
    online: false,
    rssi: -89,
    lastSeenIso: new Date(Date.now() - 65 * 60_000).toISOString(),
    firmwareVersion: "0.9.2",
    batteryPct: null,
  },
  {
    id: "NODE-003",
    name: "Outdoor node",
    location: "Yard",
    type: "WEATHER",
    online: true,
    rssi: -72,
    lastSeenIso: new Date(Date.now() - 14_000).toISOString(),
    firmwareVersion: "1.0.0",
    batteryPct: 74,
  },
];

export const mockSensors = [
  { sensorId: "temp", name: "Temperature", unit: "°C", nodeId: "NODE-001", value: 27.3, min: 26.8, max: 28.1, avg: 27.2, threshold: { warnAbove: 35 } },
  { sensorId: "hum", name: "Humidity", unit: "%", nodeId: "NODE-001", value: 58.2, min: 51.2, max: 62.4, avg: 56.9, threshold: { warnAbove: 80 } },
  { sensorId: "co2", name: "CO₂", unit: "ppm", nodeId: "NODE-003", value: 812, min: 650, max: 990, avg: 780, threshold: { warnAbove: 1200 } },
];

export const mockAlerts = [
  {
    id: "ALERT-0001",
    tsIso: new Date(Date.now() - 4 * 60_000).toISOString(),
    level: "warning",
    type: "threshold",
    nodeId: "NODE-003",
    sensorId: "co2",
    message: "CO₂ is above the warning threshold",
    value: 812,
    threshold: 800,
    active: true,
  },
  {
    id: "ALERT-0002",
    tsIso: new Date(Date.now() - 68 * 60_000).toISOString(),
    level: "error",
    type: "node_offline",
    nodeId: "NODE-002",
    sensorId: null,
    message: "Node is offline",
    value: null,
    threshold: null,
    active: false,
  },
];

export const mockOta = {
  root: { current: "1.0.0", latestAvailable: "1.0.1", updateAvailable: true },
  nodes: [
    { nodeId: "NODE-001", current: "0.9.4", latestAvailable: "1.0.0", status: "outdated" },
    { nodeId: "NODE-002", current: "0.9.2", latestAvailable: "1.0.0", status: "offline" },
    { nodeId: "NODE-003", current: "1.0.0", latestAvailable: "1.0.0", status: "ok" },
  ],
  firmwares: [
    { id: "FW-0001", device: "NODE", version: "1.0.0", checksum: "sha256:demo", uploadedIso: new Date(Date.now() - 3 * 86400_000).toISOString(), channel: "stable", notes: "Stable" },
    { id: "FW-ROOT-0001", device: "ROOT", version: "1.0.1", checksum: "sha256:demo", uploadedIso: new Date(Date.now() - 1 * 86400_000).toISOString(), channel: "test", notes: "Root OTA test" },
  ],
};

export const mockLogs = [
  { id: "LOG-1", tsIso: new Date(Date.now() - 30_000).toISOString(), source: "ROOT", level: "info", category: "wifi", message: "Client connected: 34:12:98:AA:BB:CC" },
  { id: "LOG-2", tsIso: new Date(Date.now() - 85_000).toISOString(), source: "NODE-001", level: "info", category: "sensor", message: "temp=27.3 hum=58.2" },
  { id: "LOG-3", tsIso: new Date(Date.now() - 210_000).toISOString(), source: "ROOT", level: "error", category: "ota", message: "OTA failed for NODE-002: timeout" },
];

