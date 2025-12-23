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
  setPorts: (ports) => {
    // Coerce input to an array to be defensive against backend/WS variations
    const normalized = Array.isArray(ports)
      ? ports
      : (ports && Array.isArray((ports as any).ports) ? (ports as any).ports : []);
    return set({
      ports: normalized as DMXPortStatus[],
      lastUpdate: Date.now(),
      error: null,
    });
  },
  updatePort: (port, status) => set((state) => {
    const portsArray = Array.isArray(state.ports) ? state.ports.slice() : [];
    const idx = portsArray.findIndex((p) => p.port === port);
    if (idx >= 0) {
      portsArray[idx] = { ...portsArray[idx], ...status };
    } else {
      // Insert a new port entry if missing
      portsArray.push({ port, universe: 0, enabled: false, ...(status as any) } as DMXPortStatus);
    }
    return {
      ports: portsArray,
      lastUpdate: Date.now(),
    };
  }),
  setLoading: (loading) => set({ loading }),
  setError: (error) => set({ error, loading: false }),
  reset: () => set(initialState),
}));

