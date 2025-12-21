/**
 * Wi-Fi network scanner component
 */

import React, { useState } from 'react';
import { useNetworkStore } from '../../stores/networkStore';
import { Button } from '../shared/Button';
import { LoadingSpinner } from '../shared/LoadingSpinner';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { WifiNetwork } from '../../api/types';

export interface WifiScannerProps {
  onSelectNetwork: (ssid: string) => void;
}

export const WifiScanner: React.FC<WifiScannerProps> = ({ onSelectNetwork }) => {
  const { wifiNetworks, scanning, setScanning, setWifiNetworks } = useNetworkStore();
  const [error, setError] = useState<string | null>(null);

  const scanNetworks = async () => {
    setScanning(true);
    setError(null);

    try {
      // Note: This endpoint may not exist in the API yet
      // For now, we'll use a placeholder
      // In a real implementation, this would call: GET /api/network/wifi/scan
      const response = await apiClient.get<WifiNetwork[]>(`${API_ENDPOINTS.NETWORK_STATUS}/scan`);
      setWifiNetworks(response.data || []);
    } catch (err) {
      setError(err instanceof Error ? err.message : 'Failed to scan networks');
      console.error('Wi-Fi scan error:', err);
    } finally {
      setScanning(false);
    }
  };

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-lg font-semibold text-gray-900">Wi-Fi Networks</h3>
        <Button onClick={scanNetworks} isLoading={scanning} size="sm">
          Scan
        </Button>
      </div>

      {error && (
        <div className="mb-4 p-3 bg-red-50 border border-red-200 rounded text-sm text-red-800">
          {error}
        </div>
      )}

      {scanning && wifiNetworks.length === 0 ? (
        <LoadingSpinner />
      ) : wifiNetworks.length === 0 ? (
        <p className="text-sm text-gray-500">No networks found. Click "Scan" to search.</p>
      ) : (
        <div className="space-y-2">
          {wifiNetworks.map((network) => (
            <button
              key={network.ssid}
              onClick={() => onSelectNetwork(network.ssid)}
              className="w-full text-left p-3 border border-gray-200 rounded hover:bg-gray-50 transition-colors"
            >
              <div className="flex items-center justify-between">
                <div>
                  <p className="font-medium text-gray-900">{network.ssid}</p>
                  <p className="text-sm text-gray-500">
                    Signal: {network.rssi} dBm
                    {network.channel && ` • Channel: ${network.channel}`}
                  </p>
                </div>
                <div className="text-sm text-gray-500">
                  {network.auth_mode === 0 ? 'Open' : 'Secured'}
                </div>
              </div>
            </button>
          ))}
        </div>
      )}
    </div>
  );
};

