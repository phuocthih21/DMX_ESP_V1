# DMX Node Frontend

React 18 + TypeScript SPA for DMX Node device configuration and monitoring.

## Features

- **Dashboard**: Real-time system metrics, DMX port activity, and network status
- **DMX Configuration**: Configure DMX ports, universe mapping, and timing parameters
- **Network Configuration**: Ethernet and Wi-Fi (STA/AP) settings
- **System Settings**: Reboot, factory reset, config import/export, and OTA firmware updates
  
  **Note:** OTA firmware update depends on firmware/backend support. If a device firmware does not implement the OTA API, the UI will show a clear message and disable the upload flow.
- **WebSocket Support**: Real-time status updates with fallback to REST polling
- **Offline-First**: Works completely offline when served from device (AP mode)

## Tech Stack

- React 18
- TypeScript
- Vite
- Zustand (state management)
- Axios (HTTP client)
- TailwindCSS
- React Router DOM

## Development

### Prerequisites

- Node.js 18+ and npm

### Setup

```bash
cd frontend
npm install
```

### Development Server

```bash
npm run dev
```

The app will be available at `http://localhost:5173`

### Building for Production

```bash
npm run build
```

Output will be in `dist/` directory, ready for gzip compression and embedding in firmware.

## Architecture

### API Client

- Base URL automatically switches between same-origin (production) and configured device IP (development)
- All API responses follow format: `{ ok: boolean, data: T, error: string | null }`
- Automatic error handling and user-friendly error messages

### State Management

- **systemStore**: System information (CPU, RAM, uptime)
- **dmxStore**: DMX port status and configuration
- **networkStore**: Network status and Wi-Fi networks
- **connectionStore**: Online/offline and WebSocket connection status
- **deviceStore**: Device IP management

### Polling & WebSocket

- REST API polling with configurable intervals:
  - System: 1s
  - DMX: 500ms
  - Network: 2s
- WebSocket automatically connects and updates stores
- Polling pauses when WebSocket is connected
- Automatic fallback to polling if WebSocket disconnects

### Device Connection

- **Production**: Uses same-origin (served from device)
- **Development**: Manual IP entry or mDNS discovery (future)
- Device IP persisted in localStorage
- Connection status indicator in navbar

## Project Structure

```
frontend/
├── src/
│   ├── api/              # API client and types
│   ├── stores/           # Zustand stores
│   ├── hooks/            # Custom hooks (polling, WebSocket)
│   ├── components/        # React components
│   │   ├── layout/       # App layout components
│   │   ├── dashboard/    # Dashboard components
│   │   ├── dmx/          # DMX configuration components
│   │   ├── network/      # Network configuration components
│   │   ├── system/       # System settings components
│   │   └── shared/       # Shared UI components
│   ├── pages/            # Page components
│   ├── utils/            # Utility functions
│   ├── styles/           # Global styles
│   ├── App.tsx           # Main app component
│   └── main.tsx          # Entry point
├── index.html
├── package.json
├── vite.config.ts
└── tsconfig.json
```

## API Endpoints

All endpoints are prefixed with `/api/` (backend currently uses `/api/`):

- `GET /system/info` - System information
- `POST /system/reboot` - Reboot device
- `POST /system/factory` - Factory reset
- `GET /dmx/status` - DMX port status
- `POST /dmx/config` - Update DMX configuration
- `GET /network/status` - Network status
- `POST /network/config` - Update network configuration
- `GET /file/export` - Export configuration
- `POST /file/import` - Import configuration

## WebSocket

WebSocket endpoint: `ws://<device-ip>/ws/status`

Message types:
- `system.status` - System metrics
- `dmx.port_status` - DMX port updates
- `network.link` - Network link status
- `system.event` - System events

## Building for Firmware Embedding

1. Build the project: `npm run build`
2. Gzip the files in `dist/`:
   ```bash
   gzip -9 dist/index.html
   gzip -9 dist/assets/*.js
   gzip -9 dist/assets/*.css
   ```
3. Convert to C arrays using `xxd -i` for embedding in firmware

## License

Part of the DMX Node project.

