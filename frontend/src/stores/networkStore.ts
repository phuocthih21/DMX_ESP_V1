/**
 * Network state store
 */

import { create } from 'zustand';
import { NetworkStatus, WifiNetwork } from '../api/types';

interface NetworkState {
  status: NetworkStatus | null;
  scanning: boolean;
  wifiNetworks: WifiNetwork[];
  error: string | null;
  setStatus: (status: NetworkStatus | null) => void;
  setScanning: (scanning: boolean) => void;
  setWifiNetworks: (networks: WifiNetwork[]) => void;
  setError: (error: string | null) => void;
  reset: () => void;
}

const initialState = {
  status: null,
  scanning: false,
  wifiNetworks: [],
  error: null,
};

export const useNetworkStore = create<NetworkState>((set) => ({
  ...initialState,
  setStatus: (status) => set({ status, error: null }),
  setScanning: (scanning) => set({ scanning }),
  setWifiNetworks: (networks) => set({ wifiNetworks: networks }),
  setError: (error) => set({ error }),
  reset: () => set(initialState),
}));

