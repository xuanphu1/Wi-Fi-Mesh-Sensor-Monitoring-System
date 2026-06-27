import { DEFAULT_RTC_ISO, ERROR_CODE_DEFS } from '@/constants/mesh';

export function clampNum(value: unknown, min: number, max: number) {
  const n = Number(value);
  if (!Number.isFinite(n)) return min;
  return Math.min(max, Math.max(min, n));
}

export function formatUptime(seconds: unknown) {
  const s = Math.max(0, Math.floor(Number(seconds) || 0));
  const d = Math.floor(s / 86400);
  const h = Math.floor((s % 86400) / 3600);
  const m = Math.floor((s % 3600) / 60);
  const r = s % 60;
  if (d > 0) return `${d}d ${h}h ${m}m`;
  if (h > 0) return `${h}h ${m}m ${r}s`;
  return `${m}m ${r}s`;
}

export function parseDeviceRtcMs(isoText: unknown) {
  if (typeof isoText !== 'string') return null;
  const iso = isoText.trim();
  if (!iso || iso === DEFAULT_RTC_ISO) return null;

  const direct = new Date(iso);
  if (Number.isFinite(direct.getTime())) return direct.getTime();

  const utcFallback = new Date(iso.endsWith('Z') ? iso : `${iso}Z`);
  if (Number.isFinite(utcFallback.getTime())) return utcFallback.getTime();
  return null;
}

export function formatTime(value?: number | string | null) {
  if (!value) return '--';
  const d = typeof value === 'number' ? new Date(value) : new Date(value);
  if (!Number.isFinite(d.getTime())) return '--';
  return d.toLocaleString();
}

export function runtimeErrorSummary(codes: string[]) {
  if (!codes.length) return 'No runtime errors.';
  return codes.map((code) => `${code}: ${ERROR_CODE_DEFS[code] || 'Runtime issue'}`).join('\n');
}

export function formatBytes(bytes: number) {
  if (!Number.isFinite(bytes)) return '--';
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1024 * 1024) return `${(bytes / 1024).toFixed(1)} KB`;
  return `${(bytes / (1024 * 1024)).toFixed(1)} MB`;
}
