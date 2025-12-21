/**
 * Top navigation bar component
 */

import React from 'react';
import { Link, useLocation } from 'react-router-dom';
import { StatusIndicator } from './StatusIndicator';

const navItems = [
  { path: '/', label: 'Dashboard' },
  { path: '/dmx', label: 'DMX Config' },
  { path: '/network', label: 'Network' },
  { path: '/system', label: 'System' },
];

export const NavBar: React.FC = () => {
  const location = useLocation();

  return (
    <nav className="bg-white border-b border-gray-200">
      <div className="max-w-7xl mx-auto px-4 sm:px-6 lg:px-8">
        <div className="flex justify-between h-16">
          <div className="flex">
            <div className="flex-shrink-0 flex items-center">
              <h1 className="text-xl font-semibold text-gray-900">DMX Node</h1>
            </div>
            <div className="hidden sm:ml-6 sm:flex sm:space-x-8">
              {navItems.map((item) => {
                const isActive = location.pathname === item.path;
                return (
                  <Link
                    key={item.path}
                    to={item.path}
                    className={`inline-flex items-center px-1 pt-1 border-b-2 text-sm font-medium ${
                      isActive
                        ? 'border-primary-500 text-gray-900'
                        : 'border-transparent text-gray-500 hover:border-gray-300 hover:text-gray-700'
                    }`}
                  >
                    {item.label}
                  </Link>
                );
              })}
            </div>
          </div>
          <div className="flex items-center">
            <StatusIndicator />
          </div>
        </div>
      </div>
    </nav>
  );
};

