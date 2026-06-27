/**
 * URL WebSocket backend (gateway bắn dữ liệu lên internet).
 * `.env`: REACT_APP_WS_URL=ws://your-host:8080
 * Dev: nếu không set env, mặc định ws://<hostname>:8080 (`websocket-server`, không TLS — khớp ESP).
 */
export function getWebSocketUrl() {
  const fromEnv = process.env.REACT_APP_WS_URL || process.env.REACT_APP_GATEWAY_WS_URL;
  if (fromEnv != null && String(fromEnv).trim() !== "") {
    return String(fromEnv).trim();
  }
  if (typeof window !== "undefined") {
    const host = window.location.hostname || "localhost";
    return `ws://${host}:8080`;
  }
  return "ws://localhost:8080";
}
