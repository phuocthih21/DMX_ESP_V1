/**
 * Import configuration from JSON file
 */

import React, { useState, useRef } from 'react';
import { Button } from '../shared/Button';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';
import { useToast } from '../shared/Toast';
import { ConfirmDialog } from '../shared/ConfirmDialog';

export const ConfigImport: React.FC = () => {
  const [isImporting, setIsImporting] = useState(false);
  const [showConfirm, setShowConfirm] = useState(false);
  const [fileToImport, setFileToImport] = useState<File | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { showToast } = useToast();

  const handleFileSelect = (event: React.ChangeEvent<HTMLInputElement>) => {
    const file = event.target.files?.[0];
    if (file) {
      setFileToImport(file);
      setShowConfirm(true);
    }
  };

  const handleImport = async () => {
    if (!fileToImport) return;

    setIsImporting(true);
    try {
      const fileContent = await fileToImport.text();
      const config = JSON.parse(fileContent);

      await apiClient.post(API_ENDPOINTS.FILE_IMPORT, config);

      showToast('Configuration imported successfully. Device may reboot.', 'success', 5000);
      
      // Reset file input
      if (fileInputRef.current) {
        fileInputRef.current.value = '';
      }
      setFileToImport(null);
      setShowConfirm(false);

      // Reload after a delay to reflect changes
      setTimeout(() => {
        window.location.reload();
      }, 2000);
    } catch (error) {
      showToast(
        error instanceof Error ? error.message : 'Failed to import configuration',
        'error'
      );
      setIsImporting(false);
      setShowConfirm(false);
    }
  };

  return (
    <>
      <div className="bg-white rounded-lg shadow p-6">
        <h3 className="text-lg font-semibold text-gray-900 mb-4">Import Configuration</h3>
        <p className="text-sm text-gray-500 mb-4">
          Upload a JSON configuration file to restore device settings.
        </p>
        <input
          ref={fileInputRef}
          type="file"
          accept=".json,application/json"
          onChange={handleFileSelect}
          className="hidden"
          id="config-import-input"
        />
        <Button
          onClick={() => fileInputRef.current?.click()}
          disabled={isImporting}
        >
          Select Configuration File
        </Button>
        {fileToImport && (
          <p className="mt-2 text-sm text-gray-600">Selected: {fileToImport.name}</p>
        )}
      </div>

      <ConfirmDialog
        isOpen={showConfirm}
        title="Import Configuration"
        message={`Are you sure you want to import configuration from "${fileToImport?.name}"? This will overwrite current settings.`}
        confirmLabel="Import"
        cancelLabel="Cancel"
        variant="warning"
        onConfirm={handleImport}
        onCancel={() => {
          setShowConfirm(false);
          setFileToImport(null);
          if (fileInputRef.current) {
            fileInputRef.current.value = '';
          }
        }}
      />
    </>
  );
};

