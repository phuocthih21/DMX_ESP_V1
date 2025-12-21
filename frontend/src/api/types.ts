/**
 * TypeScript types matching backend API contracts
 */

/**
 * Standard API response wrapper
 */
export interface ApiResponse<T> {
  ok: boolean;
  data: T;
  error: string | null;
}

/**
 * System Information
 */
export interface SystemInfo {
  device_id?: string;
  firmware_version?: string;
  uptime: number; // seconds
  cpu_load?: number; // percentage
  free_heap?: number; // bytes
  eth_up?: boolean;
  wifi_up?: boolean;
}

/**
 * DMX Port Status
 */
export interface DMXPortStatus {
  port: number; // 0-3
  universe: number; // 1-32768
  enabled: boolean;
  fps?: number; // frames per second
  backend?: 'RMT' | 'UART';
  activity_counter?: number;
}

/**
 * DMX Port Configuration
 */
export interface DMXPortConfig {
  port: number;
  universe: number;
  enabled: boolean;
  break_us?: number; // microseconds (88-500)
  mab_us?: number; // microseconds (8-100)
  protocol?: 'RMT' | 'UART';
}

/**
 * DMX Configuration (all ports)
 */
export interface DMXConfig {
  ports: DMXPortConfig[];
}

/**
 * Network Status
 */
export interface NetworkStatus {
  eth_up: boolean;
  wifi_up: boolean;
  eth_ip?: string | null;
  wifi_ip?: string | null;
  eth_mac?: string;
  wifi_mac?: string;
  wifi_ssid?: string | null;
  wifi_rssi?: number | null;
}

/**
 * Ethernet Configuration
 */
export interface EthernetConfig {
  enabled: boolean;
  ip?: string;
  netmask?: string;
  gateway?: string;
  dhcp?: boolean;
}

/**
 * Wi-Fi Station Configuration
 */
export interface WifiStationConfig {
  enabled: boolean;
  ssid: string;
  password: string;
}

/**
 * Wi-Fi AP Configuration
 */
export interface WifiAPConfig {
  enabled: boolean;
  ssid: string;
  password: string;
  channel?: number;
}

/**
 * Network Configuration
 */
export interface NetworkConfig {
  ethernet?: EthernetConfig;
  wifi_sta?: WifiStationConfig;
  wifi_ap?: WifiAPConfig;
}

/**
 * Wi-Fi Network Scan Result
 */
export interface WifiNetwork {
  ssid: string;
  rssi: number;
  auth_mode: number; // 0=Open, 1=WEP, 2=WPA_PSK, 3=WPA2_PSK, 4=WPA_WPA2_PSK
  channel?: number;
}

/**
 * WebSocket Message Envelope
 */
export interface WSMessage {
  type: string;
  ts: number; // timestamp (ms from boot)
  data: unknown;
}

/**
 * WebSocket System Status Message
 */
export interface WSSystemStatus {
  type: 'system.status';
  ts: number;
  data: {
    cpu: number;
    heap: number;
    uptime: number;
  };
}

/**
 * WebSocket DMX Port Status Message
 */
export interface WSDMXPortStatus {
  type: 'dmx.port_status';
  ts: number;
  data: {
    port: number;
    universe: number;
    enabled: boolean;
    fps: number;
  };
}

/**
 * WebSocket Network Link Message
 */
export interface WSNetworkLink {
  type: 'network.link';
  ts: number;
  data: {
    iface: 'eth' | 'wifi';
    status: 'up' | 'down';
  };
}

/**
 * WebSocket System Event Message
 */
export interface WSSystemEvent {
  type: 'system.event';
  ts: number;
  data: {
    code: string;
    level: 'info' | 'warn' | 'error';
  };
}

