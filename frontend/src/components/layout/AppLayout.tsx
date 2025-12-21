/**
 * Main application layout with navigation
 */

import React from 'react';
import { NavBar } from './NavBar';
import { DeviceConnector } from '../shared/DeviceConnector';

export interface AppLayoutProps {
  children: React.ReactNode;
}

export const AppLayout: React.FC<AppLayoutProps> = ({ children }) => {
  return (
    <div className="min-h-screen bg-gray-50">
      <NavBar />
      <DeviceConnector />
      <main className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8 py-8">
        {children}
      </main>
    </div>
  );
};

