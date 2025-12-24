/**
 * System metrics display component (CPU, RAM, Uptime)
 */

import React from 'react';
import { useSystemStore } from '../../stores/systemStore';
import { formatUptime, formatBytes, formatPercent } from '../../utils/formatters';
import { LoadingSpinner } from '../shared/LoadingSpinner';

export const SystemMetrics: React.FC = () => {
  const info = useSystemStore((state) => state.info);
  const loading = useSystemStore((state) => state.loading);

  if (loading && !info) {
    return (
      <div className="bg-white rounded-lg shadow p-6">
        <LoadingSpinner />
      </div>
    );
  }

  if (!info) {
    return (
      <div className="bg-white rounded-lg shadow p-6">
        <p className="text-gray-500">No system information available</p>
      </div>
    );
  }

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h2 className="text-lg font-semibold text-gray-900 mb-4">System Information</h2>
      <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
        <div>
          <p className="text-sm text-gray-500">Uptime</p>
          <p className="text-2xl font-semibold text-gray-900">
            {formatUptime(info.uptime)}
          </p>
        </div>
        {info.cpu_load !== undefined && (
          <div>
            <p className="text-sm text-gray-500">CPU Load</p>
            <p className="text-2xl font-semibold text-gray-900">
              {info.uptime !== undefined && info.uptime < 3 ? 'Calibrating…' : formatPercent(info.cpu_load)}
            </p>
          </div>
        )}
        {info.free_heap !== undefined && (
          <div>
            <p className="text-sm text-gray-500">Free Heap</p>
            <p className="text-2xl font-semibold text-gray-900">
              {formatBytes(info.free_heap)}
            </p>
          </div>
        )}
      </div>
      {info.device && (
        <div className="mt-4 pt-4 border-t border-gray-200">
          <p className="text-sm text-gray-500">Device ID</p>
          <p className="text-sm font-mono text-gray-900">{info.device}</p>
        </div>
      )}
      {info.version && (
        <div className="mt-2">
          <p className="text-sm text-gray-500">Firmware Version</p>
          <p className="text-sm font-mono text-gray-900">{info.version}</p>
        </div>
      )}
    </div>
  );
};

