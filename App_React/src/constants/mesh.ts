export const NODE_OFFLINE_MS = 7000;
export const GATEWAY_OFFLINE_MS = 10000;
export const WS_RETRY_MS = 3000;
export const DEFAULT_RTC_ISO = '1970-01-01T00:00:00';

export const SENSOR_TYPE_NAMES: Record<number, string> = {
  [-1]: 'SENSOR_NONE',
  0: 'SENSOR_BME280',
  1: 'SENSOR_MHZ14A',
  2: 'SENSOR_PMS7003',
  3: 'SENSOR_DHT22',
  4: 'SENSOR_AHT10',
  5: 'SENSOR_DHT11',
  6: 'SENSOR_HTU21D',
};

export const SENSOR_FIELD_META: Record<number, Array<{ key: string; label: string; unit: string }>> = {
  0: [
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
    { key: 'pressure_hPa', label: 'Pressure', unit: 'hPa' },
    { key: 'humidity_RH', label: 'Humidity', unit: '%' },
  ],
  1: [
    { key: 'co2_ppm', label: 'CO2', unit: 'ppm' },
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
  ],
  2: [
    { key: 'pm1_0_ugm3', label: 'PM1.0', unit: 'ug/m3' },
    { key: 'pm2_5_ugm3', label: 'PM2.5', unit: 'ug/m3' },
    { key: 'pm10_ugm3', label: 'PM10', unit: 'ug/m3' },
  ],
  3: [
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
    { key: 'humidity_RH', label: 'Humidity', unit: '%' },
  ],
  4: [
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
    { key: 'humidity_RH', label: 'Humidity', unit: '%' },
  ],
  5: [
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
    { key: 'humidity_RH', label: 'Humidity', unit: '%' },
  ],
  6: [
    { key: 'temp_C', label: 'Temperature', unit: '°C' },
    { key: 'humidity_RH', label: 'Humidity', unit: '%' },
  ],
};

export const ERROR_CODE_DEFS: Record<string, string> = {
  '0x5C': 'I2C timeout',
  '0xCD': 'Invalid sensor data',
  '0x74': 'WiFi reconnect required',
  '0xDB': 'Mesh frame decode failed',
  '0x54': 'I2C communication warning',
  '0xBA': 'Sensor read unstable',
  '0x26': 'Sensor not initialized',
  '0x08': 'Out of range reading',
  '0xEF': 'RTC invalid',
  '0x34': 'Network jitter warning',
  '0xA1': 'Driver transient fault',
};

export function getSensorTypeByName(name: string): number {
  if (name === 'System') return -2;
  if (name === 'Gateway') return -3;
  
  const normalizedName = name.replace('SENSOR_', '').replace(/-/g, '').toLowerCase();
  const entry = Object.entries(SENSOR_TYPE_NAMES).find(([_, n]) => 
    n.replace('SENSOR_', '').replace(/-/g, '').toLowerCase() === normalizedName
  );
  
  return entry ? Number(entry[0]) : -1;
}

export function getFieldsForSensorType(type: number): Array<{ key: string; label: string; unit: string }> {
  if (type === -2) {
    return [{ key: 'packetloss', label: 'Packet Loss', unit: '%' }];
  }
  if (type === -3) {
    return [
      { key: 'cpu_load', label: 'CPU Load', unit: '%' },
      { key: 'ram_used', label: 'RAM Used', unit: '%' },
      { key: 'wifi_rssi', label: 'Wi-Fi RSSI', unit: 'dBm' },
      { key: 'battery_v', label: 'Battery Voltage', unit: 'V' },
      { key: 'uptime_s', label: 'Uptime', unit: 's' },
    ];
  }
  return SENSOR_FIELD_META[type] || [];
}
