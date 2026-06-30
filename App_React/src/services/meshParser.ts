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

          // 0: BME280 -> Temperature, Pressure, Humidity
          if (sensorType === 0 && portData.length >= 5) {
            sensorName = 'BME280';
            readings.push({ index: 0, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'pressure_hPa', label: 'Pressure', unit: 'hPa', value: portData[3] });
            readings.push({ index: 2, key: 'humidity_RH', label: 'Humidity', unit: '%', value: portData[4] });
          }
          // 1: MHZ14A -> CO2, Temperature
          else if (sensorType === 1 && portData.length >= 4) {
            sensorName = 'MH-Z14A';
            readings.push({ index: 0, key: 'co2_ppm', label: 'CO2', unit: 'ppm', value: portData[2] });
            readings.push({ index: 1, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[3] });
          }
          // 2: PMS7003 -> PM1.0, PM2.5, PM10
          else if (sensorType === 2 && portData.length >= 5) {
            sensorName = 'PMS7003';
            readings.push({ index: 0, key: 'pm1_0_ugm3', label: 'PM1.0', unit: 'ug/m3', value: portData[2] });
            readings.push({ index: 1, key: 'pm2_5_ugm3', label: 'PM2.5', unit: 'ug/m3', value: portData[3] });
            readings.push({ index: 2, key: 'pm10_ugm3', label: 'PM10', unit: 'ug/m3', value: portData[4] });
          }
          // 3: DHT22 -> Temperature, Humidity
          else if (sensorType === 3 && portData.length >= 4) {
            sensorName = 'DHT22';
            readings.push({ index: 0, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'humidity_RH', label: 'Humidity', unit: '%', value: portData[3] });
          }
          // 4: AHT10 -> Temperature, Humidity
          else if (sensorType === 4 && portData.length >= 4) {
            sensorName = 'AHT10';
            readings.push({ index: 0, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'humidity_RH', label: 'Humidity', unit: '%', value: portData[3] });
          }
          // 5: DHT11 -> Temperature, Humidity
          else if (sensorType === 5 && portData.length >= 4) {
            sensorName = 'DHT11';
            readings.push({ index: 0, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'humidity_RH', label: 'Humidity', unit: '%', value: portData[3] });
          }
          // 6: HTU21D -> Temperature, Humidity
          else if (sensorType === 6 && portData.length >= 4) {
            sensorName = 'HTU21D';
            readings.push({ index: 0, key: 'temp_C', label: 'Temperature', unit: '°C', value: portData[2] });
            readings.push({ index: 1, key: 'humidity_RH', label: 'Humidity', unit: '%', value: portData[3] });
          }
          // Fallback for unknown sensor
          else {
            sensorName = `Sensor_T${sensorType}`;
            for (let i = 2; i < portData.length; i++) {
              readings.push({ index: i - 2, key: `val_${i}`, label: `Value ${i - 1}`, unit: '', value: portData[i] });
            }
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
      mac: data.M || data.i || '00:00:00:00:00:00',
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
