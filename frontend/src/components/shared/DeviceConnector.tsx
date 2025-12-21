/**
 * Device IP input/selector component (for development/cross-origin)
 */

import React, { useState, useEffect } from 'react';
import { useDeviceStore } from '../../stores/deviceStore';
import { Input } from './Input';
import { Button } from './Button';
import { apiClient } from '../../api/client';
import { API_ENDPOINTS } from '../../api/endpoints';

export const DeviceConnector: React.FC = () => {
  const { deviceIP, setDeviceIP } = useDeviceStore();
  const [inputURL, setInputURL] = useState(() => {
    const stored = deviceIP.replace(/^https?:\/\//, '');
    return stored || 'localhost:3000';
  });
  const [error, setError] = useState<string | null>(null);
  const [isConnected, setIsConnected] = useState(false);

  // Don't show in production (same-origin mode)
  if (import.meta.env.PROD) {
    return null;
  }

  // Test connection when deviceIP changes (with debounce)
  useEffect(() => {
    if (!deviceIP) {
      setIsConnected(false);
      return;
    }

    let isMounted = true;
    const timeoutId = setTimeout(async () => {
      try {
        await apiClient.get(API_ENDPOINTS.SYSTEM_INFO, { timeout: 3000 });
        if (isMounted) {
          setIsConnected(true);
          setError(null);
        }
      } catch (err) {
        if (isMounted) {
          setIsConnected(false);
        }
      }
    }, 500); // Debounce 500ms

    return () => {
      isMounted = false;
      clearTimeout(timeoutId);
    };
  }, [deviceIP]);

  const handleConnect = () => {
    setError(null);
    
    if (!inputURL.trim()) {
      setError('Device URL is required');
      return;
    }

    // Accept formats: localhost:3000, 192.168.4.1, http://localhost:3000
    let url = inputURL.trim();
    if (!url.startsWith('http://') && !url.startsWith('https://')) {
      url = `http://${url}`;
    }

    setDeviceIP(url);
    // Connection will be tested automatically by useEffect
  };

  return (
    <div className="bg-gray-50 border-b border-gray-200 px-4 py-2">
      <div className="flex items-center gap-2">
        <span className="text-sm text-gray-600">Backend URL:</span>
        <div className="flex-1 max-w-xs">
          <Input
            value={inputURL}
            onChange={(e) => {
              setInputURL(e.target.value);
              setError(null);
            }}
            placeholder="localhost:3000"
            error={error || undefined}
            className="text-sm"
            onKeyPress={(e) => {
              if (e.key === 'Enter') {
                handleConnect();
              }
            }}
          />
        </div>
        <div className="flex items-center gap-2">
          {isConnected && (
            <span className="text-xs text-green-600 flex items-center gap-1">
              <span className="w-2 h-2 bg-green-500 rounded-full"></span>
              Connected
            </span>
          )}
          <Button size="sm" onClick={handleConnect}>
            Connect
          </Button>
        </div>
      </div>
      <div className="mt-1 text-xs text-gray-500">
        Examples: localhost:3000 (mock), 192.168.4.1 (device AP), 192.168.1.100 (device STA)
      </div>
    </div>
  );
};

