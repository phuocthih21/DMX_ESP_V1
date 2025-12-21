/**
 * DMX System Constants
 */

// DMX Universe limits
export const DMX_UNIVERSE_MIN = 1;
export const DMX_UNIVERSE_MAX = 32768;
export const DMX_CHANNELS_PER_UNIVERSE = 512;

// DMX Timing limits (microseconds)
export const DMX_BREAK_MIN = 88;
export const DMX_BREAK_MAX = 500;
export const DMX_BREAK_DEFAULT = 176;

export const DMX_MAB_MIN = 8;
export const DMX_MAB_MAX = 100;
export const DMX_MAB_DEFAULT = 12;

// DMX Ports
export const DMX_PORT_COUNT = 4;
export const DMX_PORTS = ['A', 'B', 'C', 'D'] as const;

// Default device IP (AP mode)
export const DEFAULT_DEVICE_IP = '192.168.4.1';

// Polling intervals (milliseconds)
export const POLL_INTERVAL_SYSTEM = 1000; // 1s
export const POLL_INTERVAL_DMX = 500; // 500ms
export const POLL_INTERVAL_NETWORK = 2000; // 2s

// WebSocket reconnect settings
export const WS_RECONNECT_DELAY_INITIAL = 1000; // 1s
export const WS_RECONNECT_DELAY_MAX = 10000; // 10s
export const WS_RECONNECT_BACKOFF_MULTIPLIER = 1.5;

// API timeout
export const API_TIMEOUT = 5000; // 5s

