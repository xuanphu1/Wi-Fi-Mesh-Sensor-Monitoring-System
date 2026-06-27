import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import * as SecureStore from 'expo-secure-store';

export interface ThresholdConfig {
  enabled: boolean;
  warnMin: string;
  warnMax: string;
  critMin: string;
  critMax: string;
  notify: boolean;
}

interface ThresholdState {
  // Key format: `${nodeIp}_${sensorName}_${field}` -> e.g. "192.168.1.101_BME280_Temperature"
  thresholds: Record<string, ThresholdConfig>;
  setThreshold: (key: string, config: ThresholdConfig) => void;
  getThreshold: (key: string) => ThresholdConfig;
}

const secureStorage = {
  getItem: async (name: string) => {
    return (await SecureStore.getItemAsync(name)) || null;
  },
  setItem: async (name: string, value: string) => {
    await SecureStore.setItemAsync(name, value);
  },
  removeItem: async (name: string) => {
    await SecureStore.deleteItemAsync(name);
  },
};

const DEFAULT_CONFIG: ThresholdConfig = {
  enabled: false,
  warnMin: '18.0',
  warnMax: '32.0',
  critMin: '10.0',
  critMax: '40.0',
  notify: false,
};

export const useThresholdStore = create<ThresholdState>()(
  persist(
    (set, get) => ({
      thresholds: {},
      setThreshold: (key, config) => 
        set((state) => ({
          thresholds: {
            ...state.thresholds,
            [key]: config
          }
        })),
      getThreshold: (key) => {
        return get().thresholds[key] || DEFAULT_CONFIG;
      }
    }),
    {
      name: 'sensor-threshold-storage',
      storage: createJSONStorage(() => secureStorage),
    }
  )
);
