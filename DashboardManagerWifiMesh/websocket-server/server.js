const os = require("os");
const { WebSocketServer } = require("ws");
const si = require("systeminformation");
const express = require("express");
const cors = require("cors");
const http = require("http");
const swaggerUi = require("swagger-ui-express");
const swaggerJsdoc = require("swagger-jsdoc");


const PORT = Number(process.env.WS_PORT || process.env.PORT || 8080);
const HOST = process.env.WS_HOST || "0.0.0.0";
const MONGO_URI = process.env.MONGO_URI || "mongodb://127.0.0.1:27017/MeshData";

function clientIpPretty(ip) {
  if (typeof ip !== "string") return ip;
  const m = ip.match(/^::ffff:(.+)$/);
  return m ? m[1] : ip;
}

/** IPv4 không loopback — kèm tên interface để phân biệt Wi‑Fi / adapter ảo */
function listIPv4Addresses() {
  const nets = os.networkInterfaces();
  const out = [];
  for (const name of Object.keys(nets)) {
    const addrs = nets[name];
    if (!addrs) continue;
    for (const net of addrs) {
      const fam = net.family;
      const isIPv4 = fam === "IPv4" || fam === 4;
      if (!isIPv4 || net.internal) continue;
      out.push({ name, address: net.address });
    }
  }
  return out;
}

function isLikelyVirtualAdapter(name) {
  return /virtualbox|vmware|vethernet|hyper-?v|wsl|docker|npcap|zerotier|tailscale|nordvpn|hamachi|sunlogin|openvpn|wireguard|tap-?windows/i.test(
    name
  );
}

function logMachineLanHint(port) {
  const all = listIPv4Addresses();
  const likelyLan = all.filter((x) => !isLikelyVirtualAdapter(x.name));
  
  const lanIp = likelyLan.length > 0 ? likelyLan[0].address : "192.168.x.x";

  console.log("\n  You can now view \x1b[1mdashboard-ws-server API Docs\x1b[0m in the browser.\n");
  console.log(`    \x1b[1mLocal:\x1b[0m            http://localhost:${port}/api-docs`);
  console.log(`    \x1b[1mOn Your Network:\x1b[0m  http://${lanIp}:${port}/api-docs\n`);

  console.log("  WebSocket Server is also running:");
  console.log(`    \x1b[1mLocal:\x1b[0m            ws://localhost:${port}`);
  console.log(`    \x1b[1mOn Your Network:\x1b[0m  ws://${lanIp}:${port}\n`);

  if (all.length > 0) {
    console.log("  Available IPv4 addresses (all):");
    all.forEach(({ name, address }) => {
      const tag = isLikelyVirtualAdapter(name) ? " (virtual)" : "";
      console.log(`    - ${address} [${name}]${tag}`);
    });
    console.log("\n");
  }
}

function getServerNetworkInfo() {
  const nets = os.networkInterfaces();
  const rows = [];
  for (const name of Object.keys(nets)) {
    const addrs = nets[name];
    if (!addrs) continue;
    for (const net of addrs) {
      const fam = net.family;
      const isIPv4 = fam === "IPv4" || fam === 4;
      if (!isIPv4 || net.internal) continue;
      rows.push({
        adapter: name,
        ip: net.address,
        mac: net.mac || "",
      });
    }
  }
  const preferred = rows.find((x) => !isLikelyVirtualAdapter(x.adapter)) || rows[0] || null;
  return preferred;
}

function decodeHexUtf8(hex) {
  if (typeof hex !== "string") return "";
  const clean = hex.trim().replace(/\s+/g, "");
  if (!clean || clean.length % 2 !== 0 || /[^0-9a-f]/i.test(clean)) return "";
  try {
    return Buffer.from(clean, "hex").toString("utf8");
  } catch {
    return "";
  }
}

const SENSOR_TYPE_NAMES = {
  0: "SENSOR_BME280",
  1: "SENSOR_MHZ14A",
  2: "SENSOR_PMS7003",
  3: "SENSOR_DHT22",
  4: "SENSOR_AHT10",
  5: "SENSOR_DHT11",
  6: "SENSOR_HTU21D",
};

function sensorTypeName(typeId) {
  if (typeof typeId !== "number" || !Number.isFinite(typeId)) return "SENSOR_UNKNOWN";
  return SENSOR_TYPE_NAMES[typeId] ?? `SENSOR_UNKNOWN_${typeId}`;
}

const SENSOR_VALUE_FIELD_NAMES = {
  0: ["temp_C", "pressure_hPa", "humidity_RH"],
  1: ["co2_ppm", "temp_C"],
  2: ["pm1_0_ugm3", "pm2_5_ugm3", "pm10_ugm3"],
  3: ["temp_C", "humidity_RH"],
  4: ["temp_C", "humidity_RH"],
  5: ["temp_C", "humidity_RH"],
  6: ["temp_C", "humidity_RH"],
};

function formatSensorScalar(x) {
  if (typeof x === "number" && Number.isFinite(x)) return x.toFixed(3);
  const n = Number(x);
  if (Number.isFinite(n)) return n.toFixed(3);
  return String(x);
}

function formatMeshValuesLabeled(sensorType, values) {
  const labels = SENSOR_VALUE_FIELD_NAMES[sensorType];
  const parts = [];
  values.forEach((val, i) => {
    const key =
      labels && labels[i] != null ? labels[i] : `v${i}`;
    parts.push(`${key}=${formatSensorScalar(val)}`);
  });
  return parts.join(", ");
}

function looksLikeMeshUdpJson(o) {
  return store.isMeshPayload(o);
}

const seqStats = new Map();

function trackSeq(pkt) {
  if (!pkt || typeof pkt !== "object") return;
  const ip = pkt.M || pkt.i;
  const seq = pkt.seq;
  if (typeof ip === "string" && typeof seq === "number") {
    if (!seqStats.has(ip)) {
      seqStats.set(ip, { count: 0, minSeq: seq, maxSeq: seq });
    }
    const stat = seqStats.get(ip);
    stat.count++;
    if (seq < stat.minSeq) stat.minSeq = seq;
    if (seq > stat.maxSeq) stat.maxSeq = seq;
  }
}

function logMeshUdpJson(remoteIp, sourceTag, frameIdx, pkt) {
  const v = pkt.v;
  const n = pkt.n;
  const sta = pkt.M || pkt.i;
  const t = typeof pkt.t === "string" ? pkt.t : "";
  console.log(
    `[ws] ${sourceTag} from ${remoteIp} frame[${frameIdx}] header: schema(v)=${v}, mesh_level(n)=${n}, mac/ip=${sta}, rtc(t)=${t || "(missing)"}`
  );

  if (!Array.isArray(pkt.p) || pkt.p.length === 0) {
    console.log(`[ws] ${sourceTag} frame[${frameIdx}] p: (empty)`);
    return;
  }

  pkt.p.forEach((row, rowIdx) => {
    if (!Array.isArray(row) || row.length < 2) {
      console.log(`[ws] ${sourceTag} frame[${frameIdx}] p[${rowIdx}] invalid row: ${JSON.stringify(row)}`);
      return;
    }
    const portWire = row[0];
    const sensorType = row[1];
    const values = row.slice(2);
    const typeLabel = sensorTypeName(sensorType);
    const valsFmt = values.map(formatSensorScalar).join(", ");
    const labeled = formatMeshValuesLabeled(sensorType, values);
    console.log(
      `[ws] ${sourceTag} frame[${frameIdx}] p[${rowIdx}] port=${portWire} (wire Port ${portWire}), type=${sensorType} (${typeLabel})`
    );
    console.log(
      `[ws] ${sourceTag} frame[${frameIdx}] p[${rowIdx}] fields: ${labeled}`
    );
    console.log(
      `[ws] ${sourceTag} frame[${frameIdx}] p[${rowIdx}] raw v=[${valsFmt}]`
    );
  });
}

function parseUartFrames(parsed, state) {
  if (!parsed || parsed.type !== "uart_rx") return [];

  if (parsed.payload != null && typeof parsed.payload === "object" && looksLikeMeshUdpJson(parsed.payload)) {
    return [{ idx: 0, obj: parsed.payload }];
  }

  if (typeof parsed.payload === "string") {
    const raw = parsed.payload.trim();
    if (raw) {
      try {
        const obj = JSON.parse(raw);
        if (looksLikeMeshUdpJson(obj)) return [{ idx: 0, obj }];
      } catch {
        // Continue with line-based parsing below.
      }

      const packets = [];
      raw
        .split(/\r?\n/)
        .map((x) => x.trim())
        .filter(Boolean)
        .forEach((frame, idx) => {
          try {
            const obj = JSON.parse(frame);
            if (looksLikeMeshUdpJson(obj)) packets.push({ idx, obj });
          } catch {
            console.log(`[ws] uart_rx payload frame[${idx}] JSON parse failed`);
          }
        });
      if (packets.length) return packets;
    }
  }

  if (typeof parsed.hex !== "string") return [];
  const decoded = decodeHexUtf8(parsed.hex);
  if (!decoded) return [];

  const carry = typeof state.uartCarry === "string" ? state.uartCarry : "";
  const merged = `${carry}${decoded}`;
  const parts = merged.split(/\r?\n/);
  const hasTerminator = /\r?\n$/.test(merged);
  const complete = hasTerminator ? parts : parts.slice(0, -1);
  state.uartCarry = hasTerminator ? "" : (parts[parts.length - 1] || "").slice(-4096);

  const packets = [];
  complete
    .map((x) => x.trim())
    .filter(Boolean)
    .forEach((frame, idx) => {
      try {
        const obj = JSON.parse(frame);
        if (looksLikeMeshUdpJson(obj)) packets.push({ idx, obj });
      } catch {
        console.log(`[ws] uart_rx frame[${idx}] JSON parse failed`);
      }
    });
  return packets;
}

const DB_TYPE = process.env.DB_TYPE || "mongodb";
let store;

if (DB_TYPE === "sqlite") {
  const { createSqliteStore } = require("./sqlite-store");
  store = createSqliteStore({
    dbPath: process.env.SQLITE_DB_PATH || "mesh-data.sqlite",
    sensorValueFieldNames: SENSOR_VALUE_FIELD_NAMES,
    sensorTypeName,
  });
} else {
  const { createMongoStore } = require("./mongodb-store");
  store = createMongoStore({
    mongoUri: MONGO_URI,
    sensorValueFieldNames: SENSOR_VALUE_FIELD_NAMES,
    sensorTypeName,
  });
}

const app = express();
app.use(cors());
app.use(express.json());

// --- Swagger Configuration ---
const swaggerOptions = {
  definition: {
    openapi: "3.0.0",
    info: {
      title: "Wi-Fi Mesh Dashboard API",
      version: "1.0.0",
      description: "API Documentation for the Wi-Fi Mesh Dashboard Server",
    },
    servers: [
      {
        url: `http://${HOST === "0.0.0.0" ? "localhost" : HOST}:${PORT}`,
        description: "Local Server",
      },
    ],
  },
  apis: ["./server.js"], // Files containing annotations
};

const swaggerSpec = swaggerJsdoc(swaggerOptions);
app.use("/api-docs", swaggerUi.serve, swaggerUi.setup(swaggerSpec));

// --- REST APIs for History ---

/**
 * @swagger
 * /api/nodes:
 *   get:
 *     summary: Retrieve a snapshot of all active nodes
 *     description: Fetches a summary of all active gateways and nodes, including their MAC addresses, last seen times, mesh levels, and packet loss statistics.
 *     responses:
 *       200:
 *         description: An array of node snapshot objects
 *         content:
 *           application/json:
 *             schema:
 *               type: array
 *               items:
 *                 type: object
 *       500:
 *         description: Internal server error
 */
app.get("/api/nodes", async (req, res) => {
  try {
    const nodeSnapshot = await store.getNodeSnapshot();
    res.json(nodeSnapshot);
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/history/meta:
 *   get:
 *     summary: Retrieve a list of all MAC addresses
 *     description: Fetches a list of all unique MAC addresses (gateways and nodes) available in the history database.
 *     parameters:
 *       - in: query
 *         name: limit
 *         schema:
 *           type: integer
 *         description: Maximum number of MACs to return (default 200).
 *     responses:
 *       200:
 *         description: A list of MAC addresses
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 macs:
 *                   type: array
 *                   items:
 *                     type: object
 *       500:
 *         description: Internal server error
 */
app.get("/api/history/meta", async (req, res) => {
  try {
    const limit = Number(req.query.limit) || 200;
    const macs = await store.getHistoryMacs(limit);
    res.json({ macs });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/history/series:
 *   get:
 *     summary: Retrieve history series data for charts
 *     description: Fetches up to 2000 data points for a specific sensor and field to be plotted on the dashboard.
 *     parameters:
 *       - in: query
 *         name: mac
 *         required: true
 *         schema:
 *           type: string
 *         description: The MAC address of the node.
 *       - in: query
 *         name: sensorName
 *         required: true
 *         schema:
 *           type: string
 *         description: The name of the sensor (e.g., wifi, system, memory).
 *       - in: query
 *         name: field
 *         required: true
 *         schema:
 *           type: string
 *         description: The specific field to retrieve (e.g., rssi, temp).
 *       - in: query
 *         name: from
 *         schema:
 *           type: string
 *           format: date-time
 *         description: ISO 8601 timestamp to fetch data from.
 *       - in: query
 *         name: to
 *         schema:
 *           type: string
 *           format: date-time
 *         description: ISO 8601 timestamp to fetch data to.
 *       - in: query
 *         name: limit
 *         schema:
 *           type: integer
 *         description: Maximum number of data points (default 2000).
 *     responses:
 *       200:
 *         description: An array of time-series data points
 *         content:
 *           application/json:
 *             schema:
 *               type: object
 *               properties:
 *                 items:
 *                   type: array
 *                   items:
 *                     type: object
 *       500:
 *         description: Internal server error
 */
app.get("/api/history/series", async (req, res) => {
  try {
    const items = await store.getHistorySeries({
      mac: req.query.mac,
      sensorName: req.query.sensorName,
      field: req.query.field,
      from: req.query.from,
      to: req.query.to,
      limit: Number(req.query.limit) || 2000,
    });
    res.json({ items });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/history/export:
 *   get:
 *     summary: Download all history data for a MAC address as CSV
 *     description: Streams a complete CSV file containing all sensor records for a given MAC address. The output is directly streamed using chunked transfer encoding for optimal performance.
 *     parameters:
 *       - in: query
 *         name: mac
 *         required: true
 *         schema:
 *           type: string
 *         description: The MAC address of the node to export data for.
 *       - in: query
 *         name: from
 *         schema:
 *           type: string
 *           format: date-time
 *         description: ISO 8601 timestamp to start exporting from.
 *       - in: query
 *         name: to
 *         schema:
 *           type: string
 *           format: date-time
 *         description: ISO 8601 timestamp to stop exporting at.
 *     responses:
 *       200:
 *         description: A streamed CSV file
 *         content:
 *           text/csv:
 *             schema:
 *               type: string
 *               format: binary
 *       400:
 *         description: Missing required parameters (mac)
 *       500:
 *         description: Internal server error
 */
app.get("/api/history/export", async (req, res) => {
  try {
    const { mac, from, to } = req.query;
    if (!mac) {
      return res.status(400).send("MAC address is required");
    }

    const safeMac = mac.replace(/:/g, "-");
    res.setHeader("Content-Type", "text/csv;charset=utf-8");
    res.setHeader("Content-Disposition", `attachment; filename="export_all_${safeMac}.csv"`);
    res.setHeader("Transfer-Encoding", "chunked");

    // Call store method and write to response stream
    const csvHeader = await store.getHistoryAllForMacCsv(mac, from, to, (progress, total, chunk) => {
      // With the new streaming setup, sqlite-store should ideally stream chunks directly.
      // We will adjust getHistoryAllForMacCsv to stream output directly into `res`.
    }, res);
    
  } catch (err) {
    if (!res.headersSent) {
      res.status(500).send(err.message);
    }
  }
});

const server = http.createServer(app);
const wss = new WebSocketServer({ server });

server.listen(PORT, HOST, () => {
  logMachineLanHint(PORT);
});

function broadcast(data, except) {
  const payload = typeof data === "string" ? data : JSON.stringify(data);
  wss.clients.forEach((client) => {
    if (client === except) return;
    if (client.readyState === 1) client.send(payload);
  });
}

wss.on("connection", async (ws, req) => {
  ws.isAlive = true;
  ws.on('pong', () => {
    ws.isAlive = true;
  });

  const ip = clientIpPretty(req.socket.remoteAddress || "");
  console.log(`[ws] client connected ${ip} (tổng ${wss.clients.size})`);
  const serverNet = getServerNetworkInfo();
  const state = { uartCarry: "" };

  // MUST attach listeners SYNCHRONOUSLY to avoid missing early messages
  ws.on("message", async (raw, isBinary) => {
    if (isBinary) {
      console.log(`[ws] RX binary from ${ip}: ${raw.length} bytes`);
      broadcast(raw, ws);
      return;
    }
    const text = raw.toString();
    console.log(`[ws] RX text from ${ip}: ${text}`);
    try {
      const parsed = JSON.parse(text);
      const msgType = parsed && typeof parsed === "object" ? parsed.type || "unknown" : "non-object";
      console.log(`[ws] Parsed JSON from ${ip}: type=${msgType}`);

      // History API endpoints have been moved to HTTP REST API routes

      const receivedAt = new Date();

      const uartPackets = parseUartFrames(parsed, state);
      uartPackets.forEach(({ idx, obj }) => {
        trackSeq(obj);
        logMeshUdpJson(ip, "uart_rx", idx, obj);
        store.persistMeshPacket(obj, ip, receivedAt);
      });

      if (looksLikeMeshUdpJson(parsed)) {
        trackSeq(parsed);
        logMeshUdpJson(ip, "mesh", 0, parsed);
        store.persistMeshPacket(parsed, ip, receivedAt);
      }
      
      if (parsed && parsed.type === "gateway_status") {
        store.persistGatewayStatus(parsed, ip, receivedAt);
      }

      broadcast(parsed, ws);
    } catch {
      console.log(`[ws] Non-JSON text from ${ip}, forwarding as { type: "text" }`);
      broadcast({ type: "text", body: text }, ws);
    }
  });

  ws.on("close", () => {
    console.log(`[ws] client disconnected (còn ${wss.clients.size})`);
  });

  // Only send welcome if socket is still open
  if (ws.readyState === 1) {
    ws.send(
      JSON.stringify({
        type: "welcome",
        t: Date.now(),
        message: "Dashboard WS server",
        serverNet,
      })
    );
  }
});

// --- WebSocket Heartbeat Interval ---
const interval = setInterval(() => {
  wss.clients.forEach((ws) => {
    if (ws.isAlive === false) {
      console.log("[ws] Terminating zombie connection");
      return ws.terminate();
    }
    ws.isAlive = false;
    ws.ping();
  });
}, 30000);

wss.on('close', () => {
  clearInterval(interval);
});

setInterval(() => {
  if (seqStats.size === 0) return;
  const updates = {};
  const now = new Date();
  
  for (const [ip, stat] of seqStats.entries()) {
    if (stat.count > 0) {
      let expected = stat.maxSeq - stat.minSeq + 1;
      if (expected < 0 || expected > 10000) expected = stat.count;
      let lost = expected - stat.count;
      if (lost < 0) lost = 0;
      const loss = (lost / expected) * 100;
      updates[ip] = Number(loss.toFixed(2));
      
      const fakePkt = {
        v: 1, n: 2, i: ip,
        packetloss: updates[ip],
        p: []
      };
      store.persistMeshPacket(fakePkt, "127.0.0.1", now);
    }
  }
  
  if (Object.keys(updates).length > 0) {
    broadcast({ type: "packetloss_update", data: updates }, null);
  }
  seqStats.clear();
}, 10000);

setInterval(async () => {
  if (wss.clients.size === 0) return; // Save resources if no clients
  try {
    const load = await si.currentLoad();
    const mem = await si.mem();
    const temp = await si.cpuTemperature();
    const time = await si.time();

    const metrics = {
      type: "server_metrics",
      cpuLoadPercent: load.currentLoad,
      ramTotalMb: mem.total / 1048576,
      ramUsedMb: mem.active / 1048576,
      ramUsedPercent: (mem.active / mem.total) * 100,
      chipTempC: temp.main,
      uptimeS: time.uptime,
      receivedAtIso: new Date().toISOString(),
    };
    broadcast(metrics, null);
  } catch (err) {
    console.error("[metrics] fetch failed:", err.message);
  }
}, 5000);

async function start() {
  try {
    await store.connect();
    console.log(`[db] connected to ${DB_TYPE} database`);
  } catch (err) {
    console.error(`[db] connect failed:`, err.message);
  }
  console.log(`[ws] server ready on ws://${HOST === "0.0.0.0" ? "localhost" : HOST}:${PORT}`);
}

start();
