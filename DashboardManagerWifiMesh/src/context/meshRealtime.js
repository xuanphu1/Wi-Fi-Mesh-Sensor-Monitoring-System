import { createContext, useContext, useEffect, useMemo, useRef, useState } from "react";

import {
  getHttpUrl,
  getWebSocketUrl,
  WS_CONFIG_CHANGED_EVENT,
  WS_URL_STORAGE_KEY,
} from "utils/wsConfig";
import { parseMeshUdpSensorPayload, parseMeshUdpSensorString } from "utils/meshUdpJsonSchema";

const MeshRealtimeContext = createContext(null);

const UNIFIED_STALE_MS = 8_000;
const STALE_MS_NODES = UNIFIED_STALE_MS;
const DEVICE_STALE_MS = UNIFIED_STALE_MS;
const NODE_STALE_MS = UNIFIED_STALE_MS;

const THROUGHPUT_WINDOW_SECONDS = 60;
const GATEWAY_SERIES_LIMIT = 50;
const GATEWAY_LOG_LIMIT = 50;
const MAX_DEBUG_LOGS = 120;
const DEFAULT_RTC_ISO = "1970-01-01T00:00:00";

function parseDeviceRtcMs(isoText) {
  if (typeof isoText !== "string") return null;
  const iso = isoText.trim();
  if (!iso || iso === DEFAULT_RTC_ISO || iso.startsWith("1970") || iso.startsWith("2000")) return null;
  const parsedDirect = new Date(iso);
  const time = Number.isFinite(parsedDirect.getTime())
    ? parsedDirect.getTime()
    : new Date(iso.endsWith("Z") ? iso : `${iso}Z`).getTime();

  // Any RTC timestamp before 2020-01-01 is un-synchronized RTC
  if (!Number.isFinite(time) || time < 1577836800000) return null;
  return time;
}

function decodeHexUtf8(hex) {
  if (typeof hex !== "string") return "";
  const clean = hex.trim().replace(/\s+/g, "");
  if (!clean || clean.length % 2 !== 0 || /[^0-9a-f]/i.test(clean)) return "";
  try {
    const out = new Uint8Array(clean.length / 2);
    for (let i = 0; i < clean.length; i += 2) {
      out[i / 2] = parseInt(clean.slice(i, i + 2), 16);
    }
    return new TextDecoder("utf-8").decode(out);
  } catch {
    return "";
  }
}

function extractDeviceInfo(msg, current, fallbackStaIp) {
  if (!msg || typeof msg !== "object") return current;
  const out = { ...current };
  const read = (k) => (msg[k] != null ? String(msg[k]) : "");
  const pick = (...keys) => {
    for (const k of keys) {
      const v = read(k).trim();
      if (v) return v;
    }
    return "";
  };

  out.ssid = pick("ssid", "wifiSsid", "wifi_ssid") || out.ssid;
  out.gatewayIp = pick("gatewayIp", "routerIp", "ip_gateway", "gateway", "sta_gateway") || out.gatewayIp;
  out.staIp = pick("ip", "staIp", "wifiIp", "ip_sta", "sta_ipv4") || fallbackStaIp || out.staIp;
  out.mac = pick("mac", "staMac", "wifiMac", "mac_sta") || out.mac;

  // Lấy thông tin máy chạy websocket-server từ gói welcome.serverNet
  if (msg.serverNet && typeof msg.serverNet === "object") {
    const sn = msg.serverNet;
    if (!out.ssid && sn.adapter) out.ssid = String(sn.adapter);
    if (!out.staIp && sn.ip) out.staIp = String(sn.ip);
    if (!out.mac && sn.mac) out.mac = String(sn.mac);
  }

  return out;
}

function normalizeGatewayStatus(msg) {
  if (!msg || msg.type !== "gateway_status") return null;
  const numberOrNull = (value) => {
    const n = Number(value);
    return Number.isFinite(n) ? n : null;
  };

  return {
    clientType: msg.clientType ? String(msg.clientType) : "",
    cpuLoadPercent: numberOrNull(msg.cpu_load_percent),
    ramUsedKb: numberOrNull(msg.ram_used_kb),
    ramUsedPercent: numberOrNull(msg.ram_used_percent),
    batteryVoltageV: numberOrNull(msg.battery_voltage_v),
    batteryPercent: numberOrNull(msg.battery_percent),
    powerSource: msg.power_source ? String(msg.power_source) : "",
    chipTempInternalSupported: Boolean(msg.chip_temp_internal_supported),
    chipTempC: numberOrNull(msg.chip_temp_c),
    uptimeS: numberOrNull(msg.uptime_s),
    wifiSsid: msg.wifi_ssid ? String(msg.wifi_ssid) : "",
    wifiRssi: numberOrNull(msg.wifi_rssi),
    staIp: msg.sta_ip ? String(msg.sta_ip) : "",
    staGateway: msg.sta_gateway ? String(msg.sta_gateway) : "",
    receivedAtIso: new Date().toISOString(),
    raw: msg,
  };
}

function fmtGatewayUptime(seconds) {
  if (typeof seconds !== "number" || !Number.isFinite(seconds)) return "-";
  const s = Math.max(0, Math.floor(seconds));
  const h = Math.floor(s / 3600);
  const m = Math.floor((s % 3600) / 60);
  const sec = s % 60;
  if (h > 0) return `${h}h ${m}m ${sec}s`;
  if (m > 0) return `${m}m ${sec}s`;
  return `${sec}s`;
}

function ingestParsedMesh(registry, data) {
  const ip = data.staIpv4;
  if (!ip || ip === "0.0.0.0") return registry;

  const nowMs = Date.now();
  const next = new Map(registry);
  const prev = next.get(ip) || {};

  // Track reconnects: Node was considered offline if lastSeenMs was > STALE_MS_NODES ago
  const wasOffline = prev.lastSeenMs ? (nowMs - prev.lastSeenMs > STALE_MS_NODES) : false;
  const currentReconnects = typeof prev.reconnectCount === "number" ? prev.reconnectCount : 0;
  const newReconnectCount = wasOffline ? currentReconnects + 1 : currentReconnects;

  const deviceRtcMs = parseDeviceRtcMs(data.rtcIso);
  next.set(ip, {
    ...prev,
    id: ip,
    ip,
    meshLevel: data.meshLevel,
    schemaVersion: data.schemaVersion,
    packetLoss: typeof data.packetLoss === "number" && Number.isFinite(data.packetLoss)
      ? data.packetLoss
      : prev.packetLoss ?? null,
    firmwareVersion: data.firmwareVersion || prev.firmwareVersion || "0.0.0",
    runtimeErrors: Array.isArray(data.runtimeErrors) ? data.runtimeErrors : prev.runtimeErrors || [],
    portCount: Array.isArray(data.ports) ? data.ports.length : 0,
    lastSeenMs: nowMs,
    rtcIso: data.rtcIso,
    latencyMs: deviceRtcMs != null ? Math.max(0, nowMs - deviceRtcMs) : null,
    ports: Array.isArray(data.ports) ? data.ports : [],
    rawPayload: data.raw && typeof data.raw === "object" ? data.raw : null,
    reconnectCount: newReconnectCount,
  });
  return next;
}

function parseMeshFromMessage(msg, uartTextCarryRef) {
  const list = [];

  const direct = parseMeshUdpSensorPayload(msg);
  if (direct.ok) list.push(direct.data);

  if (msg && msg.type === "uart_rx" && typeof msg.payload === "string") {
    const payloadText = msg.payload.trim();
    if (payloadText) {
      const parsedPayload = parseMeshUdpSensorString(payloadText);
      if (parsedPayload.ok) list.push(parsedPayload.data);
    }
  }

  if (msg && msg.type === "uart_rx" && typeof msg.hex === "string") {
    const decoded = decodeHexUtf8(msg.hex);
    const carry = typeof uartTextCarryRef?.current === "string" ? uartTextCarryRef.current : "";
    const merged = `${carry}${decoded}`;

    const parts = merged.split(/\r?\n/);
    const hasTerminator = /\r?\n$/.test(merged);
    const completeLines = hasTerminator ? parts : parts.slice(0, -1);
    const nextCarry = hasTerminator ? "" : parts[parts.length - 1] || "";

    if (uartTextCarryRef) uartTextCarryRef.current = nextCarry.slice(-2048);

    completeLines
      .map((s) => s.trim())
      .filter(Boolean)
      .forEach((line) => {
        const one = parseMeshUdpSensorString(line);
        if (one.ok) list.push(one.data);
      });
  }

  return list;
}

function applyWelcomeToRegistry(registry, obj) {
  // nodeSnapshot is no longer provided in the welcome message.
  // It is now fetched via HTTP REST API.
  return registry;
}

function applyHttpNodesToRegistry(registry, nodes) {
  if (!Array.isArray(nodes)) return registry;

  const next = new Map(registry);
  nodes.forEach((n) => {
    const ip = n && typeof n.ip === "string" ? n.ip : "";
    if (!ip) return;

    const parsedLastSeen = new Date(n.lastSeen).getTime();
    const lastSeenMs = Number.isFinite(parsedLastSeen) ? parsedLastSeen : 0;
    const prev = next.get(ip) || {};

    next.set(ip, {
      ...prev,
      id: ip,
      ip,
        meshLevel: n.meshLevel != null ? Number(n.meshLevel) : prev.meshLevel,
        schemaVersion: n.schemaVersion != null ? Number(n.schemaVersion) : prev.schemaVersion,
        packetLoss: n.packetLoss != null ? Number(n.packetLoss) : prev.packetLoss ?? null,
        portCount: prev.portCount ?? 0,
      lastSeenMs: lastSeenMs || prev.lastSeenMs || 0,
    });
  });

  return next;
}

function addThroughputBytes(bytesRef, text) {
  const nowSec = Math.floor(Date.now() / 1000);
  const bytes = new TextEncoder().encode(text).length;
  bytesRef.current.set(nowSec, (bytesRef.current.get(nowSec) || 0) + bytes);
}

export function MeshRealtimeProvider({ children }) {
  const [url, setUrl] = useState(() => getWebSocketUrl());

  const [wsOpen, setWsOpen] = useState(false);
  const [now, setNow] = useState(Date.now());

  const [registry, setRegistry] = useState(() => new Map());
  const uartTextCarryRef = useRef("");

  const [lastDeviceAt, setLastDeviceAt] = useState(0);
  const [lastUpdateIso, setLastUpdateIso] = useState("");
  const [lastDeviceRtcIso, setLastDeviceRtcIso] = useState("");
  const [lastDataLatencyMs, setLastDataLatencyMs] = useState(null);

  const [latestSensors, setLatestSensors] = useState([]);
  const [deviceInfo, setDeviceInfo] = useState({
    ssid: "",
    gatewayIp: "",
    staIp: "",
    mac: "",
  });
  const [gatewayStatus, setGatewayStatus] = useState(null);
  const [gatewaySeries, setGatewaySeries] = useState([]);
  const [gatewayLogs, setGatewayLogs] = useState([]);

  const [serverMetrics, setServerMetrics] = useState(null);
  const [serverMetricsSeries, setServerMetricsSeries] = useState([]);

  const [throughputSeries, setThroughputSeries] = useState([]);
  const throughputBytesRef = useRef(new Map());
  const wsRef = useRef(null);

  const [debugLogs, setDebugLogs] = useState([]);
  const [nodePowerStates, setNodePowerStates] = useState({});
  const [nodeActuatorStates, setNodeActuatorStates] = useState({});

  const sendMessage = (data) => {
    if (!wsRef.current || wsRef.current.readyState !== 1) return false;
    const payload = typeof data === "string" ? data : JSON.stringify(data);
    wsRef.current.send(payload);
    return true;
  };

  const sendTimeSync = () => {
    if (!wsRef.current || wsRef.current.readyState !== 1) return false;
    const now = new Date();
    const epochSec = Math.floor(now.getTime() / 1000);
    const pad = (n) => String(n).padStart(2, "0");
    const timeStr = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
    const syncPkt = {
      type: "sync_time",
      timestamp: epochSec,
      timestamp_ms: now.getTime(),
      datetime: timeStr,
      year: now.getFullYear(),
      month: now.getMonth() + 1,
      day: now.getDate(),
      hour: now.getHours(),
      min: now.getMinutes(),
      sec: now.getSeconds(),
    };
    wsRef.current.send(JSON.stringify(syncPkt));
    return true;
  };

  useEffect(() => {
    const applyCurrentUrl = () => setUrl(getWebSocketUrl());
    const onStorage = (event) => {
      if (event.key === WS_URL_STORAGE_KEY) applyCurrentUrl();
    };

    window.addEventListener(WS_CONFIG_CHANGED_EVENT, applyCurrentUrl);
    window.addEventListener("storage", onStorage);
    return () => {
      window.removeEventListener(WS_CONFIG_CHANGED_EVENT, applyCurrentUrl);
      window.removeEventListener("storage", onStorage);
    };
  }, []);

  useEffect(() => {
    const t = setInterval(() => setNow(Date.now()), 1000);
    return () => clearInterval(t);
  }, []);

  useEffect(() => {
    const timer = setInterval(() => {
      const secNow = Math.floor(Date.now() / 1000);
      const pointSec = secNow - 1;
      const bytes = throughputBytesRef.current.get(pointSec) || 0;

      setThroughputSeries((prev) => {
        const next = [...prev, { sec: pointSec, bytes }];
        return next.slice(-THROUGHPUT_WINDOW_SECONDS);
      });

      const minKeep = secNow - (THROUGHPUT_WINDOW_SECONDS + 2);
      throughputBytesRef.current.forEach((_v, key) => {
        if (key < minKeep) throughputBytesRef.current.delete(key);
      });
    }, 1000);

    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    let active = true;

    // 1. Fetch initial node snapshot via HTTP
    const fetchNodes = async () => {
      try {
        const httpUrl = `${getHttpUrl()}/api/nodes`;
        const res = await fetch(httpUrl);
        if (!res.ok) throw new Error("Network response was not ok");
        const nodes = await res.json();
        if (active) {
          setNodeRegistry((prev) => applyHttpNodesToRegistry(prev, nodes));
        }
      } catch (err) {
        console.error("Failed to fetch node snapshot via HTTP:", err);
      }
    };
    fetchNodes();

    // 2. Connect WebSocket for Realtime updates
    if (!url) {
      setWsOpen(false);
      return undefined;
    }

    let ws;
    let disposed = false;
    let reconnectTimer;

    const open = () => {
      if (disposed) return;

      try {
        ws = new WebSocket(url);
        wsRef.current = ws;
      } catch {
        setWsOpen(false);
        reconnectTimer = setTimeout(open, 3000);
        return;
      }

      ws.onopen = () => {
        if (disposed) return;
        setWsOpen(true);
        setDebugLogs((prev) => [
          ...prev,
          { id: `${Date.now()}-open`, ts: new Date().toISOString(), level: "info", text: "WebSocket connected" },
        ].slice(-MAX_DEBUG_LOGS));
      };

      ws.onclose = () => {
        if (!disposed) {
          setWsOpen(false);
          setDebugLogs((prev) => [
            ...prev,
            { id: `${Date.now()}-close`, ts: new Date().toISOString(), level: "warn", text: "WebSocket disconnected" },
          ].slice(-MAX_DEBUG_LOGS));
        }
        reconnectTimer = setTimeout(open, 3000);
      };

      ws.onerror = () => {
        if (!disposed) {
          setWsOpen(false);
          setDebugLogs((prev) => [
            ...prev,
            { id: `${Date.now()}-error`, ts: new Date().toISOString(), level: "error", text: "WebSocket error" },
          ].slice(-MAX_DEBUG_LOGS));
        }
      };

      ws.onmessage = (ev) => {
        if (typeof ev.data !== "string") return;

        addThroughputBytes(throughputBytesRef, ev.data);

        let msg;
        try {
          msg = JSON.parse(ev.data);
        } catch {
          setDebugLogs((prev) =>
            [
              ...prev,
              { id: `${Date.now()}-json`, ts: new Date().toISOString(), level: "warn", text: `Invalid JSON message (${ev.data.length} chars)` },
            ].slice(-MAX_DEBUG_LOGS)
          );
          return;
        }

        // Auto-reply to sync_time request from ESP32 / Gateway
        if (
          msg &&
          (msg.type === "request_time_sync" ||
            (msg.type === "sync_time" && msg.action === "get_time") ||
            (msg.type === "sync_time" && !msg.timestamp && !msg.year))
        ) {
          const now = new Date();
          const epochSec = Math.floor(now.getTime() / 1000);
          const pad = (n) => String(n).padStart(2, "0");
          const timeStr = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())} ${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
          const syncPkt = {
            type: "sync_time",
            timestamp: epochSec,
            timestamp_ms: now.getTime(),
            datetime: timeStr,
            year: now.getFullYear(),
            month: now.getMonth() + 1,
            day: now.getDate(),
            hour: now.getHours(),
            min: now.getMinutes(),
            sec: now.getSeconds(),
          };
          if (ws && ws.readyState === 1) {
            ws.send(JSON.stringify(syncPkt));
          }
        }

        if (msg && msg.type === "packetloss_update" && msg.data) {
          setRegistry((prev) => {
            const next = new Map(prev);
            for (const [ip, loss] of Object.entries(msg.data)) {
              const node = next.get(ip);
              if (node) {
                next.set(ip, { ...node, packetLoss: loss });
              }
            }
            return next;
          });
          return;
        }

        const isWelcomeMsg = Boolean(msg && msg.type === "welcome");
        const isGatewayStatus = Boolean(msg && msg.type === "gateway_status");
        const isServerMetrics = Boolean(msg && msg.type === "server_metrics");
        const isDeviceMsg = Boolean(msg && msg.clientType === "esp32");

        // Update registry even if we don't have mesh payload.
        if (isWelcomeMsg) {
          setRegistry((prev) => applyWelcomeToRegistry(prev, msg));
          setDeviceInfo((prev) => extractDeviceInfo(msg, prev, ""));
        }

        const meshPayloads = parseMeshFromMessage(msg, uartTextCarryRef);

        if (meshPayloads.length > 0) {
          const first = meshPayloads[0];
          setDebugLogs((prev) =>
            [
              ...prev,
              {
                id: `${Date.now()}-mesh`,
                ts: new Date().toISOString(),
                level: "info",
                text: `Mesh parsed: ${meshPayloads.length} payload(s), ip=${first?.staIpv4 || "-"}, ports=${Array.isArray(first?.ports) ? first.ports.length : 0}`,
              },
            ].slice(-MAX_DEBUG_LOGS)
          );
        } else if (msg?.type === "uart_rx") {
          setDebugLogs((prev) =>
            [
              ...prev,
              { id: `${Date.now()}-uart`, ts: new Date().toISOString(), level: "warn", text: `uart_rx received but no mesh payload parsed (len=${String(msg?.len ?? "-")})` },
            ].slice(-MAX_DEBUG_LOGS)
          );
        }

        if (isGatewayStatus) {
          const nextGatewayStatus = normalizeGatewayStatus(msg);
          if (nextGatewayStatus) {
            setGatewayStatus(nextGatewayStatus);
            setGatewaySeries((prev) =>
              [
                ...prev,
                {
                  t: Date.now(),
                  cpu: nextGatewayStatus.cpuLoadPercent,
                  ram: nextGatewayStatus.ramUsedPercent,
                  rssi: nextGatewayStatus.wifiRssi === 0 ? null : nextGatewayStatus.wifiRssi,
                },
              ].slice(-GATEWAY_SERIES_LIMIT)
            );
            setGatewayLogs((prev) =>
              [
                {
                  id: `${Date.now()}-gateway`,
                  timeIso: nextGatewayStatus.receivedAtIso,
                  level: "Info",
                  message: `CPU ${Math.round(nextGatewayStatus.cpuLoadPercent ?? 0)}% - RAM ${Math.round(nextGatewayStatus.ramUsedPercent ?? 0)}% - up ${fmtGatewayUptime(nextGatewayStatus.uptimeS)} - RSSI ${nextGatewayStatus.wifiRssi == null || nextGatewayStatus.wifiRssi === 0 ? "N/A" : `${Math.round(nextGatewayStatus.wifiRssi)} dBm`}`,
                },
                ...prev,
              ].slice(0, GATEWAY_LOG_LIMIT)
            );
          }
        }

        if (isServerMetrics) {
          setServerMetrics(msg);
          setServerMetricsSeries((prev) =>
            [
              ...prev,
              {
                t: Date.now(),
                cpu: msg.cpuLoadPercent,
                ram: msg.ramUsedPercent,
                temp: msg.chipTempC,
              },
            ].slice(-GATEWAY_SERIES_LIMIT)
          );
        }

        if (msg && (msg.type === "node_power_status" || msg.type === "power_status")) {
          const key = msg.mac || msg.ip || msg.target || msg.targetMac || "";
          if (key) {
            setNodePowerStates((prev) => ({ ...prev, [key]: msg }));
          }
        }

        if (msg && (msg.type === "node_actuator_status" || msg.type === "actuator_status")) {
          const key = msg.mac || msg.ip || msg.target || msg.targetMac || "";
          if (key) {
            setNodeActuatorStates((prev) => ({ ...prev, [key]: msg }));
          }
        }

        if (meshPayloads.length === 0 && !isDeviceMsg && !isWelcomeMsg && !isGatewayStatus && !isServerMetrics && msg?.type !== "node_power_status" && msg?.type !== "node_actuator_status") return;

        const nowMs = Date.now();

        if (meshPayloads.length > 0 || isDeviceMsg) {
          setLastDeviceAt(nowMs);
          setLastUpdateIso(new Date(nowMs).toISOString());
        }

        if (isDeviceMsg) {
          setDeviceInfo((prev) => extractDeviceInfo(msg, prev, ""));
        } else if (meshPayloads.length > 0) {
          // Keep previous device info; just update host/server fields when they exist in mesh messages.
          setDeviceInfo((prev) => extractDeviceInfo({}, prev, ""));
        }

        if (meshPayloads.length > 0) {
          setRegistry((prev) => {
            let next = prev;
            meshPayloads.forEach((mesh) => {
              next = ingestParsedMesh(next, mesh);
            });
            return next;
          });

          meshPayloads.forEach((mesh) => {
            if (typeof mesh.rtcIso === "string" && mesh.rtcIso.trim()) {
              setLastDeviceRtcIso(mesh.rtcIso.trim());
            }

            const deviceRtcMs = parseDeviceRtcMs(mesh.rtcIso);
            if (deviceRtcMs != null) {
              setLastDataLatencyMs(Math.max(0, nowMs - deviceRtcMs));
            } else {
              setLastDataLatencyMs(null);
            }

            const flattened = [];
            mesh.ports.forEach((portRow) => {
              portRow.readings.forEach((r) => {
                flattened.push({
                  key: `${mesh.staIpv4}:${portRow.wirePort}:${r.key}`,
                  nodeIp: mesh.staIpv4,
                  meshLevel: mesh.meshLevel,
                  port: portRow.wirePort,
                  sensor: portRow.sensorName,
                  label: r.label,
                  unit: r.unit,
                  value: r.value,
                });
              });
            });

            if (flattened.length > 0) setLatestSensors(flattened);
          });
        }
      };
    };

    open();

    return () => {
      disposed = true;
      if (reconnectTimer) clearTimeout(reconnectTimer);
      if (ws) {
        ws.onopen = null;
        ws.onclose = null;
        ws.onerror = null;
        ws.onmessage = null;
        ws.close();
      }
    };
  }, [url]);

  const connected = wsOpen && lastDeviceAt > 0 && now - lastDeviceAt <= UNIFIED_STALE_MS;

  const nodeStats = useMemo(() => {
    let online = 0;
    let offline = 0;
    registry.forEach((n) => {
      if (wsOpen && now - (n.lastSeenMs || 0) <= UNIFIED_STALE_MS) online += 1;
      else offline += 1;
    });
    return { online, offline, total: online + offline };
  }, [registry, now, wsOpen]);

  const nodes = useMemo(() => {
    const list = Array.from(registry.values()).map((e) => {
      const age = now - e.lastSeenMs;
      const online = wsOpen && age < UNIFIED_STALE_MS;
      const ml = e.meshLevel != null ? Number(e.meshLevel) : null;

      return {
        id: e.id,
        name: "Mesh Node",
        location: `${e.ip} · ${e.portCount ?? 0} sensors · schema v${e.schemaVersion ?? "?"}`,
        type: ml != null && Number.isFinite(ml) ? `MESH L${ml}` : "MESH",
        meshLevel: ml,
        schemaVersion: e.schemaVersion ?? null,
        packetLoss: typeof e.packetLoss === "number" && Number.isFinite(e.packetLoss) ? e.packetLoss : null,
        firmwareVersion: e.firmwareVersion || "0.0.0",
        runtimeErrors: Array.isArray(e.runtimeErrors) ? e.runtimeErrors : [],
        portCount: e.portCount ?? 0,
        online,
        rssi: null,
        lastSeenIso: new Date(e.lastSeenMs).toISOString(),
        latencyMs: typeof e.latencyMs === "number" ? e.latencyMs : null,
        ports: Array.isArray(e.ports) ? e.ports : [],
        rawPayload: e.rawPayload && typeof e.rawPayload === "object" ? e.rawPayload : null,
        reconnectCount: typeof e.reconnectCount === "number" ? e.reconnectCount : 0,
        _source: "websocket",
      };
    });

    list.sort((a, b) => {
      const la = a.meshLevel != null ? a.meshLevel : 999;
      const lb = b.meshLevel != null ? b.meshLevel : 999;
      if (la !== lb) return la - lb;
      return String(a.id).localeCompare(String(b.id));
    });

    return list;
  }, [registry, now]);

  const throughput = useMemo(() => {
    const labels = throughputSeries.map((p) =>
      new Date(p.sec * 1000).toLocaleTimeString([], { hour12: false, minute: "2-digit", second: "2-digit" })
    );
    const bytesPerSec = throughputSeries.map((p) => p.bytes);
    const kbps = throughputSeries.map((p) => Number(((p.bytes * 8) / 1000).toFixed(2)));
    const currentBytes = bytesPerSec[bytesPerSec.length - 1] || 0;
    const avgBytes = bytesPerSec.length > 0 ? Math.round(bytesPerSec.reduce((a, b) => a + b, 0) / bytesPerSec.length) : 0;

    return {
      labels,
      kbps,
      currentBytesPerSec: currentBytes,
      averageBytesPerSec: avgBytes,
    };
  }, [throughputSeries]);

  const value = useMemo(
    () => ({
      wsOpen,
      connected,
      nodeStats,
      gatewayStatus,
      gatewaySeries,
      gatewayLogs,
      deviceInfo,
      latestSensors,
      throughput,
      lastUpdateIso,
      lastDeviceRtcIso,
      lastDataLatencyMs,
      debugLogs,
      nodes,
      registrySize: registry.size,
      serverMetrics,
      serverMetricsSeries,
      sendTimeSync,
      sendMessage,
      nodePowerStates,
      nodeActuatorStates,
    }),
    [
      wsOpen,
      connected,
      nodeStats,
      gatewayStatus,
      gatewaySeries,
      gatewayLogs,
      deviceInfo,
      latestSensors,
      throughput,
      lastUpdateIso,
      lastDeviceRtcIso,
      lastDataLatencyMs,
      debugLogs,
      nodes,
      registry,
      serverMetrics,
      serverMetricsSeries,
      nodePowerStates,
      nodeActuatorStates,
    ]
  );

  return <MeshRealtimeContext.Provider value={value}>{children}</MeshRealtimeContext.Provider>;
}

export function useMeshRealtime() {
  const ctx = useContext(MeshRealtimeContext);
  if (!ctx) throw new Error("useMeshRealtime must be used within MeshRealtimeProvider");
  return ctx;
}
