/**
 * Wi-Fi STA/AP configuration component
 */

import React, { useState } from 'react';
import { WifiStationConfig, WifiAPConfig } from '../../api/types';
import { Input } from '../shared/Input';
import { Button } from '../shared/Button';
import { validateSSID, validateWifiPassword } from '../../utils/validators';

export interface WifiConfigProps {
  staConfig: WifiStationConfig | null;
  apConfig: WifiAPConfig | null;
  onSaveSTA: (config: WifiStationConfig) => Promise<void>;
  onSaveAP: (config: WifiAPConfig) => Promise<void>;
  isSaving?: boolean;
}

export const WifiConfig: React.FC<WifiConfigProps> = ({
  staConfig,
  apConfig,
  onSaveSTA,
  onSaveAP,
  isSaving = false,
}) => {
  const [staEnabled, setStaEnabled] = useState(staConfig?.enabled ?? false);
  const [staSSID, setStaSSID] = useState(staConfig?.ssid || '');
  const [staPassword, setStaPassword] = useState(staConfig?.password || '');

  const [apEnabled, setApEnabled] = useState(apConfig?.enabled ?? false);
  const [apSSID, setApSSID] = useState(apConfig?.ssid || 'DMX-Node');
  const [apPassword, setApPassword] = useState(apConfig?.password || '');
  const [apChannel, setApChannel] = useState(apConfig?.channel?.toString() || '1');

  const [errors, setErrors] = useState<Record<string, string>>({});

  const validateSTA = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (staEnabled) {
      if (!validateSSID(staSSID)) {
        newErrors.staSSID = 'SSID is required (1-32 characters)';
      }
      if (!validateWifiPassword(staPassword)) {
        newErrors.staPassword = 'Password must be 8-63 characters';
      }
    }

    setErrors((prev) => ({ ...prev, ...newErrors }));
    return Object.keys(newErrors).length === 0;
  };

  const validateAP = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (apEnabled) {
      if (!validateSSID(apSSID)) {
        newErrors.apSSID = 'SSID is required (1-32 characters)';
      }
      if (apPassword && !validateWifiPassword(apPassword)) {
        newErrors.apPassword = 'Password must be 8-63 characters';
      }
      const channel = parseInt(apChannel, 10);
      if (isNaN(channel) || channel < 1 || channel > 11) {
        newErrors.apChannel = 'Channel must be 1-11';
      }
    }

    setErrors((prev) => ({ ...prev, ...newErrors }));
    return Object.keys(newErrors).length === 0;
  };

  const handleSaveSTA = async () => {
    if (!validateSTA()) return;

    await onSaveSTA({
      enabled: staEnabled,
      ssid: staSSID,
      password: staPassword,
    });
  };

  const handleSaveAP = async () => {
    if (!validateAP()) return;

    await onSaveAP({
      enabled: apEnabled,
      ssid: apSSID,
      password: apPassword,
      channel: parseInt(apChannel, 10),
    });
  };

  return (
    <div className="space-y-6">
      {/* Wi-Fi Station Configuration */}
      <div className="bg-white rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Wi-Fi Station (STA)</h3>

        <div className="space-y-4">
          <label className="flex items-center">
            <input
              type="checkbox"
              checked={staEnabled}
              onChange={(e) => setStaEnabled(e.target.checked)}
              className="rounded border-gray-300 text-primary-600 focus:ring-primary-500"
            />
            <span className="ml-2 text-sm text-gray-700">Enable Wi-Fi Station</span>
          </label>

          {staEnabled && (
            <>
              <Input
                label="SSID"
                type="text"
                value={staSSID}
                onChange={(e) => setStaSSID(e.target.value)}
                placeholder="Network name"
                error={errors.staSSID}
              />

              <Input
                label="Password"
                type="password"
                value={staPassword}
                onChange={(e) => setStaPassword(e.target.value)}
                placeholder="Wi-Fi password"
                error={errors.staPassword}
              />

              <Button
                onClick={handleSaveSTA}
                isLoading={isSaving}
                disabled={Object.keys(errors).some((k) => k.startsWith('sta'))}
                className="w-full"
              >
                Save STA Configuration
              </Button>
            </>
          )}
        </div>
      </div>

      {/* Wi-Fi AP Configuration */}
      <div className="bg-white rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Wi-Fi Access Point (AP)</h3>

        <div className="space-y-4">
          <label className="flex items-center">
            <input
              type="checkbox"
              checked={apEnabled}
              onChange={(e) => setApEnabled(e.target.checked)}
              className="rounded border-gray-300 text-primary-600 focus:ring-primary-500"
            />
            <span className="ml-2 text-sm text-gray-700">Enable Wi-Fi AP</span>
          </label>

          {apEnabled && (
            <>
              <Input
                label="SSID"
                type="text"
                value={apSSID}
                onChange={(e) => setApSSID(e.target.value)}
                placeholder="AP network name"
                error={errors.apSSID}
              />

              <Input
                label="Password (optional)"
                type="password"
                value={apPassword}
                onChange={(e) => setApPassword(e.target.value)}
                placeholder="Leave empty for open AP"
                error={errors.apPassword}
                helperText="8-63 characters, or leave empty for open network"
              />

              <Input
                label="Channel"
                type="number"
                value={apChannel}
                onChange={(e) => setApChannel(e.target.value)}
                min="1"
                max="11"
                error={errors.apChannel}
                helperText="Wi-Fi channel (1-11)"
              />

              <Button
                onClick={handleSaveAP}
                isLoading={isSaving}
                disabled={Object.keys(errors).some((k) => k.startsWith('ap'))}
                className="w-full"
              >
                Save AP Configuration
              </Button>
            </>
          )}
        </div>
      </div>
    </div>
  );
};

