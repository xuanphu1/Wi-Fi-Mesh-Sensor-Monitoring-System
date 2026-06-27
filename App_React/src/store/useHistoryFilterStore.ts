import { create } from 'zustand';
import { persist, createJSONStorage } from 'zustand/middleware';
import * as SecureStore from 'expo-secure-store';

interface HistoryFilterState {
  selectedIp: string;
  selectedSensor: string;
  setFilters: (ip: string, sensor: string) => void;
  setSelectedIp: (ip: string) => void;
  setSelectedSensor: (sensor: string) => void;
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
      selectedIp: '',
      selectedSensor: '',
      setFilters: (ip, sensor) => set({ selectedIp: ip, selectedSensor: sensor }),
      setSelectedIp: (ip) => set({ selectedIp: ip }),
      setSelectedSensor: (sensor) => set({ selectedSensor: sensor }),
    }),
    {
      name: 'history-filter-storage',
      storage: createJSONStorage(() => secureStorage),
    }
  )
);
