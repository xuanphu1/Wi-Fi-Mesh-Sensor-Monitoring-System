const os = require("os");
const { WebSocketServer } = require("ws");
const si = require("systeminformation");


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
  console.log("[net] Gợi ý IP LAN (ESP/Wi‑Fi — URI ws://IP:PORT, không TLS):");
  if (likelyLan.length === 0) {
    console.log("  (không lọc được — xem danh sách đầy đủ bên dưới)");
  } else {
    likelyLan.forEach(({ name, address }) => {
      console.log(`  ws://${address}:${port}   ← ${name}`);
    });
  }
  console.log("[net] Mọi IPv4 trên máy (không gồm 127.0.0.1):");
  if (all.length === 0) {
    console.log("  (không có)");
  } else {
    all.forEach(({ name, address }) => {
      const tag = isLikelyVirtualAdapter(name) ? " [thường là adapter ảo]" : "";
      console.log(`  ${address.padEnd(15)} ${name}${tag}`);
    });
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

const wss = new WebSocketServer({ port: PORT, host: HOST });

wss.on("listening", () => {
  console.log(`WebSocket listening on ws://${HOST === "0.0.0.0" ? "localhost" : HOST}:${PORT} (bind ${HOST})`);
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
  const ip = clientIpPretty(req.socket.remoteAddress || "");
  console.log(`[ws] client connected ${ip} (tổng ${wss.clients.size})`);
  const serverNet = getServerNetworkInfo();
  const state = { uartCarry: "" };
  let nodeSnapshot = [];
  try {
    nodeSnapshot = await store.getNodeSnapshot();
  } catch (err) {
    console.error("[mongo] getNodeSnapshot failed:", err.message);
  }

  ws.send(
    JSON.stringify({
      type: "welcome",
      t: Date.now(),
      message: "Dashboard WS server",
      serverNet,
      nodeSnapshot,
    })
  );

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

      if (parsed && parsed.type === "history_export_mac_request") {
        try {
          const csv = await store.getHistoryAllForMacCsv(parsed.mac, parsed.from, parsed.to, (progress, total) => {
             ws.send(
               JSON.stringify({
                 type: "history_export_mac_progress",
                 requestId: parsed.requestId || null,
                 mac: parsed.mac || "",
                 progress,
                 total,
               })
             );
          });
          ws.send(
            JSON.stringify({
              type: "history_export_mac_response",
              requestId: parsed.requestId || null,
              mac: parsed.mac || "",
              csv,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({
              type: "history_export_mac_response",
              requestId: parsed.requestId || null,
              mac: parsed.mac || "",
              error: err.message,
            })
          );
        }
        return;
      }

      if (parsed && parsed.type === "history_meta_request") {
        try {
          const macs = await store.getHistoryMacs(parsed.limit || 200);
          ws.send(
            JSON.stringify({
              type: "history_meta_response",
              requestId: parsed.requestId || null,
              macs,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({
              type: "history_meta_response",
              requestId: parsed.requestId || null,
              macs: [],
              error: err.message,
            })
          );
        }
        return;
      }

      if (parsed && parsed.type === "history_series_request") {
        try {
          const items = await store.getHistorySeries({
            mac: parsed.mac,
            sensorName: parsed.sensorName,
            field: parsed.field,
            from: parsed.from,
            to: parsed.to,
            limit: parsed.limit || 2000,
          });
          ws.send(
            JSON.stringify({
              type: "history_series_response",
              requestId: parsed.requestId || null,
              mac: parsed.mac || "",
              sensorName: parsed.sensorName || "",
              field: parsed.field || "",
              items,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({
              type: "history_series_response",
              requestId: parsed.requestId || null,
              mac: parsed.mac || "",
              sensorName: parsed.sensorName || "",
              field: parsed.field || "",
              items: [],
              error: err.message,
            })
          );
        }
        return;
      }

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
