/* eslint-disable no-unused-vars */
import { useMeshRealtime } from "context/meshRealtime";

/** Mark node offline if no data in 3 seconds. */
const STALE_MS = 7_000;
const DEFAULT_RTC_ISO = "1970-01-01T00:00:00";
const MESH_NODES_CACHE = {
  registry: new Map(),
  uartCarry: "",
};

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

function parseDeviceRtcMs(isoText) {
  if (typeof isoText !== "string") return null;
  const iso = isoText.trim();
  if (!iso || iso === DEFAULT_RTC_ISO) return null;
  const parsedDirect = new Date(iso);
  if (Number.isFinite(parsedDirect.getTime())) return parsedDirect.getTime();
  const parsedUtc = new Date(iso.endsWith("Z") ? iso : `${iso}Z`);
  if (Number.isFinite(parsedUtc.getTime())) return parsedUtc.getTime();
  return null;
}

function ingestParsedMesh(registry, data) {
  const ip = data.staIpv4;
  if (!ip || ip === "0.0.0.0") return registry;
  const next = new Map(registry);
  const prev = next.get(ip) || {};
  const nowMs = Date.now();
  const deviceRtcMs = parseDeviceRtcMs(data.rtcIso);
  next.set(ip, {
    ...prev,
    id: ip,
    ip,
    meshLevel: data.meshLevel,
    schemaVersion: data.schemaVersion,
    firmwareVersion: data.firmwareVersion || prev.firmwareVersion || "0.0.0",
    runtimeErrors: Array.isArray(data.runtimeErrors) ? data.runtimeErrors : (prev.runtimeErrors || []),
    portCount: Array.isArray(data.ports) ? data.ports.length : 0,
    lastSeenMs: nowMs,
    rtcIso: data.rtcIso,
    latencyMs: deviceRtcMs != null ? Math.max(0, nowMs - deviceRtcMs) : null,
    ports: Array.isArray(data.ports) ? data.ports : [],
    rawPayload: data.raw && typeof data.raw === "object" ? data.raw : null,
  });
  return next;
}

function applyWsText(registry, text, uartTextCarryRef) {
  let obj;
  try {
    obj = JSON.parse(text);
  } catch {
    return registry;
  }

  if (obj && obj.type === "welcome") {
    // nodeSnapshot is no longer provided in the welcome message.
    return registry;
  }

  if (obj && obj.type === "uart_rx" && typeof obj.hex === "string") {
    let r = registry;
    const decoded = decodeHexUtf8(obj.hex);
    const carry = typeof uartTextCarryRef?.current === "string" ? uartTextCarryRef.current : "";
    const merged = `${carry}${decoded}`;
    const parts = merged.split(/\r?\n/);
    const hasTerminator = /\r?\n$/.test(merged);
    const completeLines = hasTerminator ? parts : parts.slice(0, -1);
    const nextCarry = hasTerminator ? "" : (parts[parts.length - 1] || "");
    if (uartTextCarryRef) {
      uartTextCarryRef.current = nextCarry.slice(-2048);
      MESH_NODES_CACHE.uartCarry = uartTextCarryRef.current;
    }

    completeLines
      .map((s) => s.trim())
      .filter(Boolean)
      .forEach((line) => {
        const parsed = parseMeshUdpSensorString(line);
        if (parsed.ok) r = ingestParsedMesh(r, parsed.data);
      });
    return r;
  }

  if (obj && obj.type === "uart_rx" && typeof obj.payload === "string") {
    const payloadText = obj.payload.trim();
    if (!payloadText) return registry;
    const parsed = parseMeshUdpSensorString(payloadText);
    if (parsed.ok) return ingestParsedMesh(registry, parsed.data);
    return registry;
  }

  const parsed = parseMeshUdpSensorPayload(obj);
  if (parsed.ok) return ingestParsedMesh(registry, parsed.data);
  return registry;
}

/**
 * Lắng nghe WebSocket dashboard, tích lũy node mesh theo STA IPv4 (`i`) và level (`n`).
 * @returns {{ connected: boolean, nodes: Array<object>, registrySize: number }}
 */
export function useMeshNodesFromWebSocket() {
  const ctx = useMeshRealtime();
  return {
    connected: ctx.wsOpen,
    nodes: ctx.nodes,
    registrySize: ctx.registrySize,
  };
  /*
  const url = getWebSocketUrl();
  const [connected, setConnected] = useState(false);
  const [registry, setRegistry] = useState(() => new Map(MESH_NODES_CACHE.registry));
  const [now, setNow] = useState(() => Date.now());
  const uartTextCarryRef = useRef(MESH_NODES_CACHE.uartCarry || "");
  */
}
