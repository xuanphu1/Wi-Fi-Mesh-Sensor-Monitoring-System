const os = require("os");
const { WebSocketServer } = require("ws");
const { createMongoStore } = require("./mongodb-store");

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
  4: "SENSOR_MQ2",
  5: "SENSOR_MQ3",
  6: "SENSOR_MQ4",
  7: "SENSOR_MQ5",
  8: "SENSOR_MQ6",
  9: "SENSOR_MQ7",
  10: "SENSOR_MQ8",
  11: "SENSOR_MQ9",
  12: "SENSOR_MQ135",
  13: "SENSOR_AHT10",
};

function sensorTypeName(typeId) {
  if (typeof typeId !== "number" || !Number.isFinite(typeId)) return "SENSOR_UNKNOWN";
  return SENSOR_TYPE_NAMES[typeId] ?? `SENSOR_UNKNOWN_${typeId}`;
}

const SENSOR_VALUE_FIELD_NAMES = {
  0: ["temp_C", "humidity_RH", "pressure_hPa"],
  1: ["co2_ppm"],
  2: ["pm1_0_ugm3", "pm2_5_ugm3", "pm10_ugm3"],
  3: ["temp_C", "humidity_RH"],
  4: ["v0", "v1", "v2", "v3", "v4"],
  5: ["v0", "v1", "v2", "v3", "v4"],
  6: ["v0", "v1", "v2", "v3", "v4"],
  7: ["v0", "v1", "v2", "v3", "v4"],
  8: ["v0", "v1", "v2", "v3", "v4"],
  9: ["v0", "v1", "v2", "v3", "v4"],
  10: ["v0", "v1", "v2", "v3", "v4"],
  11: ["v0", "v1", "v2", "v3", "v4"],
  12: ["v0", "v1", "v2", "v3", "v4"],
  13: ["temp_C", "humidity_RH"],
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
  return mongoStore.isMeshPayload(o);
}

function logMeshUdpJson(remoteIp, sourceTag, frameIdx, pkt) {
  const v = pkt.v;
  const n = pkt.n;
  const sta = pkt.i;
  const t = typeof pkt.t === "string" ? pkt.t : "";
  console.log(
    `[ws] ${sourceTag} from ${remoteIp} frame[${frameIdx}] header: schema(v)=${v}, mesh_level(n)=${n}, sta_ipv4(i)=${sta}, rtc(t)=${t || "(missing)"}`
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

const mongoStore = createMongoStore({
  mongoUri: MONGO_URI,
  sensorValueFieldNames: SENSOR_VALUE_FIELD_NAMES,
  sensorTypeName,
});

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
    nodeSnapshot = await mongoStore.getNodeSnapshot();
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

      if (parsed && parsed.type === "history_meta_request") {
        try {
          const ips = await mongoStore.getHistoryIps(parsed.limit || 200);
          ws.send(
            JSON.stringify({
              type: "history_meta_response",
              requestId: parsed.requestId || null,
              ips,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({
              type: "history_meta_response",
              requestId: parsed.requestId || null,
              ips: [],
              error: err.message,
            })
          );
        }
        return;
      }

      if (parsed && parsed.type === "history_series_request") {
        try {
          const items = await mongoStore.getHistorySeries({
            ip: parsed.ip,
            field: parsed.field,
            from: parsed.from,
            to: parsed.to,
            limit: parsed.limit || 2000,
          });
          ws.send(
            JSON.stringify({
              type: "history_series_response",
              requestId: parsed.requestId || null,
              ip: parsed.ip || "",
              field: parsed.field || "",
              items,
            })
          );
        } catch (err) {
          ws.send(
            JSON.stringify({
              type: "history_series_response",
              requestId: parsed.requestId || null,
              ip: parsed.ip || "",
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
        logMeshUdpJson(ip, "uart_rx", idx, obj);
        mongoStore.persistMeshPacket(obj, ip, receivedAt);
      });

      if (looksLikeMeshUdpJson(parsed)) {
        logMeshUdpJson(ip, "mesh", 0, parsed);
        mongoStore.persistMeshPacket(parsed, ip, receivedAt);
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

async function start() {
  try {
    await mongoStore.connect();
    console.log(`[mongo] connected: ${MONGO_URI}`);
  } catch (err) {
    console.error("[mongo] connect failed:", err.message);
  }
  console.log(`[ws] server ready on ws://${HOST === "0.0.0.0" ? "localhost" : HOST}:${PORT}`);
}

start();
