/**
 * System state store
 */

import { create } from 'zustand';
import { SystemInfo } from '../api/types';

interface SystemState {
  info: SystemInfo | null;
  loading: boolean;
  error: string | null;
  setInfo: (info: SystemInfo | null) => void;
  setLoading: (loading: boolean) => void;
  setError: (error: string | null) => void;
  reset: () => void;
}

const initialState = {
  info: null,
  loading: false,
  error: null,
};

export const useSystemStore = create<SystemState>((set) => ({
  ...initialState,
  setInfo: (info) => set({ info, error: null }),
  setLoading: (loading) => set({ loading }),
  setError: (error) => set({ error, loading: false }),
  reset: () => set(initialState),
}));

