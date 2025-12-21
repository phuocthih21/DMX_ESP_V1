/**
 * Main dashboard page
 */

import React from 'react';
import { SystemMetrics } from '../components/dashboard/SystemMetrics';
import { DMXActivity } from '../components/dashboard/DMXActivity';
import { NetworkStatus } from '../components/dashboard/NetworkStatus';
import { usePolling } from '../hooks/usePolling';
import { apiClient } from '../api/client';
import { API_ENDPOINTS } from '../api/endpoints';
import { useSystemStore } from '../stores/systemStore';
import { useDMXStore } from '../stores/dmxStore';
import { useNetworkStore } from '../stores/networkStore';
import { SystemInfo, DMXPortStatus, NetworkStatus as NetworkStatusType } from '../api/types';
import { POLL_INTERVAL_SYSTEM, POLL_INTERVAL_DMX, POLL_INTERVAL_NETWORK } from '../utils/constants';

export const Dashboard: React.FC = () => {
  const { setInfo, setLoading: setSystemLoading, setError: setSystemError } = useSystemStore();
  const { setPorts, setLoading: setDMXLoading, setError: setDMXError } = useDMXStore();
  const { setStatus, setError: setNetworkError } = useNetworkStore();

  // Poll system info
  usePolling({
    interval: POLL_INTERVAL_SYSTEM,
    onPoll: React.useCallback(async () => {
      try {
        setSystemLoading(true);
        const response = await apiClient.get<SystemInfo>(API_ENDPOINTS.SYSTEM_INFO);
        setInfo(response.data);
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Failed to fetch system info';
        setSystemError(message);
      } finally {
        setSystemLoading(false);
      }
    }, [setInfo, setSystemLoading, setSystemError]),
    onError: React.useCallback((error: Error) => {
      console.error('System polling error:', error);
    }, []),
  });

  // Poll DMX status
  usePolling({
    interval: POLL_INTERVAL_DMX,
    onPoll: React.useCallback(async () => {
      try {
        setDMXLoading(true);
        const response = await apiClient.get<DMXPortStatus[]>(API_ENDPOINTS.DMX_STATUS);
        setPorts(response.data);
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Failed to fetch DMX status';
        setDMXError(message);
      } finally {
        setDMXLoading(false);
      }
    }, [setPorts, setDMXLoading, setDMXError]),
    onError: React.useCallback((error: Error) => {
      console.error('DMX polling error:', error);
    }, []),
  });

  // Poll network status
  usePolling({
    interval: POLL_INTERVAL_NETWORK,
    onPoll: React.useCallback(async () => {
      try {
        const response = await apiClient.get<NetworkStatusType>(API_ENDPOINTS.NETWORK_STATUS);
        setStatus(response.data);
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Failed to fetch network status';
        setNetworkError(message);
      }
    }, [setStatus, setNetworkError]),
    onError: React.useCallback((error: Error) => {
      console.error('Network polling error:', error);
    }, []),
  });

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Dashboard</h1>
        <p className="mt-1 text-sm text-gray-500">
          System overview and real-time status
        </p>
      </div>

      <SystemMetrics />
      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <DMXActivity />
        <NetworkStatus />
      </div>
    </div>
  );
};

