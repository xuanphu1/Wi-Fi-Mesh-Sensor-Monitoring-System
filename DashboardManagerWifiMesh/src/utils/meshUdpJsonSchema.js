/**
 * Parser JSON compact từ firmware mesh (MESH_UDP_JSON_SCHEMA).
 * Định dạng: {"v":schema,"n":level,"i":"STA IPv4","t":"YYYY-MM-DDTHH:MM:SS","ver":"x.x.x","err":["0xNN",...],"p":[[port,type,v0,...], ...]}
 *
 * @typedef {{
 *   wirePort: number,
 *   portIndex: number,
 *   sensorType: number,
 *   sensorName: string,
 *   readings: Array<{ index: number, key: string, label: string, unit: string, value: number }>,
 *   rawRow: number[],
 * }} ParsedMeshUdpPortRow
 *
 * @typedef {{
 *   schemaVersion: number,
 *   meshLevel: number,
 *   staIpv4: string,
 *   packetLoss: number | null,
 *   rtcIso: string,
 *   firmwareVersion: string,
 *   runtimeErrors: string[],
 *   ports: ParsedMeshUdpPortRow[],
 *   raw: object,
 * }} ParsedMeshUdpPayload
 */

/** Phiên bản schema hiện tại phía nhận (đồng bộ firmware khi đổi cấu trúc). */
export const MESH_UDP_JSON_SCHEMA = 2;
const DEFAULT_RTC_ISO = "1970-01-01T00:00:00";

/**
 * SensorType_t — trùng typedef firmware (SensorTypes.h).
 * Trên wire JSON chỉ gửi type 0…13; SENSOR_NONE (-1) không đẩy lên gói chuẩn.
 */
export const SensorType = Object.freeze({
  SENSOR_NONE: -1,
  SENSOR_BME280: 0,
  SENSOR_MHZ14A: 1,
  SENSOR_PMS7003: 2,
  SENSOR_DHT22: 3,
  SENSOR_AHT10: 4,
  SENSOR_DHT11: 5,
  SENSOR_HTU21D: 6,
});

/** Mảng ID hợp lệ trên wire (0…13), thứ tự giống enum sau SENSOR_NONE. */
export const SENSOR_TYPE_WIRE_IDS = Object.freeze([
  SensorType.SENSOR_BME280,
  SensorType.SENSOR_MHZ14A,
  SensorType.SENSOR_PMS7003,
  SensorType.SENSOR_DHT22,
  SensorType.SENSOR_AHT10,
  SensorType.SENSOR_DHT11,
  SensorType.SENSOR_HTU21D,
]);

/** Trùng `sensor_type_to_name()` — SensorRegistry.c */
const TYPE_NAMES = {
  [-1]: "None",
  0: "BME280",
  1: "MH-Z14A",
  2: "PMS7003",
  3: "DHT22",
  4: "AHT10",
  5: "DHT11",
  6: "HTU21D",
};

/**
 * Khớp `sensor_drivers[].description[]`, `unit[]`, thứ tự `data_fl[]` — SensorRegistry.c
 * (BME280: Temperature → Pressure → Humidity).
 */
export const SENSOR_REGISTRY_FIELDS = Object.freeze({
  [-1]: Object.freeze([]),
  0: Object.freeze([
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
    Object.freeze({ key: "pressure_hPa", description: "Pressure", unit: "hPa" }),
    Object.freeze({ key: "humidity_RH", description: "Humidity", unit: "%" }),
  ]),
  1: Object.freeze([
    Object.freeze({ key: "co2_ppm", description: "CO2", unit: "ppm" }),
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
  ]),
  2: Object.freeze([
    Object.freeze({ key: "pm1_0_ugm3", description: "PM1.0", unit: "ug/m3" }),
    Object.freeze({ key: "pm2_5_ugm3", description: "PM2.5", unit: "ug/m3" }),
    Object.freeze({ key: "pm10_ugm3", description: "PM10", unit: "ug/m3" }),
  ]),
  3: Object.freeze([
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
    Object.freeze({ key: "humidity_RH", description: "Humidity", unit: "%" }),
  ]),
  4: Object.freeze([
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
    Object.freeze({ key: "humidity_RH", description: "Humidity", unit: "%" }),
  ]),
  5: Object.freeze([
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
    Object.freeze({ key: "humidity_RH", description: "Humidity", unit: "%" }),
  ]),
  6: Object.freeze([
    Object.freeze({ key: "temp_C", description: "Temperature", unit: "°C" }),
    Object.freeze({ key: "humidity_RH", description: "Humidity", unit: "%" }),
  ]),
});

const MAX_VALUES_PER_ROW = 5;

function sensorTypeToName(type) {
  const k = Number(type);
  if (Object.prototype.hasOwnProperty.call(TYPE_NAMES, k)) return TYPE_NAMES[k];
  return `UNKNOWN_${k}`;
}

/** type === SENSOR_NONE (-1). */
export function isSensorTypeNone(type) {
  return Number(type) === SensorType.SENSOR_NONE;
}

/** type có trong SensorType_t và không phải SENSOR_NONE (dùng cho kiểm tra wire). */
export function isSensorTypeWireValue(type) {
  const k = Number(type);
  return Number.isFinite(k) && k >= 0 && k <= SensorType.SENSOR_AHT10;
}

/** Danh sách định nghĩa trường theo SensorType_t (không có giá trị đo). */
export function getSensorRegistryFields(sensorType) {
  const k = Number(sensorType);
  const row = SENSOR_REGISTRY_FIELDS[k];
  return row ? [...row] : [];
}

function fieldMetaAt(sensorType, index) {
  const k = Number(sensorType);
  const defs = SENSOR_REGISTRY_FIELDS[k];
  if (defs && defs[index] != null) return defs[index];
  return {
    key: `v${index}`,
    description: `v${index}`,
    unit: "",
  };
}

function extractFirstJsonObjectText(text) {
  if (typeof text !== "string") return null;
  const src = text.trim();
  if (!src) return null;

  let start = -1;
  let depth = 0;
  let inString = false;
  let escaped = false;

  for (let i = 0; i < src.length; i += 1) {
    const ch = src[i];
    if (start < 0) {
      if (ch === "{") {
        start = i;
        depth = 1;
      }
      continue;
    }

    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (ch === "\\") {
        escaped = true;
      } else if (ch === "\"") {
        inString = false;
      }
      continue;
    }

    if (ch === "\"") {
      inString = true;
      continue;
    }
    if (ch === "{") {
      depth += 1;
      continue;
    }
    if (ch === "}") {
      depth -= 1;
      if (depth === 0) {
        return src.slice(start, i + 1);
      }
    }
  }

  return null;
}

/**
 * @param {unknown} obj
 * @returns {boolean}
 */
export function isMeshUdpSensorPayload(obj) {
  if (obj == null || typeof obj !== "object" || Array.isArray(obj)) return false;
  return (
    "v" in obj &&
    "n" in obj &&
    ("i" in obj || "M" in obj) &&
    "p" in obj &&
    Array.isArray(obj.p)
  );
}

/**
 * Chuẩn hoá một hàng [ port, type, v0, ... ].
 * @param {unknown} row
 * @param {number} rowIndex
 * @returns {{ ok: true, row: ParsedMeshUdpPortRow } | { ok: false, errors: string[] }}
 */
export function parseMeshUdpPortRow(row, rowIndex = 0) {
  const errors = [];
  const prefix = `p[${rowIndex}]`;

  if (!Array.isArray(row)) {
    return { ok: false, errors: [`${prefix}: không phải mảng`] };
  }
  if (row.length < 2) {
    return { ok: false, errors: [`${prefix}: cần ít nhất [port, type]`] };
  }

  const wirePort = Number(row[0]);
  const sensorType = Number(row[1]);

  if (!Number.isFinite(wirePort) || wirePort < 1 || wirePort > 255) {
    errors.push(`${prefix}: port không hợp lệ (${row[0]})`);
  }
  if (!Number.isFinite(sensorType)) {
    errors.push(`${prefix}: type không hợp lệ (${row[1]})`);
  }

  const rawValues = row.slice(2);
  const capped = rawValues.slice(0, MAX_VALUES_PER_ROW);
  const readings = [];

  capped.forEach((cell, vi) => {
    const num = Number(cell);
    if (!Number.isFinite(num)) {
      errors.push(`${prefix}: v${vi} không phải số (${cell})`);
    }
    const meta = fieldMetaAt(sensorType, vi);
    readings.push({
      index: vi,
      key: meta.key,
      label: meta.description,
      unit: meta.unit,
      value: num,
    });
  });

  const portIndex = Number.isFinite(wirePort) ? wirePort - 1 : -1;

  const parsed = {
    wirePort,
    portIndex,
    sensorType,
    sensorName: sensorTypeToName(sensorType),
    readings,
    rawRow: row.map((x) => (typeof x === "number" ? x : Number(x))),
  };

  if (errors.length) return { ok: false, errors };
  return { ok: true, row: parsed };
}

/**
 * Parse toàn bộ payload mesh UDP JSON.
 * @param {unknown} obj
 * @returns {{ ok: true, data: ParsedMeshUdpPayload } | { ok: false, errors: string[] }}
 */
export function parseMeshUdpSensorPayload(obj) {
  const errors = [];

  if (obj == null || typeof obj !== "object" || Array.isArray(obj)) {
    return { ok: false, errors: ["payload không phải object"] };
  }

  const schemaVersion =
    obj.v !== undefined && obj.v !== null ? Number(obj.v) : NaN;
  const meshLevel =
    obj.n !== undefined && obj.n !== null ? Number(obj.n) : NaN;
  const packetLossRaw = obj.packetloss ?? obj.packetLoss;
  const packetLoss =
    packetLossRaw !== undefined && packetLossRaw !== null && packetLossRaw !== ""
      ? Number(packetLossRaw)
      : null;
  const staIpv4 = String(obj.M || obj.i || "").trim();
  const rtcIso =
    typeof obj.t === "string" && obj.t.trim().length > 0
      ? obj.t.trim()
      : DEFAULT_RTC_ISO;
  const firmwareVersionRaw = obj.ver;
  const firmwareVersion =
    firmwareVersionRaw == null ? "0.0.0" : String(firmwareVersionRaw).trim() || "0.0.0";
  const runtimeErrorsRaw = Array.isArray(obj.err) ? obj.err : [];
  const runtimeErrors = runtimeErrorsRaw
    .map((e) => String(e).trim())
    .filter((e) => /^0x[0-9a-fA-F]{1,2}$/.test(e))
    .map((e) => {
      const hex = e.slice(2).toUpperCase().padStart(2, "0");
      return `0x${hex}`;
    });

  if (!Number.isFinite(schemaVersion)) errors.push("v (schema) thiếu hoặc không phải số");
  if (!Number.isFinite(meshLevel)) errors.push("n (mesh level) thiếu hoặc không phải số");
  if (packetLoss != null && !Number.isFinite(packetLoss)) errors.push("packetloss không phải số");
  if (typeof obj.i !== "string" && typeof obj.M !== "string") errors.push('i hoặc M phải là string');
  if (obj.t != null && typeof obj.t !== "string") errors.push("t phải là string ISO 8601");
  // ver / err are optional metadata. Do not reject payload when these fields are malformed.

  if (!Array.isArray(obj.p)) {
    errors.push("p phải là mảng các hàng đo");
    return { ok: false, errors };
  }

  const ports = [];
  obj.p.forEach((r, idx) => {
    const one = parseMeshUdpPortRow(r, idx);
    if (!one.ok) {
      errors.push(...one.errors);
      return;
    }
    ports.push(one.row);
  });

  if (errors.length) {
    return { ok: false, errors };
  }

  return {
    ok: true,
    data: {
      schemaVersion,
      meshLevel,
      staIpv4,
      packetLoss,
      rtcIso,
      firmwareVersion,
      runtimeErrors,
      ports,
      raw: obj,
    },
  };
}

/**
 * @param {string} text — JSON UTF-8
 * @returns {{ ok: true, data: ParsedMeshUdpPayload } | { ok: false, errors: string[] }}
 */
export function parseMeshUdpSensorString(text) {
  try {
    const obj = JSON.parse(text);
    return parseMeshUdpSensorPayload(obj);
  } catch {
    const candidate = extractFirstJsonObjectText(text);
    if (candidate) {
      try {
        const obj = JSON.parse(candidate);
        return parseMeshUdpSensorPayload(obj);
      } catch {
        // Ignore and return common parse error below.
      }
    }
    return { ok: false, errors: ["JSON parse failed"] };
  }
}

/**
 * Nếu là mesh payload thì parse; không thì trả `{ matched: false }`.
 * @param {unknown} obj
 */
export function tryParseMeshUdpSensorPayload(obj) {
  if (!isMeshUdpSensorPayload(obj)) {
    return { matched: false };
  }
  const r = parseMeshUdpSensorPayload(obj);
  if (!r.ok) return { matched: true, ok: false, errors: r.errors };
  return { matched: true, ok: true, data: r.data };
}

export { sensorTypeToName };
