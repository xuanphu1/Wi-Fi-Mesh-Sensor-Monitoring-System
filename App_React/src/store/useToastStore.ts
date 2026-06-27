import { create } from 'zustand';

export type ToastType = 'info' | 'success' | 'warning' | 'error';

interface ToastState {
  visible: boolean;
  title: string;
  message: string;
  type: ToastType;
  showToast: (title: string, message: string, type?: ToastType) => void;
  hideToast: () => void;
}

export const useToastStore = create<ToastState>((set) => ({
  visible: false,
  title: '',
  message: '',
  type: 'info',
  showToast: (title, message, type = 'info') => set({ visible: true, title, message, type }),
  hideToast: () => set({ visible: false }),
}));
