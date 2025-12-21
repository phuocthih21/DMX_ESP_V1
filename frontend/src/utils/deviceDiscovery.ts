/**
 * Device discovery utilities
 * Supports mDNS browser API (optional) and manual IP entry
 */

const DEVICE_IP_STORAGE_KEY = 'dmx_node_device_ip';

/**
 * Get device IP from localStorage or use default
 */
export function getDeviceIP(): string {
  if (typeof window === 'undefined') {
    return '';
  }

  // In production (served from device), use same-origin
  if (import.meta.env.PROD) {
    return '';
  }

  // In development, use stored IP or default to mock server
  const stored = localStorage.getItem(DEVICE_IP_STORAGE_KEY);
  return stored || 'http://localhost:3000';
}

/**
 * Set device IP in localStorage
 */
export function setDeviceIP(ip: string): void {
  if (typeof window === 'undefined') return;
  
  // Remove protocol if present
  const cleanIP = ip.replace(/^https?:\/\//, '');
  
  // Add http:// if no protocol
  const fullURL = cleanIP.startsWith('http') ? cleanIP : `http://${cleanIP}`;
  
  localStorage.setItem(DEVICE_IP_STORAGE_KEY, fullURL);
}

/**
 * Get base URL for API calls
 */
export function getBaseURL(): string {
  if (import.meta.env.PROD) {
    // Production: same-origin (served from device)
    return '';
  }
  
  // Development: use configured device IP
  const deviceIP = getDeviceIP();
  // Ensure it has protocol
  if (deviceIP && !deviceIP.startsWith('http://') && !deviceIP.startsWith('https://')) {
    return `http://${deviceIP}`;
  }
  return deviceIP;
}

/**
 * Build WebSocket URL
 */
export function getWebSocketURL(): string {
  const baseURL = getBaseURL();
  
  if (!baseURL) {
    // Same-origin, use current host
    const protocol = window.location.protocol === 'https:' ? 'wss:' : 'ws:';
    return `${protocol}//${window.location.host}/ws/status`;
  }
  
  // Use configured device IP
  const wsProtocol = baseURL.startsWith('https') ? 'wss:' : 'ws:';
  const host = baseURL.replace(/^https?:\/\//, '');
  return `${wsProtocol}//${host}/ws/status`;
}

/**
 * Try to discover device via mDNS (browser API)
 * Note: This requires browser support and may not work in all environments
 */
export async function discoverDeviceMDNS(): Promise<string | null> {
  // Check if mDNS API is available
  if (typeof window === 'undefined' || !('serviceWorker' in navigator)) {
    return null;
  }

  // For now, return null - mDNS discovery can be implemented later
  // if browser support becomes available
  return null;
}

