import React, { useState, useEffect } from 'react';
import { DMXPortConfig } from '../../api/types';
import { Input } from '../shared/Input';
import { Button } from '../shared/Button';
import { Toggle } from '../shared/Toggle';
import { 
  DMX_UNIVERSE_MIN, DMX_UNIVERSE_MAX, 
  DMX_BREAK_MIN, DMX_BREAK_MAX, 
  DMX_MAB_MIN, DMX_MAB_MAX 
} from '../../utils/constants';

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
  // Local state
  const [enabled, setEnabled] = useState(!!config.enabled);
  const [universe, setUniverse] = useState(config.universe.toString());
  const [breakUs, setBreakUs] = useState(config.break_us?.toString() || '176');
  const [mabUs, setMabUs] = useState(config.mab_us?.toString() || '12');

  // Biến cờ để biết người dùng có đang sửa hay không (chặn ghi đè từ server)
  const [isDirty, setIsDirty] = useState(false);

  // Đồng bộ từ Props vào State (Chỉ khi người dùng KHÔNG đang sửa)
  useEffect(() => {
    if (!isDirty) {
      setEnabled(!!config.enabled);
      setUniverse(config.universe.toString());
      setBreakUs(config.break_us?.toString() || '176');
      setMabUs(config.mab_us?.toString() || '12');
    }
  }, [config, isDirty]);

  // Hàm xử lý chung khi thay đổi giá trị
  const handleLocalChange = (updates: Partial<DMXPortConfig>) => {
    setIsDirty(true); // Đánh dấu là đang sửa
    onChange({ ...config, ...updates });
  };

  const handleSaveInternal = async () => {
    await onSave();
    setIsDirty(false); // Reset cờ sau khi save thành công
  };

  return (
    <div className="bg-gray-50 p-4 rounded-md border border-gray-200">
      <div className="grid grid-cols-1 md:grid-cols-2 gap-4">
        {/* Toggle Enabled */}
        <div className="flex items-center space-x-2">
            <span className="text-sm font-medium text-gray-700">Status</span>
            <Toggle 
                label={enabled ? "Enabled" : "Disabled"}
                checked={enabled}
                onChange={(checked) => {
                    setEnabled(checked);
                    handleLocalChange({ enabled: checked });
                }}
            />
        </div>

        {/* Universe Input */}
        <Input
          label="Universe"
          type="number"
          value={universe}
          onChange={(e) => {
            setUniverse(e.target.value);
            const val = parseInt(e.target.value, 10);
            if (!isNaN(val)) handleLocalChange({ universe: val });
          }}
          min={DMX_UNIVERSE_MIN}
          max={DMX_UNIVERSE_MAX}
        />

        {/* Break Timing */}
        <Input
          label="Break (μs)"
          type="number"
          value={breakUs}
          onChange={(e) => {
            setBreakUs(e.target.value);
            const val = parseInt(e.target.value, 10);
            if (!isNaN(val)) handleLocalChange({ break_us: val });
          }}
          min={DMX_BREAK_MIN}
          max={DMX_BREAK_MAX}
        />

        {/* MAB Timing */}
        <Input
          label="MAB (μs)"
          type="number"
          value={mabUs}
          onChange={(e) => {
            setMabUs(e.target.value);
            const val = parseInt(e.target.value, 10);
            if (!isNaN(val)) handleLocalChange({ mab_us: val });
          }}
          min={DMX_MAB_MIN}
          max={DMX_MAB_MAX}
        />
      </div>

      <div className="mt-4 flex justify-end">
        <Button 
          onClick={handleSaveInternal} 
          isLoading={isSaving}
          variant="primary"
          disabled={!isDirty && !isSaving} // Disable nút Save nếu chưa sửa gì
        >
          {isDirty ? `Save Port ${port} *` : `Save Port ${port}`}
        </Button>
      </div>
    </div>
  );
};