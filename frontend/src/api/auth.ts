import { apiClient, setAuthToken } from './client';
import { API_ENDPOINTS } from './endpoints';

export async function login(password: string): Promise<{ token: string; expires_seconds: number }> {
  const resp = await apiClient.post(API_ENDPOINTS.AUTH_LOGIN, { password });
  const payload = resp.data as { token: string; expires_seconds: number };
  // Store token for subsequent calls
  setAuthToken(payload.token);
  return payload;
}

export async function setPassword(password: string): Promise<void> {
  await apiClient.post(API_ENDPOINTS.AUTH_SET_PASSWORD, { password });
}