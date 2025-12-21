/**
 * Connection state store (online/offline, WebSocket status)
 */

import { create } from 'zustand';

interface ConnectionState {
  online: boolean;
  wsConnected: boolean;
  setOnline: (online: boolean) => void;
  setWSConnected: (connected: boolean) => void;
}

export const useConnectionStore = create<ConnectionState>((set) => ({
  online: typeof navigator !== 'undefined' ? navigator.onLine : true,
  wsConnected: false,
  setOnline: (online) => set({ online }),
  setWSConnected: (wsConnected) => set({ wsConnected }),
}));

// Listen to browser online/offline events
if (typeof window !== 'undefined') {
  window.addEventListener('online', () => {
    useConnectionStore.getState().setOnline(true);
  });
  
  window.addEventListener('offline', () => {
    useConnectionStore.getState().setOnline(false);
  });
}

