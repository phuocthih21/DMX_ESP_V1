/**
 * DMX port configuration page
 */

import React, { useState, useEffect } from 'react';
import { PortConfig } from '../components/dmx/PortConfig';
import { PortStatus } from '../components/dmx/PortStatus';
import { useDMXStore } from '../stores/dmxStore';
import { apiClient } from '../api/client';
import { API_ENDPOINTS } from '../api/endpoints';
import { DMXPortStatus, DMXPortConfig, DMXConfig } from '../api/types';
import { usePolling } from '../hooks/usePolling';
import { POLL_INTERVAL_DMX } from '../utils/constants';
import { useToast } from '../components/shared/Toast';

export const DMXConfigPage: React.FC = () => {
  const { ports, setPorts, setLoading, setError } = useDMXStore();
  const [savingPort, setSavingPort] = useState<number | null>(null);
  const { showToast } = useToast();

  // Poll DMX status
  usePolling({
    interval: POLL_INTERVAL_DMX,
    onPoll: async () => {
      try {
        setLoading(true);
        const response = await apiClient.get(API_ENDPOINTS.DMX_STATUS);
        const portsData = Array.isArray(response.data)
          ? response.data
          : (response.data && Array.isArray(response.data.ports) ? response.data.ports : []);
        setPorts(portsData as DMXPortStatus[]);
      } catch (error) {
        const message = error instanceof Error ? error.message : 'Failed to fetch DMX status';
        setError(message);
      } finally {
        setLoading(false);
      }
    },
  });

  const handleConfigChange = (portConfig: DMXPortConfig) => {
    // Update local state immediately for better UX
    const updatedPorts = ports.map((p) =>
      p.port === portConfig.port
        ? { ...p, ...portConfig }
        : p
    );
    setPorts(updatedPorts);
  };

  const handleSave = async (portConfig: DMXPortConfig) => {
    setSavingPort(portConfig.port);
    try {
      const config: DMXConfig = {
        ports: [portConfig],
      };

      await apiClient.post(API_ENDPOINTS.DMX_CONFIG, config);
      showToast(`Port ${String.fromCharCode(65 + portConfig.port)} configuration saved`, 'success');
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to save configuration',
        'error'
      );
      // Reload status on error
      const response = await apiClient.get(API_ENDPOINTS.DMX_STATUS);
      const portsData = Array.isArray(response.data)
        ? response.data
        : (response.data && Array.isArray(response.data.ports) ? response.data.ports : []);
      setPorts(portsData as DMXPortStatus[]);
    } finally {
      setSavingPort(null);
    }
  };

  // Initialize ports if empty
  useEffect(() => {
    if (ports.length === 0) {
      // Create default port configs
      const defaultPorts: DMXPortStatus[] = [0, 1, 2, 3].map((port) => ({
        port,
        universe: port + 1,
        enabled: false,
      }));
      setPorts(defaultPorts);
    }
  }, [ports.length, setPorts]);

  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">DMX Configuration</h1>
        <p className="mt-1 text-sm text-gray-500">
          Configure DMX ports, universe mapping, and timing parameters
        </p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        {[0, 1, 2, 3].map((portNum) => {
          const port = ports.find((p) => p.port === portNum) || {
            port: portNum,
            universe: portNum + 1,
            enabled: false,
          };

          return (
            <div key={portNum} className="space-y-4">
              <PortStatus port={port} />
              <PortConfig
                port={portNum}
                config={port}
                onChange={handleConfigChange}
                onSave={() => handleSave(port)}
                isSaving={savingPort === portNum}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
};

