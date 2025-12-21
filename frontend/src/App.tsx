/**
 * Main app component with routing
 */

import React from 'react';
import { HashRouter, Routes, Route, Navigate } from 'react-router-dom';
import { AppLayout } from './components/layout/AppLayout';
import { ToastProvider } from './components/shared/Toast';
import { ErrorBoundary } from './components/shared/ErrorBoundary';
import { Dashboard } from './pages/Dashboard';
import { DMXConfigPage } from './pages/DMXConfig';
import { NetworkConfigPage } from './pages/NetworkConfig';
import { SystemSettingsPage } from './pages/SystemSettings';
import { useWebSocket } from './hooks/useWebSocket';
import { updateBaseURL } from './api/client';
import { useDeviceStore } from './stores/deviceStore';

function AppContent() {
  const deviceIP = useDeviceStore((state) => state.deviceIP);
  
  // Update API base URL when device IP changes
  React.useEffect(() => {
    updateBaseURL();
  }, [deviceIP]);

  // Initialize WebSocket connection only if device IP is set
  useWebSocket({
    enabled: !!deviceIP || import.meta.env.PROD, // Only enable if deviceIP set or in production
    onConnect: () => {
      console.log('[App] WebSocket connected');
    },
    onDisconnect: () => {
      console.log('[App] WebSocket disconnected');
    },
    onError: (error) => {
      console.error('[App] WebSocket error:', error);
    },
  });

  return (
    <HashRouter>
      <AppLayout>
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/dmx" element={<DMXConfigPage />} />
          <Route path="/network" element={<NetworkConfigPage />} />
          <Route path="/system" element={<SystemSettingsPage />} />
          <Route path="*" element={<Navigate to="/" replace />} />
        </Routes>
      </AppLayout>
    </HashRouter>
  );
}

function App() {
  return (
    <ErrorBoundary>
      <ToastProvider>
        <AppContent />
      </ToastProvider>
    </ErrorBoundary>
  );
}

export default App;

