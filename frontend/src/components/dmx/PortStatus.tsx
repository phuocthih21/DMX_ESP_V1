/**
 * DMX port status display component
 */

import React from 'react';
import { DMXPortStatus as DMXPortStatusType } from '../../api/types';
import { formatPortName, formatFPS } from '../../utils/formatters';

export interface PortStatusProps {
  port: DMXPortStatusType;
}

export const PortStatus: React.FC<PortStatusProps> = ({ port }) => {
  return (
    <div className="bg-white rounded-lg shadow p-4">
      <div className="flex items-center justify-between mb-3">
        <h3 className="text-lg font-semibold text-gray-900">
          Port {formatPortName(port.port)}
        </h3>
        <div
          className={`px-3 py-1 rounded-full text-xs font-medium ${
            port.enabled
              ? 'bg-green-100 text-green-800'
              : 'bg-gray-100 text-gray-800'
          }`}
        >
          {port.enabled ? 'Enabled' : 'Disabled'}
        </div>
      </div>
      <div className="space-y-2">
        <div>
          <span className="text-sm text-gray-500">Universe:</span>
          <span className="ml-2 text-sm font-medium text-gray-900">
            {port.universe}
          </span>
        </div>
        {port.fps !== undefined && port.fps > 0 && (
          <div>
            <span className="text-sm text-gray-500">FPS:</span>
            <span className="ml-2 text-sm font-medium text-gray-900">
              {formatFPS(port.fps)}
            </span>
          </div>
        )}
        {port.backend && (
          <div>
            <span className="text-sm text-gray-500">Backend:</span>
            <span className="ml-2 text-sm font-medium text-gray-900">
              {port.backend}
            </span>
          </div>
        )}
        {port.activity_counter !== undefined && (
          <div>
            <span className="text-sm text-gray-500">Activity:</span>
            <span className="ml-2 text-sm font-medium text-gray-900">
              {port.activity_counter}
            </span>
          </div>
        )}
      </div>
    </div>
  );
};

