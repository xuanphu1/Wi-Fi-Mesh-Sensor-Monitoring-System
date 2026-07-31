export const WS_URL_STORAGE_KEY = "msms_websocket_url";
export const WS_SOURCE_STORAGE_KEY = "msms_websocket_source";
export const WS_CONFIG_CHANGED_EVENT = "msms:websocket-config-changed";

export const WS_SOURCE_LOCAL = "local";
export const WS_SOURCE_SERVER = "server";
export const WS_SOURCE_CUSTOM = "custom";

export const SERVER_WEBSOCKET_URL = "wss://systemmsems.msems.click/ws";

export function getLocalWebSocketUrl() {
  const host =
    typeof window !== "undefined" && window.location.hostname
      ? window.location.hostname
      : "localhost";
  return `ws://${host}:9090/ws`;
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
  if (typeof window !== "undefined") {
    const stored = window.localStorage.getItem(WS_URL_STORAGE_KEY);
    const checked = validateWebSocketUrl(stored);
    if (checked.valid) return checked.url;
  }

  const fromEnv = process.env.REACT_APP_WS_URL || process.env.REACT_APP_GATEWAY_WS_URL;
  if (fromEnv != null && String(fromEnv).trim() !== "") {
    return String(fromEnv).trim();
  }
  return getLocalWebSocketUrl();
}

export function getWebSocketSourceSettings() {
  const url = getWebSocketUrl();
  const storedSource =
    typeof window !== "undefined"
      ? window.localStorage.getItem(WS_SOURCE_STORAGE_KEY)
      : null;
  const source =
    storedSource === WS_SOURCE_LOCAL ||
    storedSource === WS_SOURCE_SERVER ||
    storedSource === WS_SOURCE_CUSTOM
      ? storedSource
      : url === SERVER_WEBSOCKET_URL
        ? WS_SOURCE_SERVER
        : url === getLocalWebSocketUrl()
          ? WS_SOURCE_LOCAL
          : WS_SOURCE_CUSTOM;

  return { source, url };
}

export function saveWebSocketSourceSettings(source, value) {
  const selectedUrl =
    source === WS_SOURCE_LOCAL
      ? getLocalWebSocketUrl()
      : source === WS_SOURCE_SERVER
        ? SERVER_WEBSOCKET_URL
        : value;
  const checked = validateWebSocketUrl(selectedUrl);
  if (!checked.valid) return checked;

  if (typeof window !== "undefined") {
    window.localStorage.setItem(WS_SOURCE_STORAGE_KEY, source);
    window.localStorage.setItem(WS_URL_STORAGE_KEY, checked.url);
    window.dispatchEvent(
      new CustomEvent(WS_CONFIG_CHANGED_EVENT, {
        detail: { source, url: checked.url },
      })
    );
  }

  return { valid: true, source, url: checked.url };
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
