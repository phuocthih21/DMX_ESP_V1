/**
 * Ethernet IP configuration component
 */

import React, { useState } from 'react';
import { EthernetConfig } from '../../api/types';
import { Input } from '../shared/Input';
import { Button } from '../shared/Button';
import { validateIPv4, validateNetmask } from '../../utils/validators';

export interface EthernetConfigProps {
  config: EthernetConfig | null;
  onSave: (config: EthernetConfig) => Promise<void>;
  isSaving?: boolean;
}

export const EthernetConfigComponent: React.FC<EthernetConfigProps> = ({
  config,
  onSave,
  isSaving = false,
}) => {
  const [enabled, setEnabled] = useState(config?.enabled ?? false);
  const [dhcp, setDhcp] = useState(config?.dhcp ?? true);
  const [ip, setIp] = useState(config?.ip || '');
  const [netmask, setNetmask] = useState(config?.netmask || '255.255.255.0');
  const [gateway, setGateway] = useState(config?.gateway || '');
  const [errors, setErrors] = useState<Record<string, string>>({});

  const validate = (): boolean => {
    const newErrors: Record<string, string> = {};

    if (!dhcp) {
      if (!ip || !validateIPv4(ip)) {
        newErrors.ip = 'Valid IP address is required';
      }
      if (!netmask || !validateNetmask(netmask)) {
        newErrors.netmask = 'Valid netmask is required';
      }
      if (gateway && !validateIPv4(gateway)) {
        newErrors.gateway = 'Valid gateway IP is required';
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSave = async () => {
    if (!validate()) {
      return;
    }

    const updatedConfig: EthernetConfig = {
      enabled,
      dhcp,
      ...(dhcp ? {} : { ip, netmask, gateway: gateway || undefined }),
    };

    await onSave(updatedConfig);
  };

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h3 className="text-lg font-semibold text-gray-900 mb-4">Ethernet Configuration</h3>

      <div className="space-y-4">
        <label className="flex items-center">
          <input
            type="checkbox"
            checked={enabled}
            onChange={(e) => setEnabled(e.target.checked)}
            className="rounded border-gray-300 text-primary-600 focus:ring-primary-500"
          />
          <span className="ml-2 text-sm text-gray-700">Enable Ethernet</span>
        </label>

        {enabled && (
          <>
            <label className="flex items-center">
              <input
                type="checkbox"
                checked={dhcp}
                onChange={(e) => setDhcp(e.target.checked)}
                className="rounded border-gray-300 text-primary-600 focus:ring-primary-500"
              />
              <span className="ml-2 text-sm text-gray-700">Use DHCP</span>
            </label>

            {!dhcp && (
              <div className="space-y-4 pl-6 border-l-2 border-gray-200">
                <Input
                  label="IP Address"
                  type="text"
                  value={ip}
                  onChange={(e) => setIp(e.target.value)}
                  placeholder="192.168.1.100"
                  error={errors.ip}
                />

                <Input
                  label="Netmask"
                  type="text"
                  value={netmask}
                  onChange={(e) => setNetmask(e.target.value)}
                  placeholder="255.255.255.0"
                  error={errors.netmask}
                />

                <Input
                  label="Gateway (optional)"
                  type="text"
                  value={gateway}
                  onChange={(e) => setGateway(e.target.value)}
                  placeholder="192.168.1.1"
                  error={errors.gateway}
                />
              </div>
            )}
          </>
        )}

        <Button
          onClick={handleSave}
          isLoading={isSaving}
          disabled={Object.keys(errors).length > 0}
          className="w-full"
        >
          Save Ethernet Configuration
        </Button>
      </div>
    </div>
  );
};

