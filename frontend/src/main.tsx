/**
 * React entry point
 */

import React from 'react';
import ReactDOM from 'react-dom/client';
import App from './App';
import './styles/index.css';
import { loadStoredAuthToken } from './api/client';

// Load stored auth token (if any)
loadStoredAuthToken();

ReactDOM.createRoot(document.getElementById('root')!).render(
  <React.StrictMode>
    <App />
  </React.StrictMode>
);

