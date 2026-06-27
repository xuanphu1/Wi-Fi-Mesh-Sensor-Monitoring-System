/* eslint-disable no-unused-vars */
import { useMeshRealtime } from "context/meshRealtime";

const DEVICE_STALE_MS = 15_000;
const NODE_STALE_MS = 20_000;
const THROUGHPUT_WINDOW_SECONDS = 60;
const MAX_DEBUG_LOGS = 120;
const DEFAULT_RTC_ISO = "1970-01-01T00:00:00";
const DASHBOARD_REALTIME_CACHE = {
  lastDeviceAt: 0,
  lastUpdateIso: "",
  lastDeviceRtcIso: "",
  lastDataLatencyMs: null,
  latestSensors: [],
  deviceInfo: {
    ssid: "",
    gatewayIp: "",
    staIp: "",
    mac: "",
  },
  throughputSeries: [],
  nodesByIp: new Map(),
  debugLogs: [],
};

function parseDeviceRtcMs(isoText) {
  if (typeof isoText !== "string") return null;
  const iso = isoText.trim();
  if (!iso || iso === DEFAULT_RTC_ISO) return null;
  const parsedDirect = new Date(iso);
  if (Number.isFinite(parsedDirect.getTime())) return parsedDirect.getTime();

  const parsedUtc = new Date(iso.endsWith("Z") ? iso : `${iso}Z`);
  if (Number.isFinite(parsedUtc.getTime())) return parsedUtc.getTime();

  const m = iso.match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})$/);
  if (!m) return null;
  const y = Number(m[1]);
  const mo = Number(m[2]);
  const d = Number(m[3]);
  const h = Number(m[4]);
  const mi = Number(m[5]);
  const s = Number(m[6]);
  if (![y, mo, d, h, mi, s].every(Number.isFinite)) return null;
  if (mo < 1 || mo > 12) return null;
  if (d < 1 || d > 31) return null;
  if (h < 0 || h > 23) return null;
  if (mi < 0 || mi > 59) return null;
  if (s < 0 || s > 59) return null;
  return Date.UTC(y, mo - 1, d, h, mi, s);
}

function decodeHexUtf8(hex) {
  if (typeof hex !== "string") return "";
  const clean = hex.trim().replace(/\s+/g, "");
  if (!clean || clean.length % 2 !== 0 || /[^0-9a-f]/i.test(clean)) return "";
  try {
    return new TextDecoder("utf-8").decode(
      Uint8Array.from(clean.match(/.{1,2}/g) || [], (b) => Number.parseInt(b, 16))
    );
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
  out.gatewayIp = pick("gatewayIp", "routerIp", "ip_gateway", "gateway") || out.gatewayIp;
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

function addThroughputBytes(bytesRef, text) {
  const nowSec = Math.floor(Date.now() / 1000);
  const bytes = new TextEncoder().encode(text).length;
  bytesRef.current.set(nowSec, (bytesRef.current.get(nowSec) || 0) + bytes);
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
    const nextCarry = hasTerminator ? "" : (parts[parts.length - 1] || "");
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

export function useDashboardRealtime() {
  const ctx = useMeshRealtime();
  return {
    wsOpen: ctx.wsOpen,
    connected: ctx.connected,
    nodeStats: ctx.nodeStats,
    nodes: ctx.nodes,
    deviceInfo: ctx.deviceInfo,
    latestSensors: ctx.latestSensors,
    throughput: ctx.throughput,
    lastUpdateIso: ctx.lastUpdateIso,
    lastDeviceRtcIso: ctx.lastDeviceRtcIso,
    lastDataLatencyMs: ctx.lastDataLatencyMs,
    debugLogs: ctx.debugLogs,
    serverMetrics: ctx.serverMetrics,
    serverMetricsSeries: ctx.serverMetricsSeries,
  };
  /*
  const url = getWebSocketUrl();
  const [wsOpen, setWsOpen] = useState(false);
  const [lastDeviceAt, setLastDeviceAt] = useState(() => DASHBOARD_REALTIME_CACHE.lastDeviceAt);
  const [lastUpdateIso, setLastUpdateIso] = useState(() => DASHBOARD_REALTIME_CACHE.lastUpdateIso);
  const [lastDeviceRtcIso, setLastDeviceRtcIso] = useState(() => DASHBOARD_REALTIME_CACHE.lastDeviceRtcIso);
  const [lastDataLatencyMs, setLastDataLatencyMs] = useState(() => DASHBOARD_REALTIME_CACHE.lastDataLatencyMs);
  const [latestSensors, setLatestSensors] = useState(() => [...DASHBOARD_REALTIME_CACHE.latestSensors]);
  const [deviceInfo, setDeviceInfo] = useState(() => ({ ...DASHBOARD_REALTIME_CACHE.deviceInfo }));
  const [throughputSeries, setThroughputSeries] = useState(() => [...DASHBOARD_REALTIME_CACHE.throughputSeries]);
  const [debugLogs, setDebugLogs] = useState(() => [...DASHBOARD_REALTIME_CACHE.debugLogs]);
  const [now, setNow] = useState(Date.now());

  const nodesRef = useRef(new Map(DASHBOARD_REALTIME_CACHE.nodesByIp));
  const throughputBytesRef = useRef(new Map());
  const uartTextCarryRef = useRef("");
  */
}

