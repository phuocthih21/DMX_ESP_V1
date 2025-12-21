/**
 * DMX timing configuration component (Break/MAB)
 * This is a simplified version - detailed timing is handled in PortConfig
 */

import React from 'react';

export const TimingConfig: React.FC = () => {
  // This component can be used for global timing settings if needed
  // For now, timing is configured per-port in PortConfig
  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h3 className="text-lg font-semibold text-gray-900 mb-4">DMX Timing</h3>
      <p className="text-sm text-gray-500">
        Timing configuration is available per-port in the port configuration section.
      </p>
    </div>
  );
};

