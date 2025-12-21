/**
 * Network configuration page
 */

import React, { useState } from 'react';
import { EthernetConfigComponent } from '../components/network/EthernetConfig';
import { WifiConfig } from '../components/network/WifiConfig';
import { WifiScanner } from '../components/network/WifiScanner';
import { useNetworkStore } from '../stores/networkStore';
import { apiClient } from '../api/client';
import { API_ENDPOINTS } from '../api/endpoints';
import { NetworkStatus, NetworkConfig, EthernetConfig, WifiStationConfig, WifiAPConfig } from '../api/types';
import { usePolling } from '../hooks/usePolling';
import { POLL_INTERVAL_NETWORK } from '../utils/constants';
import { useToast } from '../components/shared/Toast';

export const NetworkConfigPage: React.FC = () => {
  const { status, setStatus, setError } = useNetworkStore();
  const [isSaving, setIsSaving] = useState(false);
  const { showToast } = useToast();

  // Poll network status
  usePolling({
    interval: POLL_INTERVAL_NETWORK,
    onPoll: async () => {
      try {
        const response = await apiClient.get<NetworkStatus>(API_ENDPOINTS.NETWORK_STATUS);
        setStatus(response.data);
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Failed to fetch network status';
        setError(message);
      }
    },
  });

  const handleSaveEthernet = async (config: EthernetConfig) => {
    setIsSaving(true);
    try {
      const networkConfig: NetworkConfig = {
        ethernet: config,
      };
      await apiClient.post(API_ENDPOINTS.NETWORK_CONFIG, networkConfig);
      showToast('Ethernet configuration saved', 'success');
      // Reload status
      const response = await apiClient.get<NetworkStatus>(API_ENDPOINTS.NETWORK_STATUS);
      setStatus(response.data);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to save Ethernet configuration',
        'error'
      );
    } finally {
      setIsSaving(false);
    }
  };

  const handleSaveSTA = async (config: WifiStationConfig) => {
    setIsSaving(true);
    try {
      const networkConfig: NetworkConfig = {
        wifi_sta: config,
      };
      await apiClient.post(API_ENDPOINTS.NETWORK_CONFIG, networkConfig);
      showToast('Wi-Fi STA configuration saved', 'success');
      // Reload status
      const response = await apiClient.get<NetworkStatus>(API_ENDPOINTS.NETWORK_STATUS);
      setStatus(response.data);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to save Wi-Fi STA configuration',
        'error'
      );
    } finally {
      setIsSaving(false);
    }
  };

  const handleSaveAP = async (config: WifiAPConfig) => {
    setIsSaving(true);
    try {
      const networkConfig: NetworkConfig = {
        wifi_ap: config,
      };
      await apiClient.post(API_ENDPOINTS.NETWORK_CONFIG, networkConfig);
      showToast('Wi-Fi AP configuration saved', 'success');
      // Reload status
      const response = await apiClient.get<NetworkStatus>(API_ENDPOINTS.NETWORK_STATUS);
      setStatus(response.data);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to save Wi-Fi AP configuration',
        'error'
      );
    } finally {
      setIsSaving(false);
    }
  };

  const handleSelectNetwork = (ssid: string) => {
    showToast(`Selected network: ${ssid}`, 'info');
  };

  // Extract current configs from status
  const ethernetConfig: EthernetConfig | null = status
    ? {
        enabled: status.eth_up,
        ip: status.eth_ip || undefined,
        dhcp: true, // Assume DHCP if IP is present (simplified)
      }
    : null;

  const staConfig: WifiStationConfig | null = status
    ? {
        enabled: status.wifi_up,
        ssid: status.wifi_ssid || '',
        password: '', // Password not returned in status
      }
    : null;

  const apConfig: WifiAPConfig | null = {
    enabled: true, // AP is typically always available
    ssid: 'DMX-Node',
    password: '',
  };

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">Network Configuration</h1>
        <p className="mt-1 text-sm text-gray-500">
          Configure Ethernet and Wi-Fi settings
        </p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <EthernetConfigComponent
          config={ethernetConfig}
          onSave={handleSaveEthernet}
          isSaving={isSaving}
        />
        <WifiScanner onSelectNetwork={handleSelectNetwork} />
      </div>

      <WifiConfig
        staConfig={staConfig}
        apConfig={apConfig}
        onSaveSTA={handleSaveSTA}
        onSaveAP={handleSaveAP}
        isSaving={isSaving}
      />
    </div>
  );
};

