/**
 * Device IP/connection management store
 */

import { create } from 'zustand';
import { getDeviceIP, setDeviceIP as saveDeviceIP } from '../utils/deviceDiscovery';

interface DeviceState {
  deviceIP: string;
  setDeviceIP: (ip: string) => void;
  isSameOrigin: boolean;
}

const initialState = {
  deviceIP: getDeviceIP(),
  isSameOrigin: import.meta.env.PROD,
};

export const useDeviceStore = create<DeviceState>((set) => ({
  ...initialState,
  setDeviceIP: (ip) => {
    saveDeviceIP(ip);
    set({ deviceIP: ip });
  },
}));

