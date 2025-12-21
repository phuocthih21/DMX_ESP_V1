/**
 * Export configuration to JSON file
 */

import React, { useState } from 'react';
import { Button } from '../shared/Button';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { useToast } from '../shared/Toast';

export const ConfigExport: React.FC = () => {
  const [isExporting, setIsExporting] = useState(false);
  const { showToast } = useToast();

  const handleExport = async () => {
    setIsExporting(true);
    try {
      const response = await apiClient.get(API_ENDPOINTS.FILE_EXPORT, {
        responseType: 'blob',
      });

      // Create download link
      const blob = new Blob([response.data], { type: 'application/json' });
      const url = window.URL.createObjectURL(blob);
      const link = document.createElement('a');
      link.href = url;
      link.download = `dmx-node-config-${new Date().toISOString().split('T')[0]}.json`;
      document.body.appendChild(link);
      link.click();
      document.body.removeChild(link);
      window.URL.revokeObjectURL(url);

      showToast('Configuration exported successfully', 'success');
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to export configuration',
        'error'
      );
    } finally {
      setIsExporting(false);
    }
  };

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h3 className="text-lg font-semibold text-gray-900 mb-4">Export Configuration</h3>
      <p className="text-sm text-gray-500 mb-4">
        Download the current device configuration as a JSON file.
      </p>
      <Button onClick={handleExport} isLoading={isExporting}>
        Export Configuration
      </Button>
    </div>
  );
};

