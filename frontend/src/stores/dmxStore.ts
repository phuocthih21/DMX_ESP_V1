/**
 * DMX state store
 */

import { create } from 'zustand';
import { DMXPortStatus } from '../api/types';

interface DMXState {
  ports: DMXPortStatus[];
  lastUpdate: number;
  loading: boolean;
  error: string | null;
  setPorts: (ports: DMXPortStatus[]) => void;
  updatePort: (port: number, status: Partial<DMXPortStatus>) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string | null) => void;
  reset: () => void;
}

const initialState = {
  ports: [],
  lastUpdate: 0,
  loading: false,
  error: null,
};

export const useDMXStore = create<DMXState>((set) => ({
  ...initialState,
  setPorts: (ports) => set({ 
    ports, 
    lastUpdate: Date.now(),
    error: null 
  }),
  updatePort: (port, status) => set((state) => ({
    ports: state.ports.map((p) => 
      p.port === port ? { ...p, ...status } : p
    ),
    lastUpdate: Date.now(),
  })),
  setLoading: (loading) => set({ loading }),
  setError: (error) => set({ error, loading: false }),
  reset: () => set(initialState),
}));

