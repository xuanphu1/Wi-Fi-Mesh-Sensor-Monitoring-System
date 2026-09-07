export const WS_URL_STORAGE_KEY = "msms_websocket_url";
export const WS_SOURCE_STORAGE_KEY = "msms_websocket_source";
export const WS_CONFIG_CHANGED_EVENT = "msms:websocket-config-changed";

export const WS_SOURCE_LOCAL = "local";
export const WS_SOURCE_SERVER = "server";
export const WS_SOURCE_CUSTOM = "custom";

export const SERVER_WEBSOCKET_URL = "wss://msems.click/ws";

export function getLocalWebSocketUrl() {
  if (typeof window !== "undefined" && window.location) {
    const isHttps = window.location.protocol === "https:";
    const host = window.location.hostname || "localhost";
    const port = window.location.port;

    // Production environment (HTTPS / standard domain behind reverse proxy)
    if (isHttps) {
      return `wss://${window.location.host}/ws`;
    }
    if (host !== "localhost" && host !== "127.0.0.1" && (port === "" || port === "80")) {
      return `ws://${window.location.host}/ws`;
    }
    return `ws://${host}:9090/ws`;
  }
  return "ws://localhost:9090/ws";
}

export function validateWebSocketUrl(value) {
  const text = String(value || "").trim();
  try {
    const parsed = new URL(text);
    if (parsed.protocol !== "ws:" && parsed.protocol !== "wss:") {
      return { valid: false, error: "URL must start with ws:// or wss://" };
    }
    return { valid: true, url: parsed.toString().replace(/\/$/, "") };
  } catch {
    return { valid: false, error: "Invalid WebSocket URL" };
  }
}

export function getWebSocketUrl() {
  // Always auto-detect based on current protocol and host - completely automatic!
  return getLocalWebSocketUrl();
}

export function getWebSocketSourceSettings() {
  const url = getWebSocketUrl();
  return { source: WS_SOURCE_SERVER, url };
}

export function saveWebSocketSourceSettings(source, value) {
  return { valid: true, source, url: getWebSocketUrl() };
}

export function getHttpUrl() {
  const wsUrl = getWebSocketUrl();
  let httpUrl = wsUrl
    .replace(/^ws:\/\//i, "http://")
    .replace(/^wss:\/\//i, "https://");
  if (httpUrl.endsWith("/ws")) {
    httpUrl = httpUrl.slice(0, -3);
  } else if (httpUrl.endsWith("/")) {
    httpUrl = httpUrl.slice(0, -1);
  }
  return httpUrl;
}
