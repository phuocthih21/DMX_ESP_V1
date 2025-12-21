/**
 * DMX port activity indicators component
 */

import React from 'react';
import { useDMXStore } from '../../stores/dmxStore';
import { formatPortName, formatFPS } from '../../utils/formatters';
import { LoadingSpinner } from '../shared/LoadingSpinner';

export const DMXActivity: React.FC = () => {
  const ports = useDMXStore((state) => state.ports);
  const loading = useDMXStore((state) => state.loading);

  if (loading && ports.length === 0) {
    return (
      <div className="bg-white rounded-lg shadow p-6">
        <LoadingSpinner />
      </div>
    );
  }

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h2 className="text-lg font-semibold text-gray-900 mb-4">DMX Ports</h2>
      <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
        {[0, 1, 2, 3].map((portNum) => {
          const port = ports.find((p) => p.port === portNum);
          const isEnabled = port?.enabled ?? false;
          const universe = port?.universe ?? 0;
          const fps = port?.fps ?? 0;

          return (
            <div
              key={portNum}
              className={`border-2 rounded-lg p-4 ${
                isEnabled
                  ? 'border-green-500 bg-green-50'
                  : 'border-gray-300 bg-gray-50'
              }`}
            >
              <div className="flex items-center justify-between mb-2">
                <h3 className="font-semibold text-gray-900">
                  Port {formatPortName(portNum)}
                </h3>
                <div
                  className={`w-3 h-3 rounded-full ${
                    isEnabled ? 'bg-green-500' : 'bg-gray-400'
                  }`}
                  title={isEnabled ? 'Enabled' : 'Disabled'}
                />
              </div>
              {isEnabled ? (
                <>
                  <p className="text-sm text-gray-600">Universe: {universe}</p>
                  {fps > 0 && (
                    <p className="text-sm text-gray-600">{formatFPS(fps)}</p>
                  )}
                </>
              ) : (
                <p className="text-sm text-gray-500">Disabled</p>
              )}
            </div>
          );
        })}
      </div>
    </div>
  );
};

