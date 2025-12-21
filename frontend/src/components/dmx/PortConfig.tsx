/**
 * Individual DMX port configuration form component
 */

import React, { useState, useEffect } from 'react';
import { DMXPortConfig } from '../../api/types';
import { Input } from '../shared/Input';
import { Button } from '../shared/Button';
import { validateUniverse, validateBreakTiming, validateMABTiming } from '../../utils/validators';
import { DMX_UNIVERSE_MIN, DMX_UNIVERSE_MAX, DMX_BREAK_MIN, DMX_BREAK_MAX, DMX_MAB_MIN, DMX_MAB_MAX } from '../../utils/constants';
import { formatPortName } from '../../utils/formatters';

export interface PortConfigProps {
  port: number;
  config: DMXPortConfig;
  onChange: (config: DMXPortConfig) => void;
  onSave: () => Promise<void>;
  isSaving?: boolean;
}

export const PortConfig: React.FC<PortConfigProps> = ({
  port,
  config,
  onChange,
  onSave,
  isSaving = false,
}) => {
  const [universe, setUniverse] = useState(config.universe.toString());
  const [enabled, setEnabled] = useState(config.enabled);
  const [breakUs, setBreakUs] = useState(config.break_us?.toString() || '176');
  const [mabUs, setMabUs] = useState(config.mab_us?.toString() || '12');
  const [errors, setErrors] = useState<Record<string, string>>({});

  useEffect(() => {
    setUniverse(config.universe.toString());
    setEnabled(config.enabled);
    setBreakUs(config.break_us?.toString() || '176');
    setMabUs(config.mab_us?.toString() || '12');
  }, [config]);

  const validate = (): boolean => {
    const newErrors: Record<string, string> = {};

    const universeNum = parseInt(universe, 10);
    if (!validateUniverse(universeNum)) {
      newErrors.universe = `Universe must be between ${DMX_UNIVERSE_MIN} and ${DMX_UNIVERSE_MAX}`;
    }

    if (breakUs) {
      const breakNum = parseInt(breakUs, 10);
      if (!validateBreakTiming(breakNum)) {
        newErrors.break = `Break must be between ${DMX_BREAK_MIN} and ${DMX_BREAK_MAX} microseconds`;
      }
    }

    if (mabUs) {
      const mabNum = parseInt(mabUs, 10);
      if (!validateMABTiming(mabNum)) {
        newErrors.mab = `MAB must be between ${DMX_MAB_MIN} and ${DMX_MAB_MAX} microseconds`;
      }
    }

    setErrors(newErrors);
    return Object.keys(newErrors).length === 0;
  };

  const handleSave = async () => {
    if (!validate()) {
      return;
    }

    const updatedConfig: DMXPortConfig = {
      port,
      universe: parseInt(universe, 10),
      enabled,
      break_us: parseInt(breakUs, 10),
      mab_us: parseInt(mabUs, 10),
    };

    onChange(updatedConfig);
    await onSave();
  };

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <div className="flex items-center justify-between mb-4">
        <h3 className="text-lg font-semibold text-gray-900">
          Port {formatPortName(port)} Configuration
        </h3>
        <label className="flex items-center">
          <input
            type="checkbox"
            checked={enabled}
            onChange={(e) => {
              setEnabled(e.target.checked);
              onChange({ ...config, enabled: e.target.checked });
            }}
            className="rounded border-gray-300 text-primary-600 focus:ring-primary-500"
          />
          <span className="ml-2 text-sm text-gray-700">Enabled</span>
        </label>
      </div>

      <div className="space-y-4">
        <Input
          label="Universe"
          type="number"
          value={universe}
          onChange={(e) => {
            setUniverse(e.target.value);
            onChange({ ...config, universe: parseInt(e.target.value, 10) || 1 });
          }}
          min={DMX_UNIVERSE_MIN}
          max={DMX_UNIVERSE_MAX}
          error={errors.universe}
          helperText={`Range: ${DMX_UNIVERSE_MIN}-${DMX_UNIVERSE_MAX}`}
        />

        <div className="grid grid-cols-2 gap-4">
          <Input
            label="Break (μs)"
            type="number"
            value={breakUs}
            onChange={(e) => {
              setBreakUs(e.target.value);
              onChange({ ...config, break_us: parseInt(e.target.value, 10) || 176 });
            }}
            min={DMX_BREAK_MIN}
            max={DMX_BREAK_MAX}
            error={errors.break}
            helperText={`${DMX_BREAK_MIN}-${DMX_BREAK_MAX}μs`}
          />

          <Input
            label="MAB (μs)"
            type="number"
            value={mabUs}
            onChange={(e) => {
              setMabUs(e.target.value);
              onChange({ ...config, mab_us: parseInt(e.target.value, 10) || 12 });
            }}
            min={DMX_MAB_MIN}
            max={DMX_MAB_MAX}
            error={errors.mab}
            helperText={`${DMX_MAB_MIN}-${DMX_MAB_MAX}μs`}
          />
        </div>

        <Button
          onClick={handleSave}
          isLoading={isSaving}
          disabled={Object.keys(errors).length > 0}
          className="w-full"
        >
          Save Configuration
        </Button>
      </div>
    </div>
  );
};

