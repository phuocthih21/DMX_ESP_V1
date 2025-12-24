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
  setInfo: (payload) => set((state) => ({
    // Logic: Nếu có payload mới, gộp nó vào state cũ.
    // Nếu payload là null (reset), thì gán là null.
    info: payload 
      ? { ...state.info, ...payload } 
      : null,
    error: null 
  })),
  setLoading: (loading) => set({ loading }),
  setError: (error) => set({ error, loading: false }),
  reset: () => set(initialState),
}));

