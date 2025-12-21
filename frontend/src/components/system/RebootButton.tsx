/**
 * Reboot action button with confirmation
 */

import React, { useState } from 'react';
import { Button } from '../shared/Button';
import { ConfirmDialog } from '../shared/ConfirmDialog';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { useToast } from '../shared/Toast';

export const RebootButton: React.FC = () => {
  const [showConfirm, setShowConfirm] = useState(false);
  const [isRebooting, setIsRebooting] = useState(false);
  const { showToast } = useToast();

  const handleReboot = async () => {
    setIsRebooting(true);
    try {
      await apiClient.post(API_ENDPOINTS.SYSTEM_REBOOT);
      showToast('Device rebooting...', 'info', 3000);
      // Device will disconnect, so we don't need to handle response
      setTimeout(() => {
        window.location.reload();
      }, 2000);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to reboot device',
        'error'
      );
      setIsRebooting(false);
      setShowConfirm(false);
    }
  };

  return (
    <>
      <Button
        variant="danger"
        onClick={() => setShowConfirm(true)}
        disabled={isRebooting}
      >
        Reboot Device
      </Button>
      <ConfirmDialog
        isOpen={showConfirm}
        title="Reboot Device"
        message="Are you sure you want to reboot the device? This will temporarily disconnect the connection."
        confirmLabel="Reboot"
        cancelLabel="Cancel"
        variant="warning"
        onConfirm={handleReboot}
        onCancel={() => setShowConfirm(false)}
      />
    </>
  );
};

