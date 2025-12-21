/**
 * OTA firmware upload component with progress bar
 */

import React, { useState, useRef } from 'react';
import { Button } from '../shared/Button';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { useToast } from '../shared/Toast';

export const OTAUpload: React.FC = () => {
  const [isUploading, setIsUploading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(0);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { showToast } = useToast();

  const handleFileSelect = async (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (!file) return;

    // Validate file type (typically .bin for ESP32)
    if (!file.name.endsWith('.bin')) {
      showToast('Please select a .bin firmware file', 'error');
      return;
    }

    setIsUploading(true);
    setUploadProgress(0);

    try {
      const formData = new FormData();
      formData.append('firmware', file);

      // Note: OTA endpoint may vary - adjust based on actual API
      // This is a placeholder implementation
      await apiClient.post(API_ENDPOINTS.SYSTEM_OTA, formData, {
        headers: {
          'Content-Type': 'multipart/form-data',
        },
        onUploadProgress: (progressEvent) => {
          if (progressEvent.total) {
            const percentCompleted = Math.round(
              (progressEvent.loaded * 100) / progressEvent.total
            );
            setUploadProgress(percentCompleted);
          }
        },
        timeout: 300000, // 5 minutes for firmware upload
      });

      showToast('Firmware uploaded successfully. Device will reboot.', 'success', 5000);
      
      // Reset file input
      if (fileInputRef.current) {
        fileInputRef.current.value = '';
      }

      // Device will reboot after OTA
      setTimeout(() => {
        window.location.reload();
      }, 5000);
    } catch (error) {
      {
        const message = error instanceof Error ? error.message : 'Failed to upload firmware';
        const lower = typeof message === 'string' ? message.toLowerCase() : '';
        if (lower.includes('404') || lower.includes('not found') || lower.includes('not implemented') || lower.includes('unsupported')) {
          showToast('OTA is not supported on this firmware build', 'error');
        } else {
          showToast(message, 'error');
        }
      }
      setIsUploading(false);
      setUploadProgress(0);
    }
  };

  return (
    <div className="bg-white rounded-lg shadow p-6">
      <h3 className="text-lg font-semibold text-gray-900 mb-4">OTA Firmware Update</h3>
      <p className="text-sm text-gray-500 mb-4">
        Upload a new firmware file (.bin) to update the device. The device will reboot after upload.
      </p>

      <input
        ref={fileInputRef}
        type="file"
        accept=".bin"
        onChange={handleFileSelect}
        className="hidden"
        id="ota-upload-input"
      />

      <Button
        onClick={() => fileInputRef.current?.click()}
        disabled={isUploading}
        className="mb-4"
      >
        {isUploading ? 'Uploading...' : 'Select Firmware File'}
      </Button>

      {isUploading && (
        <div className="mt-4">
          <div className="flex items-center justify-between mb-2">
            <span className="text-sm text-gray-600">Upload Progress</span>
            <span className="text-sm font-medium text-gray-900">{uploadProgress}%</span>
          </div>
          <div className="w-full bg-gray-200 rounded-full h-2.5">
            <div
              className="bg-primary-600 h-2.5 rounded-full transition-all duration-300"
              style={{ width: `${uploadProgress}%` }}
            ></div>
          </div>
        </div>
      )}
    </div>
  );
};

