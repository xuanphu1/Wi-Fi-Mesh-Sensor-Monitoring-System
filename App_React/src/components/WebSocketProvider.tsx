import React, { createContext, useEffect, useRef } from 'react';
import { useMeshStore } from '../store/useMeshStore';
import { parseUartRx } from '../services/meshParser';
import { Platform, Alert } from 'react-native';
import Constants from 'expo-constants';
import { useThresholdStore } from '../store/useThresholdStore';
import { useToastStore } from '../store/useToastStore';

const WebSocketContext = createContext<WebSocket | null>(null);

export const WebSocketProvider: React.FC<{ children: React.ReactNode }> = ({ children }) => {
  const wsRef = useRef<WebSocket | null>(null);
  const lastAlerted = useRef<Record<string, number>>({});
  const setWsStatus = useMeshStore((s) => s.setWsStatus);
  const ingestPacket = useMeshStore((s) => s.ingestPacket);

  useEffect(() => {
    let reconnectTimeout: NodeJS.Timeout;

    const connect = () => {
      setWsStatus('Connecting');
      
      // Use Expo Constants to automatically get the PC's local LAN IP (e.g. 192.168.x.x)
      const debuggerHost = Constants.expoConfig?.hostUri;
      let host = '127.0.0.1';
      
      if (debuggerHost) {
        host = debuggerHost.split(':')[0];
      } else if (Platform.OS === 'android') {
        host = '10.0.2.2'; // Fallback for Android Emulator
      }
      
      const wsUrl = `ws://${host}:8765`;
      
      console.log(`Connecting to WebSocket at ${wsUrl}`);
      const ws = new WebSocket(wsUrl);
      wsRef.current = ws;

      ws.onopen = () => {
        console.log('WebSocket Connected');
        setWsStatus('Connected');
      };

      ws.onmessage = (event) => {
        try {
          const data = JSON.parse(event.data);
          
          if (data.type === 'uart_rx' && data.payload) {
            const parsed = parseUartRx(data.payload);
            if (parsed) {
              useMeshStore.getState().ingestPacket(parsed, data.len || 0);

              // Check thresholds and alert
              const thresholds = useThresholdStore.getState().thresholds;
              parsed.ports.forEach(port => {
                port.readings.forEach(reading => {
                  const key = `${parsed.staIpv4}_${port.sensorName}_${reading.label}`;
                  const th = thresholds[key];
                  
                  if (th && th.enabled && th.notify) {
                    const val = reading.value;
                    const cMin = parseFloat(th.critMin);
                    const cMax = parseFloat(th.critMax);
                    
                    console.log(`[Alert Check] ${key}: val=${val}, cMin=${cMin}, cMax=${cMax}`);
                    
                    if (val < cMin || val > cMax) {
                      const now = Date.now();
                      const lastAlert = lastAlerted.current[key] || 0;
                      console.log(`[Alert Trigger] Out of bounds! lastAlert was ${now - lastAlert}ms ago.`);
                      
                      if (now - lastAlert > 10000) { // Reduced to 10s for easier testing
                        console.log(`[Alert Display] Showing Dropdown Toast!`);
                        useToastStore.getState().showToast(
                          'Critical Threshold Exceeded',
                          `Node: ${parsed.staIpv4}\nSensor: ${port.sensorName} - ${reading.label}\nValue: ${val}${reading.unit} is outside limits [${cMin}, ${cMax}]`,
                          'error'
                        );
                        lastAlerted.current[key] = now;
                      }
                    }
                  }
                });
              });
            }
          } else if (data.type === 'gateway_status') {
            useMeshStore.getState().ingestGateway(data);
          }
        } catch (e) {
          console.warn("WebSocket parse error", e);
        }
      };

      ws.onclose = () => {
        console.log('WebSocket Disconnected. Reconnecting in 3s...');
        setWsStatus('Disconnected');
        reconnectTimeout = setTimeout(connect, 3000);
      };

      ws.onerror = (e) => {
        console.warn('WebSocket Error', e);
        ws.close(); // Force close to trigger onclose and reconnect
      };
    };

    connect();

    return () => {
      clearTimeout(reconnectTimeout);
      if (wsRef.current) {
        wsRef.current.close();
      }
    };
  }, [ingestPacket, setWsStatus]);

  return (
    <WebSocketContext.Provider value={wsRef.current}>
      {children}
    </WebSocketContext.Provider>
  );
};
