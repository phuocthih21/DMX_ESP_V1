/**
 * API endpoint constants
 */

// Backend currently exposes APIs at `/api` (no version). If the backend later adds a versioned prefix,
// update this constant accordingly.
const API_PREFIX = `/api`;

export const API_ENDPOINTS = {
  // System endpoints
  SYSTEM_INFO: `${API_PREFIX}/system/info`,
  SYSTEM_REBOOT: `${API_PREFIX}/system/reboot`,
  SYSTEM_FACTORY: `${API_PREFIX}/system/factory`,
  SYSTEM_OTA: `${API_PREFIX}/system/ota`,

  // DMX endpoints
  DMX_STATUS: `${API_PREFIX}/dmx/status`,
  DMX_CONFIG: `${API_PREFIX}/dmx/config`,

  // Network endpoints
  NETWORK_STATUS: `${API_PREFIX}/network/status`,
  NETWORK_CONFIG: `${API_PREFIX}/network/config`,

  // File endpoints (optional)
  FILE_EXPORT: `${API_PREFIX}/file/export`,
  FILE_IMPORT: `${API_PREFIX}/file/import`,
} as const;

