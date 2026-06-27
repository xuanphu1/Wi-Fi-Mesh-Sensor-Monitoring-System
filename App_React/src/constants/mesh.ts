export const NODE_OFFLINE_MS = 7000;
export const GATEWAY_OFFLINE_MS = 10000;
export const WS_RETRY_MS = 3000;
export const DEFAULT_RTC_ISO = '1970-01-01T00:00:00';

export const SENSOR_TYPE_NAMES: Record<number, string> = {
  [-1]: 'None',
  0: 'BME280',
  1: 'MH-Z14A',
  2: 'PMS7003',
  3: 'DHT22',
  4: 'MQ-2',
  5: 'MQ-3',
  6: 'MQ-4',
  7: 'MQ-5',
  8: 'MQ-6',
  9: 'MQ-7',
  10: 'MQ-8',
  11: 'MQ-9',
  12: 'MQ-135',
  13: 'AHT10',
};

export const SENSOR_FIELD_META: Record<number, Array<{ key: string; label: string; unit: string }>> = {
  0: [
    { key: 'temperature', label: 'Temperature', unit: 'C' },
    { key: 'humidity', label: 'Humidity', unit: '%' },
    { key: 'pressure', label: 'Pressure', unit: 'hPa' },
  ],
  1: [{ key: 'co2', label: 'CO2', unit: 'ppm' }],
  2: [
    { key: 'pm1_0', label: 'PM1.0', unit: 'ug/m3' },
    { key: 'pm2_5', label: 'PM2.5', unit: 'ug/m3' },
    { key: 'pm10', label: 'PM10', unit: 'ug/m3' },
  ],
  3: [
    { key: 'temperature', label: 'Temperature', unit: 'C' },
    { key: 'humidity', label: 'Humidity', unit: '%' },
  ],
  13: [
    { key: 'temperature', label: 'Temperature', unit: 'C' },
    { key: 'humidity', label: 'Humidity', unit: '%' },
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
