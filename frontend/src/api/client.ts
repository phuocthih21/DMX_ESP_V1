/**
 * Axios API client with error handling and dynamic base URL
 */

import axios, { AxiosInstance, AxiosError, AxiosResponse } from 'axios';
import { ApiResponse } from './types';
import { getBaseURL } from '../utils/deviceDiscovery';
import { API_TIMEOUT } from '../utils/constants';

/**
 * Create axios instance with base configuration
 */
function createAxiosInstance(): AxiosInstance {
  const instance = axios.create({
    baseURL: getBaseURL(),
    timeout: API_TIMEOUT,
    headers: {
      'Content-Type': 'application/json',
    },
  });

  // Request interceptor - update base URL if device IP changes
  instance.interceptors.request.use((config) => {
    const baseURL = getBaseURL();
    if (baseURL !== config.baseURL) {
      config.baseURL = baseURL;
    }
    return config;
  });

  // Response interceptor - handle errors and unwrap ApiResponse
  instance.interceptors.response.use(
    (response: AxiosResponse<ApiResponse<unknown>>) => {
      // Unwrap ApiResponse if present
      if (response.data && typeof response.data === 'object' && 'ok' in response.data) {
        const apiResponse = response.data as ApiResponse<unknown>;
        if (!apiResponse.ok && apiResponse.error) {
          throw new Error(apiResponse.error);
        }
        // Return unwrapped data
        return { ...response, data: apiResponse.data };
      }
      return response;
    },
    (error: AxiosError) => {
      // Handle network errors
      if (error.code === 'ECONNABORTED') {
        throw new Error('Request timeout - device may be unreachable');
      }
      if (error.code === 'ERR_NETWORK') {
        throw new Error('Network error - check device connection');
      }
      if (error.response) {
        // Server responded with error status
        const status = error.response.status;
        const responseData = error.response.data as { error?: string } | string | null;
        const errorMessage = typeof responseData === 'object' && responseData !== null && 'error' in responseData
          ? responseData.error
          : typeof responseData === 'string'
          ? responseData
          : null;
        const message = errorMessage || error.message || `HTTP ${status}`;
        throw new Error(message);
      }
      throw error;
    }
  );

  return instance;
}

// Export singleton instance
export const apiClient = createAxiosInstance();

/**
 * Set or clear Authorization Bearer token for subsequent requests
 */
export function setAuthToken(token: string | null): void {
  if (token) {
    apiClient.defaults.headers.common['Authorization'] = `Bearer ${token}`;
    localStorage.setItem('dmx_token', token);
  } else {
    delete apiClient.defaults.headers.common['Authorization'];
    localStorage.removeItem('dmx_token');
  }
}

/**
 * Load token from localStorage and apply to client (call on app start)
 */
export function loadStoredAuthToken(): void {
  const token = localStorage.getItem('dmx_token');
  if (token) setAuthToken(token);
}

/**
 * Update base URL (for device IP changes)
 */
export function updateBaseURL(): void {
  const baseURL = getBaseURL();
  apiClient.defaults.baseURL = baseURL;
}

