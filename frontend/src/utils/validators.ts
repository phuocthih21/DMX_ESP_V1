/**
 * Input validation functions
 */

/**
 * Validate DMX universe number (1-32768)
 */
export function validateUniverse(universe: number): boolean {
  return Number.isInteger(universe) && universe >= 1 && universe <= 32768;
}

/**
 * Validate IPv4 address format
 */
export function validateIPv4(ip: string): boolean {
  const parts = ip.split('.');
  if (parts.length !== 4) return false;
  
  return parts.every(part => {
    const num = parseInt(part, 10);
    return !isNaN(num) && num >= 0 && num <= 255;
  });
}

/**
 * Validate DMX break timing (88-500 microseconds)
 */
export function validateBreakTiming(breakUs: number): boolean {
  return Number.isInteger(breakUs) && breakUs >= 88 && breakUs <= 500;
}

/**
 * Validate DMX MAB timing (8-100 microseconds)
 */
export function validateMABTiming(mabUs: number): boolean {
  return Number.isInteger(mabUs) && mabUs >= 8 && mabUs <= 100;
}

/**
 * Validate netmask format (IPv4)
 */
export function validateNetmask(netmask: string): boolean {
  return validateIPv4(netmask);
}

/**
 * Validate port number (0-3 for DMX ports)
 */
export function validatePort(port: number): boolean {
  return Number.isInteger(port) && port >= 0 && port < 4;
}

/**
 * Validate SSID (Wi-Fi network name)
 */
export function validateSSID(ssid: string): boolean {
  return ssid.length > 0 && ssid.length <= 32;
}

/**
 * Validate Wi-Fi password
 */
export function validateWifiPassword(password: string): boolean {
  return password.length >= 8 && password.length <= 63;
}

