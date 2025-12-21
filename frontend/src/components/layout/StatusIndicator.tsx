/**
 * Connection status indicator component
 */

import React from 'react';
import { useConnectionStore } from '../../stores/connectionStore';

export const StatusIndicator: React.FC = () => {
  const online = useConnectionStore((state) => state.online);
  const wsConnected = useConnectionStore((state) => state.wsConnected);

  return (
    <div className="flex items-center gap-2 text-sm">
      <div className="flex items-center gap-1">
        <div
          className={`w-2 h-2 rounded-full ${
            online ? 'bg-green-500' : 'bg-red-500'
          }`}
          title={online ? 'Online' : 'Offline'}
        />
        <span className="text-gray-600">{online ? 'Online' : 'Offline'}</span>
      </div>
      {online && (
        <div className="flex items-center gap-1">
          <div
            className={`w-2 h-2 rounded-full ${
              wsConnected ? 'bg-blue-500' : 'bg-yellow-500'
            }`}
            title={wsConnected ? 'WebSocket Connected' : 'Polling Mode'}
          />
          <span className="text-gray-600">
            {wsConnected ? 'WS' : 'Poll'}
          </span>
        </div>
      )}
    </div>
  );
};

