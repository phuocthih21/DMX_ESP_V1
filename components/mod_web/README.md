# MOD_WEB - Web Server Module

## 📋 Overview

MOD_WEB provides HTTP server functionality for the DMX Node system, including:
- REST API endpoints for system configuration
- Static file serving (SPA frontend)
- WebSocket support for realtime status updates

## 🏗️ Architecture

```
MOD_WEB
├── HTTP Server (esp_http_server)
│   ├── Static File Handlers (/)
│   ├── REST API Handlers (/api/*)
│   └── WebSocket Handler (/ws/status)
├── JSON Utilities (cJSON)
├── Validation Layer
└── Error Handling
```

## 📁 File Structure

```
components/mod_web/
├── include/
│   ├── mod_web.h              # Public API
│   ├── mod_web_server.h       # HTTP server management
│   ├── mod_web_routes.h       # Route registration
│   ├── mod_web_api.h          # REST API handlers
│   ├── mod_web_static.h       # Static file serving
│   ├── mod_web_ws.h          # WebSocket handler
│   ├── mod_web_json.h        # JSON utilities
│   ├── mod_web_validation.h  # Input validation
│   └── mod_web_error.h       # Error responses
├── src/
│   ├── mod_web.c              # Module init
│   ├── mod_web_server.c      # HTTP server setup
│   ├── mod_web_routes.c      # Route registration
│   ├── mod_web_api.c         # REST API implementation
│   ├── mod_web_static.c      # Static file serving
│   ├── mod_web_ws.c          # WebSocket implementation
│   ├── mod_web_json.c        # JSON utilities
│   ├── mod_web_validation.c # Validation functions
│   └── mod_web_error.c      # Error handling
├── assets/                    # Embedded static files (generated)
└── CMakeLists.txt
```

## 🔌 API Endpoints

### System APIs

- `GET /api/sys/info` - Get system information
- `POST /api/sys/reboot` - Reboot device
- `POST /api/sys/factory` - Factory reset

### DMX APIs

- `GET /api/dmx/status` - Get DMX port status
- `POST /api/dmx/config` - Update DMX port configuration

### Network APIs

- `GET /api/network/status` - Get network status
- `POST /api/network/config` - Update network configuration

### WebSocket

- `ws://<ip>/ws/status` - Realtime status stream

### Authentication

A minimal authentication system is available to protect admin endpoints. New endpoints:

- `POST /api/auth/set_password` - Set admin password. Allowed when no password is set, or when authenticated.
  - Request body: `{ "password": "<plain>" }`
- `POST /api/auth/login` - Obtain a Bearer token to authenticate subsequent requests.
  - Request body: `{ "password": "<plain>" }`
  - Response: `{ "token": "<hex>", "expires_seconds": 28800 }`

Protected admin endpoints (e.g., `/api/sys/reboot`, `/api/sys/factory`, `/api/net/config`) will return `401 Unauthorized` when authentication is enabled and the request lacks a valid `Authorization: Bearer <token>` header.

Notes:
- Passwords are stored as a SHA-256 hex hash in NVS (`namespace: auth`, `key: admin_hash`).
- Session tokens are stored in RAM and expire after a configurable period (8 hours by default).
## 🚀 Usage

### Initialization

```c
#include "mod_web.h"

void app_main(void)
{
    // Initialize other modules first
    sys_mod_init();
    mod_net_init();
    
    // Initialize web server
    esp_err_t ret = mod_web_init();
    if (ret != ESP_OK) {
        ESP_LOGE("APP", "Failed to initialize MOD_WEB");
        return;
    }
    
    // Web server is now running
}
```

### Configuration

The web server runs on Core 0 with low priority to avoid interfering with realtime DMX operations.

Default configuration:
- Port: 80
- Stack size: 8KB
- Max connections: 4
- Max URI handlers: 16

## 📦 Dependencies

- `esp_http_server` - HTTP server component
- `cJSON` - JSON parsing
- `sys_mod` - System core module
- `mod_net` - Network module
- `esp_timer` - Timer functions
- `freertos` - FreeRTOS
- `esp_system` - System functions

## 🔧 Build Configuration

The module is configured via `CMakeLists.txt`. To embed static files:

1. Build frontend: `cd frontend && npm run build`
2. Gzip assets: `gzip -9k dist/index.html dist/assets/*.js dist/assets/*.css`
3. Convert to C arrays: `xxd -i file.gz > assets/file.c`
4. Update `mod_web_static.c` to use embedded binaries

## ⚠️ Notes

- All API handlers are non-blocking
- JSON parsing uses cJSON (must call `cJSON_Delete()` after use)
- Input validation is performed before calling SYS_MOD
- Error responses follow standard format: `{"ok": false, "error": "message"}`

## 🐛 Known Issues / TODOs

- [ ] WebSocket implementation needs completion
- [ ] Static file serving needs embedded binary integration
- [ ] CPU load calculation needs FreeRTOS stats integration
- [ ] MAC address retrieval from mod_net
- [ ] WiFi RSSI retrieval from mod_net

## 📚 Related Documentation

- `PLAN_BACKEND_FRONTEND_INTEGRATION.md` - Implementation plan
- `API_MAPPING_FRONTEND_BACKEND.md` - API mapping details
- `docs_v1/MOD_WEB.md` - Module specification
- `DESIGN_MOD/web_socket_spec_v_1.md` - WebSocket specification

