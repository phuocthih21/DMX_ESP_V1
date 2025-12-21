/**
 * Factory reset action button with confirmation
 */

import React, { useState } from 'react';
import { Button } from '../shared/Button';
import { ConfirmDialog } from '../shared/ConfirmDialog';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { useToast } from '../shared/Toast';

export const FactoryResetButton: React.FC = () => {
  const [showConfirm, setShowConfirm] = useState(false);
  const [isResetting, setIsResetting] = useState(false);
  const { showToast } = useToast();

  const handleFactoryReset = async () => {
    setIsResetting(true);
    try {
      await apiClient.post(API_ENDPOINTS.SYSTEM_FACTORY);
      showToast('Factory reset initiated. Device will reboot.', 'warning', 5000);
      // Device will disconnect and reboot
      setTimeout(() => {
        window.location.reload();
      }, 3000);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to factory reset',
        'error'
      );
      setIsResetting(false);
      setShowConfirm(false);
    }
  };

  return (
    <>
      <Button
        variant="danger"
        onClick={() => setShowConfirm(true)}
        disabled={isResetting}
      >
        Factory Reset
      </Button>
      <ConfirmDialog
        isOpen={showConfirm}
        title="Factory Reset"
        message="This will erase all configuration and reset the device to factory defaults. This action cannot be undone. Are you sure?"
        confirmLabel="Reset"
        cancelLabel="Cancel"
        variant="danger"
        onConfirm={handleFactoryReset}
        onCancel={() => setShowConfirm(false)}
      />
    </>
  );
};

