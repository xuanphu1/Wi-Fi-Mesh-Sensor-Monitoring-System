export type GatewayStatus = {
  type: 'gateway_status';
  clientType: string;
  cpu_load_percent: number;
  ram_used_kb: number;
  ram_used_percent: number;
  battery_voltage_v: number;
  battery_percent: number;
  power_source: string;
  chip_temp_c: number | null;
  chip_temp_internal_supported?: boolean;
  uptime_s: number;
  wifi_ssid: string;
  wifi_rssi: number;
  sta_ip: string;
  sta_gateway: string;
  firmware_version?: string;
};

export type SensorReading = {
  index: number;
  key: string;
  label: string;
  unit: string;
  value: number;
};

export type MeshPort = {
  wirePort: number;
  sensorType: number;
  sensorName: string;
  readings: SensorReading[];
};

export type ParsedMeshPacket = {
  schemaVersion: number;
  meshLevel: number;
  staIpv4: string;
  rtcIso: string;
  firmwareVersion: string;
  runtimeErrors: string[];
  ports: MeshPort[];
  raw: unknown;
};

export type MeshNode = {
  ip: string;
  name: string;
  status: 'Online' | 'Offline';
  lastSeen: number | null;
  schemaVersion: number;
  firmwareVersion: string;
  meshLevel: number;
  validRtc: boolean;
  latencyMs: number | null;
  runtimeErrors: string[];
  sensorEntries: Array<SensorReading & { sensorName: string; port: number }>;
  portCount: number;
};

export type ServerNetwork = {
  adapter?: string;
  ip?: string;
  mac?: string;
};

export type HistoryIpRow = {
  ip: string;
  lastSeen?: string;
};

export type HistorySeriesItem = {
  ip: string;
  sensorName: string;
  sensorType?: number;
  port?: number;
  field: string;
  time: string;
  nodeTime?: string | null;
  value: number;
  nodeTimeValid?: boolean;
};

export type FirmwareMeta = {
  object: 'Gateway' | 'Node';
  version: string;
  size: number;
  uploadedAt?: string;
};
