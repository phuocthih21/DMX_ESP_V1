/**
 * Network connection status component
 */

import React from 'react';
import { useNetworkStore } from '../../stores/networkStore';
import { LoadingSpinner } from '../shared/LoadingSpinner';

export const NetworkStatus: React.FC = () => {
  const status = useNetworkStore((state) => state.status);
  const loading = useNetworkStore((state) => state.error === null && !status);

  if (loading) {
    return (
      <div className="bg-white rounded-lg shadow p-6">
        <LoadingSpinner />
      </div>
    );
  }

  if (!status) {
    return (
      <div className="bg-white rounded-lg shadow p-6">
        <p className="text-gray-500">No network information available</p>
      </div>
    );
  }

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h2 className="text-lg font-semibold text-gray-900 mb-4">Network Status</h2>
      <div className="space-y-4">
        <div>
          <div className="flex items-center justify-between mb-2">
            <span className="text-sm font-medium text-gray-700">Ethernet</span>
            <div
              className={`w-3 h-3 rounded-full ${
                status.eth_up ? 'bg-green-500' : 'bg-gray-400'
              }`}
              title={status.eth_up ? 'Connected' : 'Disconnected'}
            />
          </div>
          {status.eth_up && status.eth_ip && (
            <p className="text-sm text-gray-600 font-mono">{status.eth_ip}</p>
          )}
        </div>
        <div>
          <div className="flex items-center justify-between mb-2">
            <span className="text-sm font-medium text-gray-700">Wi-Fi</span>
            <div
              className={`w-3 h-3 rounded-full ${
                status.wifi_up ? 'bg-green-500' : 'bg-gray-400'
              }`}
              title={status.wifi_up ? 'Connected' : 'Disconnected'}
            />
          </div>
          {status.wifi_up && status.wifi_ip && (
            <>
              <p className="text-sm text-gray-600 font-mono">{status.wifi_ip}</p>
              {status.wifi_ssid && (
                <p className="text-sm text-gray-500">SSID: {status.wifi_ssid}</p>
              )}
            </>
          )}
        </div>
      </div>
    </div>
  );
};

