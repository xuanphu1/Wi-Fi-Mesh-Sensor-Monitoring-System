const path = require("path");
const fs = require("fs");
const crypto = require("crypto");
const os = require("os");
const multer = require("multer");
const { WebSocketServer } = require("ws");
const si = require("systeminformation");
const express = require("express");
const cors = require("cors");
const http = require("http");
const swaggerUi = require("swagger-ui-express");
const swaggerJsdoc = require("swagger-jsdoc");
const { Bonjour } = require("bonjour-service");


const PORT = Number(process.env.WS_PORT || process.env.PORT || 9090);
const HOST = process.env.WS_HOST || "0.0.0.0";
const MDNS_HOSTNAME = process.env.MDNS_HOSTNAME || "systemmsems";
let bonjour = null;
let mdnsService = null;

function startMdns(port) {
  const preferredNetwork = getServerNetworkInfo();
  const mdnsOptions = preferredNetwork?.ip
    ? { interface: preferredNetwork.ip }
    : {};

  bonjour = new Bonjour(mdnsOptions);
  mdnsService = bonjour.publish({
    name: "System MSEMS WebSocket",
    host: `${MDNS_HOSTNAME}.local`,
    type: "http",
    protocol: "tcp",
    port,
    disableIPv6: true,
    txt: {
      service: "websocket",
      path: "/ws",
    },
  });

  mdnsService.on("up", () => {
    console.log(`[mDNS] Local server: ws://${MDNS_HOSTNAME}.local:${port}/ws`);
    if (preferredNetwork?.ip) {
      console.log(
        `[mDNS] Advertising on ${preferredNetwork.adapter}: ${preferredNetwork.ip}`
      );
    }
  });

  mdnsService.on("error", (error) => {
    console.warn(`[mDNS] Could not advertise ${MDNS_HOSTNAME}.local:`, error.message);
  });
}

function stopMdns() {
  if (mdnsService) {
    mdnsService.stop();
    mdnsService = null;
  }
  if (bonjour) {
    bonjour.destroy();
    bonjour = null;
  }
}

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

const { createSqliteStore } = require("./sqlite-store");
const defaultSqlitePath = path.join(__dirname, "mesh-data.sqlite");
const SQLITE_DB_PATH = process.env.SQLITE_DB_PATH || defaultSqlitePath;

const store = createSqliteStore({
  dbPath: SQLITE_DB_PATH,
  sensorValueFieldNames: SENSOR_VALUE_FIELD_NAMES,
  sensorTypeName,
});

store.connect().then(() => {
  console.log(`[db] SQLite connected successfully: ${SQLITE_DB_PATH}`);
}).catch((err) => {
  console.error(`[db] SQLite connection failed:`, err.message);
});

const app = express();
app.use(cors());
app.use(express.json());

// --- Firmware Upload Configuration (In-Memory Buffer to SQLite BLOB) ---
const storage = multer.memoryStorage();
const upload = multer({
  storage,
  limits: { fileSize: 30 * 1024 * 1024 }, // 30MB max
});

// --- OTA REST Endpoints ---

/**
 * @swagger
 * /api/ota/upload:
 *   post:
 *     summary: Upload a firmware binary file (.bin)
 *     description: Stores the binary file directly as a BLOB in SQLite under 'namefile_version.bin'.
 */
app.post("/api/ota/upload", upload.single("file"), async (req, res) => {
  try {
    if (!req.file) {
      return res.status(400).json({ error: "No firmware binary file uploaded" });
    }

    const targetType = (req.body.targetType || "gateway").toLowerCase().trim();
    const version = (req.body.version || "1.0.0").trim().replace(/[^a-zA-Z0-9.-]/g, "_");
    const rawBaseName = path.parse(req.file.originalname).name.replace(/[^a-zA-Z0-9_-]/g, "_");
    const baseName = rawBaseName || targetType;
    const filename = `${baseName}_${version}.bin`;
    const channel = req.body.channel || "stable";
    const notes = req.body.notes || "";
    const originalName = req.file.originalname;
    const fileSize = req.file.size;
    const fileBuffer = req.file.buffer;

    // Calculate MD5 hash directly from buffer
    const checksum = crypto.createHash("md5").update(fileBuffer).digest("hex");

    const record = await store.saveFirmware({
      targetType,
      version: req.body.version || "1.0.0",
      filename,
      originalName,
      fileSize,
      checksum,
      bin: fileBuffer, // Stored directly as BLOB in SQLite!
      channel,
      notes,
    });

    console.log(`[ota] Firmware saved directly in SQLite: ${filename} (${(fileSize / (1024 * 1024)).toFixed(2)} MB), MD5: ${checksum}`);
    res.json({ success: true, firmware: record });
  } catch (err) {
    console.error("[ota] Upload error:", err.message);
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/ota/firmwares:
 *   get:
 *     summary: Retrieve list of uploaded firmwares from SQLite
 */
app.get("/api/ota/firmwares", async (req, res) => {
  try {
    const targetType = req.query.targetType || null;
    const list = await store.getFirmwares(targetType);
    res.json({ firmwares: list });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/ota/firmwares/{id}:
 *   delete:
 *     summary: Delete a firmware record from SQLite
 */
app.delete("/api/ota/firmwares/:id", async (req, res) => {
  try {
    const id = Number(req.params.id);
    const item = await store.deleteFirmware(id);
    res.json({ success: true, deleted: item });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/ota/download/{filename}:
 *   get:
 *     summary: Endpoint for ESP32 Gateway/Nodes to download binary firmware from SQLite BLOB
 */
app.get("/api/ota/download/:filename", async (req, res) => {
  try {
    const filename = req.params.filename;
    const safeFilename = path.basename(filename);

    // Read directly from SQLite BLOB column
    const fw = await store.getFirmwareBinary(safeFilename);
    if (!fw || !fw.bin) {
      console.warn(`[ota] Download requested but firmware binary not found in SQLite: ${safeFilename}`);
      return res.status(404).send("Firmware binary not found in SQLite database");
    }

    res.setHeader("Content-Type", "application/octet-stream");
    res.setHeader("Content-Disposition", `attachment; filename="${safeFilename}"`);
    res.setHeader("Content-Length", fw.fileSize || fw.bin.length);

    console.log(`[ota] ESP32 streaming firmware from SQLite: ${safeFilename} (${fw.fileSize || fw.bin.length} bytes) to IP ${req.ip}`);
    res.end(fw.bin);
  } catch (err) {
    console.error("[ota] Download error:", err.message);
    res.status(500).send(err.message);
  }
});

/**
 * @swagger
 * /api/ota/trigger:
 *   post:
 *     summary: Trigger an OTA update command to Gateway/Mesh
 */
app.post("/api/ota/trigger", async (req, res) => {
  try {
    const { targetType = "gateway", targetDetail, targetMac, version, filename, firmwareId } = req.body;
    const finalTargetDetail = targetDetail || targetMac || (targetType === "gateway" ? "gateway" : "all");
    const isRootOrGateway = targetType === "gateway" || finalTargetDetail === "root";
    
    let fw = null;
    if (firmwareId) {
      fw = await store.getFirmwareById(firmwareId);
    } else if (filename) {
      fw = await store.getFirmwareByFilename(filename);
    } else if (version) {
      const allFw = await store.getFirmwares(targetType);
      fw = allFw.find(f => f.version === version) || allFw[0];
    } else if (isRootOrGateway) {
      const allFw = await store.getFirmwares(targetType);
      fw = allFw[0] || null;
    }

    if (isRootOrGateway && !fw) {
      return res.status(404).json({ error: "Firmware binary not found in database. Please upload a firmware file first." });
    }

    // Auto-detect domain/IP and protocol (HTTP/HTTPS) from request or fallback to LAN IP
    const hostHeader = req.get("host");
    const isDomain = hostHeader && !hostHeader.includes("localhost") && !/^\d+\.\d+\.\d+\.\d+/.test(hostHeader);
    const protocol = req.headers["x-forwarded-proto"] || (req.secure || isDomain ? "https" : "http");
    
    let baseUrl;
    if (hostHeader && hostHeader !== "localhost" && !hostHeader.startsWith("127.0.0.1")) {
      baseUrl = `${protocol}://${hostHeader}`;
    } else {
      const serverNet = getServerNetworkInfo();
      const serverIp = serverNet?.ip || "192.168.1.100";
      baseUrl = `http://${serverIp}:${PORT}`;
    }

    const downloadUrl = fw ? `${baseUrl}/api/ota/download/${fw.filename}` : "";
    const jobId = `JOB-${targetType.toUpperCase()}-${Date.now()}`;
    const fwVersion = fw ? fw.version : "root-active";

    const job = await store.createOtaJob({
      jobId,
      targetType,
      targetDetail: finalTargetDetail,
      targetMac: finalTargetDetail,
      version: fwVersion,
      firmwareUrl: downloadUrl || "internal://root-mesh-distribution",
      fileSize: fw?.fileSize || 0,
      checksum: fw?.checksum || "",
      summary: isRootOrGateway
        ? `Target: ${targetType.toUpperCase()} (${finalTargetDetail}) | Version: ${fwVersion}`
        : `Target: MESH NODES (${finalTargetDetail}) | Source: Root Node Internal Distribution`,
    });

    const commandPayload = {
      type: "ota_start",
      target: targetType,
      targetDetail: finalTargetDetail,
      targetMac: finalTargetDetail,
      jobId: jobId,
    };

    // Only include download URL, size, md5, ver if target is Gateway or Root Node (requires HTTP download)
    if (isRootOrGateway && fw) {
      commandPayload.url = downloadUrl;
      commandPayload.ver = fw.version;
      commandPayload.size = fw.fileSize;
      commandPayload.md5 = fw.checksum;
    }

    console.log(`[ota] Command sent to Gateway/Mesh:`, JSON.stringify(commandPayload));
    broadcast(commandPayload);

    res.json({ success: true, job, command: commandPayload });
  } catch (err) {
    console.error("[ota] Trigger error:", err.message);
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/ota/jobs:
 *   get:
 *     summary: Retrieve OTA Jobs history from SQLite
 */
app.get("/api/ota/jobs", async (req, res) => {
  try {
    const targetType = req.query.targetType || null;
    const limit = Number(req.query.limit) || 50;
    const jobs = await store.getOtaJobs(targetType, limit);
    res.json({ jobs });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

/**
 * @swagger
 * /api/ota/jobs/{jobId}/status:
 *   patch:
 *     summary: Update an OTA Job status directly (e.g. on client timeout)
 */
app.patch("/api/ota/jobs/:jobId/status", async (req, res) => {
  try {
    const { jobId } = req.params;
    const { status, progress, summary, errorMsg } = req.body;
    const completedAt = (status === "Completed" || status === "Failed" || status === "Success") ? new Date().toISOString() : null;

    const updated = await store.updateOtaJobProgress(jobId, {
      status,
      progress,
      summary,
      errorMsg,
      completedAt,
    });
    res.json({ success: true, job: updated });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

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
  startMdns(PORT);
});

function shutdown(signal) {
  console.log(`[server] ${signal} received, shutting down`);
  stopMdns();
  server.close(() => process.exit(0));
}

process.once("SIGINT", () => shutdown("SIGINT"));
process.once("SIGTERM", () => shutdown("SIGTERM"));

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

    let parsed;
    try {
      parsed = JSON.parse(text);
    } catch {
      console.log(`[ws] Non-JSON text from ${ip}, forwarding as { type: "text" }`);
      broadcast({ type: "text", body: text }, ws);
      return;
    }

    const msgType =
      parsed && typeof parsed === "object"
        ? parsed.type || "unknown"
        : "non-object";
    console.log(`[ws] Parsed JSON from ${ip}: type=${msgType}`);

    const receivedAt = new Date();
    try {
      const uartPackets = parseUartFrames(parsed, state);
      uartPackets.forEach(({ idx, obj }) => {
        trackSeq(obj);
        logMeshUdpJson(ip, "uart_rx", idx, obj);
        Promise.resolve(store.persistMeshPacket(obj, ip, receivedAt)).catch(
          (error) =>
            console.error("[db] persistMeshPacket failed:", error.message)
        );
      });

      if (looksLikeMeshUdpJson(parsed)) {
        trackSeq(parsed);
        logMeshUdpJson(ip, "mesh", 0, parsed);
        Promise.resolve(store.persistMeshPacket(parsed, ip, receivedAt)).catch(
          (error) =>
            console.error("[db] persistMeshPacket failed:", error.message)
        );
      }
      
      if (
        parsed &&
        parsed.type === "gateway_status" &&
        typeof store.persistGatewayStatus === "function"
      ) {
        Promise.resolve(
          store.persistGatewayStatus(parsed, ip, receivedAt)
        ).catch((error) =>
          console.error("[db] persistGatewayStatus failed:", error.message)
        );
      }

      // Handle OTA status/progress reports from Gateway / Mesh Nodes
      if (parsed && (parsed.type === "ota_progress" || parsed.type === "ota_status")) {
        const jobId = parsed.jobId;
        const status = parsed.status || (parsed.percent === 100 ? "Success" : "In progress");
        const progress = Number(parsed.percent ?? parsed.progress ?? 0);
        const bytesRead = Number(parsed.bytes_read || 0);
        const totalBytes = Number(parsed.total_bytes || 0);
        const runningPart = parsed.running_partition || "";
        const targetPart = parsed.target_partition || "";
        const message = parsed.message || parsed.msg || "";
        const errorMsg = parsed.error || parsed.err || (status === "Failed" ? message : "");
        
        let summaryParts = [`Status: ${status}`];
        if (runningPart && targetPart) {
          summaryParts.push(`Partition: ${runningPart} -> ${targetPart}`);
        }
        if (totalBytes > 0) {
          summaryParts.push(`${(bytesRead / 1024).toFixed(1)}KB / ${(totalBytes / 1024).toFixed(1)}KB (${progress}%)`);
        } else {
          summaryParts.push(`${progress}%`);
        }
        if (message) summaryParts.push(message);
        const summary = summaryParts.join(" | ");

        const completedAt = (status === "Completed" || status === "Failed" || status === "Success") ? new Date().toISOString() : null;

        if (jobId) {
          store.updateOtaJobProgress(jobId, { status, progress, summary, errorMsg, completedAt }).catch(e => console.error("[db] updateOtaJobProgress error:", e.message));
        }

        const finalTargetDetail = parsed.targetDetail || parsed.targetMac || (parsed.target === "gateway" ? "gateway" : "all");

        const otaBroadcastPayload = {
          type: "ota_progress",
          jobId,
          target: parsed.target || "gateway",
          targetDetail: finalTargetDetail,
          targetMac: finalTargetDetail,
          status,
          progress,
          percent: progress,
          bytes_read: bytesRead,
          total_bytes: totalBytes,
          running_partition: runningPart,
          target_partition: targetPart,
          message,
          summary,
          errorMsg,
        };

        console.log(`[ota] Progress update for ${parsed.target || "gateway"}: [${status} ${progress}%] ${runningPart ? `(${runningPart} -> ${targetPart})` : ""}`);
        broadcast(otaBroadcastPayload);
      }
    } catch (error) {
      console.error(`[ws] Processing JSON from ${ip} failed:`, error.message);
    }

    // Database or parser failures must never change a valid JSON message into
    // plain text. Dashboard clients always receive the original structure.
    broadcast(parsed, ws);
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

let cachedHostInfo = null;
async function getHostInfo() {
  if (cachedHostInfo) return cachedHostInfo;
  try {
    const [cpu, osInfo, sys] = await Promise.all([
      si.cpu().catch(() => ({})),
      si.osInfo().catch(() => ({})),
      si.system().catch(() => ({})),
    ]);
    const cpuModel = cpu.brand || os.cpus()[0]?.model || "Unknown CPU";
    const cpuManufacturer = cpu.manufacturer || "";
    cachedHostInfo = {
      cpuBrand: `${cpuManufacturer} ${cpuModel}`.trim(),
      cpuCores: cpu.cores || cpu.physicalCores || os.cpus().length,
      cpuSpeed: cpu.speed ? `${cpu.speed} GHz` : `${(os.cpus()[0]?.speed / 1000).toFixed(2)} GHz`,
      osPlatform: osInfo.platform || os.platform(),
      osDistro: osInfo.distro || os.type(),
      osRelease: osInfo.release || os.release(),
      osArch: osInfo.arch || os.arch(),
      hostname: osInfo.hostname || os.hostname(),
      manufacturer: sys.manufacturer || "Generic System",
      model: sys.model || "Desktop / Server",
      nodeVersion: process.version,
      port: PORT,
    };
    return cachedHostInfo;
  } catch (err) {
    cachedHostInfo = {
      cpuBrand: os.cpus()[0]?.model || "Generic CPU",
      cpuCores: os.cpus().length,
      cpuSpeed: `${(os.cpus()[0]?.speed / 1000).toFixed(2)} GHz`,
      osPlatform: os.platform(),
      osDistro: os.type(),
      osRelease: os.release(),
      osArch: os.arch(),
      hostname: os.hostname(),
      manufacturer: "Generic System",
      model: "Desktop / Server",
      nodeVersion: process.version,
      port: PORT,
    };
    return cachedHostInfo;
  }
}

app.get("/api/system/info", async (req, res) => {
  try {
    const hw = await getHostInfo();
    const [load, mem, temp] = await Promise.all([
      si.currentLoad().catch(() => ({ currentLoad: 0 })),
      si.mem().catch(() => ({ total: os.totalmem(), active: os.totalmem() - os.freemem() })),
      si.cpuTemperature().catch(() => ({ main: null })),
    ]);
    const time = si.time();

    const totalRam = mem.total || os.totalmem();
    const activeRam = mem.active || mem.used || (mem.total - mem.available) || (os.totalmem() - os.freemem());
    const ramUsedPercent = (activeRam / totalRam) * 100;

    res.json({
      hardware: hw,
      metrics: {
        cpuLoadPercent: Number((load.currentLoad || 0).toFixed(1)),
        ramTotalMb: totalRam / 1048576,
        ramUsedMb: activeRam / 1048576,
        ramUsedPercent: Number(ramUsedPercent.toFixed(1)),
        chipTempC: temp.main || null,
        uptimeS: time?.uptime || os.uptime(),
        nodeUptimeS: process.uptime(),
        connectedClients: wss.clients.size,
        receivedAtIso: new Date().toISOString(),
      }
    });
  } catch (err) {
    res.status(500).json({ error: err.message });
  }
});

setInterval(async () => {
  if (wss.clients.size === 0) return; // Save resources if no clients
  try {
    const hw = await getHostInfo();
    const [load, mem, temp] = await Promise.all([
      si.currentLoad().catch(() => ({ currentLoad: 0 })),
      si.mem().catch(() => ({ total: os.totalmem(), active: os.totalmem() - os.freemem() })),
      si.cpuTemperature().catch(() => ({ main: null })),
    ]);
    const time = si.time();

    const totalRam = mem.total || os.totalmem();
    const activeRam = mem.active || mem.used || (mem.total - mem.available) || (os.totalmem() - os.freemem());
    const ramUsedPercent = (activeRam / totalRam) * 100;

    const metrics = {
      type: "server_metrics",
      cpuLoadPercent: Number((load.currentLoad || 0).toFixed(1)),
      ramTotalMb: totalRam / 1048576,
      ramUsedMb: activeRam / 1048576,
      ramUsedPercent: Number(ramUsedPercent.toFixed(1)),
      chipTempC: temp.main || null,
      uptimeS: time?.uptime || os.uptime(),
      nodeUptimeS: process.uptime(),
      hardware: hw,
      connectedClients: wss.clients.size,
      receivedAtIso: new Date().toISOString(),
    };
    broadcast(metrics, null);
  } catch (err) {
    console.error("[metrics] fetch failed:", err.message);
  }
}, 3000);

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
