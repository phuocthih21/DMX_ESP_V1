/**
 * System settings page
 */

import React from 'react';
import { RebootButton } from '../components/system/RebootButton';
import { FactoryResetButton } from '../components/system/FactoryResetButton';
import { ConfigExport } from '../components/system/ConfigExport';
import { ConfigImport } from '../components/system/ConfigImport';
import { OTAUpload } from '../components/system/OTAUpload';

export const SystemSettingsPage: React.FC = () => {
  return (
    <div className="space-y-6">
      <div>
        <h1 className="text-2xl font-bold text-gray-900">System Settings</h1>
        <p className="mt-1 text-sm text-gray-500">
          Device management and firmware updates
        </p>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
        <div className="bg-white rounded-lg shadow p-6">
          <h2 className="text-lg font-semibold text-gray-900 mb-4">Device Actions</h2>
          <div className="space-y-4">
            <RebootButton />
            <FactoryResetButton />
          </div>
        </div>

        <ConfigExport />
      </div>

      <ConfigImport />
      <OTAUpload />
    </div>
  );
};

