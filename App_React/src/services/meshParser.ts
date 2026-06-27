import { ParsedMeshPacket, MeshPort, SensorReading } from '../types/mesh';

export function parseUartRx(payloadStr: string): ParsedMeshPacket | null {
  try {
    const data = JSON.parse(payloadStr);
    
    // Parse ports
    const ports: MeshPort[] = [];
    if (Array.isArray(data.p)) {
      data.p.forEach((portData: number[]) => {
        if (portData.length >= 2) {
          const wirePort = portData[0];
          const sensorType = portData[1];
          const readings: SensorReading[] = [];
          let sensorName = 'Unknown';

          // BME280: [port, type, temp, hum, press]
          if (sensorType === 0 && portData.length >= 5) {
            sensorName = 'BME280';
            readings.push({ index: 0, key: 'temp_c', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'hum_rh', label: 'Humidity', unit: '%', value: portData[3] });
            readings.push({ index: 2, key: 'press_hpa', label: 'Pressure', unit: 'hPa', value: portData[4] });
          }
          // AHT10/AHT20: [port, type, temp, hum]
          else if (sensorType === 13 && portData.length >= 4) {
            sensorName = 'AHT10'; // Or AHT20
            readings.push({ index: 0, key: 'temp_c', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'hum_rh', label: 'Humidity', unit: '%', value: portData[3] });
          }

          ports.push({
            wirePort,
            sensorType,
            sensorName,
            readings
          });
        }
      });
    }

    return {
      schemaVersion: data.v || 0,
      meshLevel: data.n || 0,
      staIpv4: data.i || '0.0.0.0',
      rtcIso: data.t || new Date().toISOString(),
      firmwareVersion: data.ver || '0.0.0',
      runtimeErrors: Array.isArray(data.err) ? data.err : [],
      ports,
      raw: data
    };
  } catch (e) {
    console.warn("Failed to parse uart_rx payload", e);
    return null;
  }
}
