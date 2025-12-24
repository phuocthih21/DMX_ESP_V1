import React, { useState, useEffect } from 'react';
import { PortConfig } from '../components/dmx/PortConfig';
import { useDMXStore } from '../stores/dmxStore';
import { apiClient } from '../api/client';
import { API_ENDPOINTS } from '../api/endpoints';
import { DMXPortStatus, DMXPortConfig } from '../api/types';
import { useToast } from '../components/shared/Toast';

export const DMXConfigPage: React.FC = () => {
  const { ports, setPorts } = useDMXStore();
  const [savingPort, setSavingPort] = useState<number | null>(null);
  const [isLoading, setIsLoading] = useState(false);
  const { showToast } = useToast();

  // Load data ONCE on mount (Thay thế Polling)
  useEffect(() => {
    const fetchData = async () => {
      setIsLoading(true);
      try {
        const response = await apiClient.get(API_ENDPOINTS.DMX_STATUS);
        const portsData = Array.isArray(response.data)
          ? response.data
          : (response.data?.ports || []);
        
        console.log("Loaded DMX Config:", portsData);
        setPorts(portsData as DMXPortStatus[]);
      } catch (err) {
        console.error("Load failed", err);
        showToast("Failed to load configuration", "error");
      } finally {
        setIsLoading(false);
      }
    };

    fetchData();
  }, [setPorts, showToast]);

  const updatePort = (portNum: number, newConfig: DMXPortConfig) => {
    const { updatePort } = useDMXStore.getState();
    updatePort(portNum, newConfig);
  };

  const handleSave = async (portNum: number, currentConfig: DMXPortConfig) => {
    try {
      setSavingPort(portNum);
      
      const payload = {
        port: portNum,
        universe: Number(currentConfig.universe) || 1,
        enabled: Boolean(currentConfig.enabled),
        break_us: currentConfig.break_us ? Number(currentConfig.break_us) : 176,
        mab_us: currentConfig.mab_us ? Number(currentConfig.mab_us) : 12,
      };

      console.log(`Sending DMX Config for Port ${portNum}:`, payload);

      await apiClient.post(API_ENDPOINTS.DMX_CONFIG, payload);
      showToast(`Port ${portNum} configuration saved`, 'success');
      
      // Refresh data sau khi save thành công
      const response = await apiClient.get(API_ENDPOINTS.DMX_STATUS);
      if(response.data?.ports) setPorts(response.data.ports);

    } catch (error: any) {
      console.error('Save failed:', error);
      const msg = error.response?.data?.error || error.message || "Unknown error";
      showToast(`Failed to save: ${msg}`, 'error');
    } finally {
      setSavingPort(null);
    }
  };

  if (isLoading && ports.length === 0) {
      return <div className="p-8 text-center">Loading configuration...</div>;
  }

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
              <h2 className="text-lg font-medium text-gray-900">Port {portNum}</h2>
              <PortConfig
                port={portNum}
                config={port as DMXPortConfig}
                onChange={(newConfig) => updatePort(portNum, newConfig)}
                onSave={() => handleSave(portNum, port as DMXPortConfig)}
                isSaving={savingPort === portNum}
              />
            </div>
          );
        })}
      </div>
    </div>
  );
};