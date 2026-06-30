import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import * as SecureStore from 'expo-secure-store';

interface HistoryFilterState {
  selectedMac: string;
  selectedSensorName: string;
  selectedField: string;
  timeRange: '1h' | '24h' | '7d' | 'custom';
  customStartTime: number | null;
  setFilters: (mac: string, sensorName: string, field: string) => void;
  setSelectedMac: (mac: string) => void;
  setSelectedSensorName: (sensorName: string) => void;
  setSelectedField: (field: string) => void;
  setTimeRange: (range: '1h' | '24h' | '7d' | 'custom') => void;
  setCustomStartTime: (time: number | null) => void;
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

export const useHistoryFilterStore = create<HistoryFilterState>()(
  persist(
    (set) => ({
      selectedMac: '',
      selectedSensorName: '',
      selectedField: '',
      timeRange: '24h',
      customStartTime: null,
      setFilters: (mac, sensorName, field) => set({ selectedMac: mac, selectedSensorName: sensorName, selectedField: field }),
      setSelectedMac: (mac) => set({ selectedMac: mac }),
      setSelectedSensorName: (sensorName) => set({ selectedSensorName: sensorName }),
      setSelectedField: (field) => set({ selectedField: field }),
      setTimeRange: (range) => set({ timeRange: range }),
      setCustomStartTime: (time) => set({ customStartTime: time }),
    }),
    {
      name: 'history-filter-storage',
      storage: createJSONStorage(() => secureStorage),
    }
  )
);
