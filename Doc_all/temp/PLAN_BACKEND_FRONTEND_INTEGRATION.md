# 📋 KẾ HOẠCH TRIỂN KHAI BACKEND VÀ KẾT NỐI FRONTEND

## 🎯 Mục tiêu

Xây dựng **MOD_WEB Backend** (ESP-IDF) và tích hợp hoàn chỉnh với **Frontend React SPA** để tạo hệ thống quản lý DMX Node qua Web Interface.

---

## 📐 Kiến trúc tổng thể

```
┌─────────────────────────────────────────────────────────┐
│                    Frontend (React SPA)                  │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌──────────┐ │
│  │ Dashboard│  │ DMX Config│ │ Network  │ │  System  │ │
│  └──────────┘  └──────────┘  └──────────┘  └──────────┘ │
│       │              │              │              │      │
│       └──────────────┴──────────────┴──────────────┘      │
│                          │                                │
│                    ┌─────▼─────┐                          │
│                    │ API Client │                          │
│                    │  (Axios)   │                          │
│                    └─────┬─────┘                          │
│                          │                                │
│                    ┌─────▼─────┐                          │
│                    │ WebSocket │                          │
│                    │  Client   │                          │
│                    └───────────┘                          │
└─────────────────────────┬─────────────────────────────────┘
                           │ HTTP/WS
                           │ JSON
┌─────────────────────────▼─────────────────────────────────┐
│              MOD_WEB Backend (ESP-IDF)                    │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │ HTTP Server  │  │ REST API     │  │ WebSocket    │   │
│  │ (esp_http)   │  │ Handlers     │  │ Handler      │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│         │                 │                 │            │
│         └─────────────────┴─────────────────┘            │
│                           │                               │
│                    ┌──────▼──────┐                        │
│                    │ Static Files │                        │
│                    │ (Gzip Assets)│                        │
│                    └──────────────┘                        │
└─────────────────────────┬─────────────────────────────────┘
                          │ API Calls
                          │
┌─────────────────────────▼─────────────────────────────────┐
│                    SYS_MOD (Core)                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐   │
│  │ Config Mgmt  │  │ State Machine│  │ Event System │   │
│  └──────┬───────┘  └──────┬───────┘  └──────┬───────┘   │
│         │                 │                 │            │
└─────────┼─────────────────┼─────────────────┼────────────┘
          │                 │                 │
    ┌─────▼─────┐     ┌─────▼─────┐     ┌─────▼─────┐
    │ MOD_DMX   │     │ MOD_NET   │     │ MOD_PROTO │
    │ (Realtime)│     │ (Network)  │     │ (ArtNet)  │
    └───────────┘     └───────────┘     └───────────┘
```

---

## 📁 Cấu trúc Backend (MOD_WEB)

### Thư mục và File

```
components/mod_web/
├── include/
│   ├── mod_web.h              # Public API (web_init, web_stop)
│   ├── mod_web_api.h          # API handler declarations
│   └── mod_web_types.h        # Internal types
│
├── src/
│   ├── mod_web.c              # Module init, lifecycle
│   ├── mod_web_server.c       # HTTP server setup
│   ├── mod_web_routes.c       # URI handler registration
│   │
│   ├── api/
│   │   ├── api_system.c       # /api/sys/* handlers
│   │   ├── api_dmx.c          # /api/dmx/* handlers
│   │   ├── api_network.c      # /api/network/* handlers
│   │   └── api_file.c         # /api/file/* handlers
│   │
│   ├── ws/
│   │   ├── ws_handler.c       # WebSocket connection handler
│   │   ├── ws_broadcast.c     # Broadcast logic
│   │   └── ws_events.c        # Event subscription
│   │
│   ├── static/
│   │   ├── static_handler.c   # Gzip static file serving
│   │   └── static_assets.h    # Embedded binary declarations
│   │
│   └── utils/
│       ├── json_utils.c       # JSON parsing helpers
│       ├── validation.c       # Input validation
│       └── error_handler.c    # Error response formatting
│
├── assets/                     # Generated from frontend build
│   ├── index_html.c           # index.html.gz as C array
│   ├── app_js.c               # app.js.gz as C array
│   └── app_css.c              # style.css.gz as C array
│
└── CMakeLists.txt             # Build configuration
```

---

## 🔌 API Endpoints Mapping

### 1. System APIs

| Frontend Endpoint | Backend Handler | SYS_MOD Call | Response Type |
|-------------------|-----------------|--------------|---------------|
| `GET /api/sys/info` | `api_system_info()` | `sys_get_system_status()` | `SystemInfo` |
| `POST /api/sys/reboot` | `api_system_reboot()` | `sys_system_control(REBOOT)` | `{ok: true}` |
| `POST /api/sys/factory` | `api_system_factory()` | `sys_system_control(FACTORY)` | `{ok: true}` |

**Request/Response Examples:**

```json
// GET /api/sys/info
Response:
{
  "ok": true,
  "data": {
    "device_id": "DMX-Node-001",
    "firmware_version": "4.0.0",
    "uptime": 3600,
    "cpu_load": 32,
    "free_heap": 145632,
    "eth_up": true,
    "wifi_up": false
  },
  "error": null
}
```

### 2. DMX APIs

| Frontend Endpoint | Backend Handler | SYS_MOD Call | Response Type |
|-------------------|-----------------|--------------|---------------|
| `GET /api/dmx/status` | `api_dmx_status()` | `sys_get_dmx_status()` | `DMXConfig` |
| `POST /api/dmx/config` | `api_dmx_config()` | `sys_apply_dmx_config()` | `{ok: true}` |

**Request/Response Examples:**

```json
// GET /api/dmx/status
Response:
{
  "ok": true,
  "data": {
    "ports": [
      {
        "port": 0,
        "universe": 1,
        "enabled": true,
        "fps": 40,
        "backend": "RMT",
        "activity_counter": 1234
      },
      // ... ports 1-3
    ]
  },
  "error": null
}

// POST /api/dmx/config
Request:
{
  "port": 0,
  "universe": 10,
  "enabled": true,
  "break_us": 200,
  "mab_us": 12
}
Response:
{
  "ok": true,
  "data": null,
  "error": null
}
```

### 3. Network APIs

| Frontend Endpoint | Backend Handler | SYS_MOD Call | Response Type |
|-------------------|-----------------|--------------|---------------|
| `GET /api/network/status` | `api_network_status()` | `sys_get_network_status()` | `NetworkStatus` |
| `POST /api/network/config` | `api_network_config()` | `sys_apply_network_config()` | `{ok: true}` |

**Request/Response Examples:**

```json
// GET /api/network/status
Response:
{
  "ok": true,
  "data": {
    "eth_up": true,
    "wifi_up": false,
    "eth_ip": "192.168.1.100",
    "wifi_ip": null,
    "eth_mac": "AA:BB:CC:DD:EE:FF",
    "wifi_mac": "11:22:33:44:55:66",
    "wifi_ssid": null,
    "wifi_rssi": null
  },
  "error": null
}

// POST /api/network/config
Request:
{
  "ethernet": {
    "enabled": true,
    "dhcp": false,
    "ip": "192.168.1.100",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1"
  }
}
Response:
{
  "ok": true,
  "data": null,
  "error": null
}
```

### 4. File APIs (Optional)

| Frontend Endpoint | Backend Handler | SYS_MOD Call | Response Type |
|-------------------|-----------------|--------------|---------------|
| `GET /api/file/export` | `api_file_export()` | `sys_export_config()` | `application/json` |
| `POST /api/file/import` | `api_file_import()` | `sys_import_config()` | `{ok: true}` |

---

## 🔄 WebSocket Implementation

### Endpoint
```
ws://<device-ip>/ws/status
```

### Message Envelope (Bắt buộc)

```json
{
  "type": "string",
  "ts": 12345678,
  "data": { }
}
```

### Event Types

#### 1. `system.status` (1 Hz)
```json
{
  "type": "system.status",
  "ts": 102345,
  "data": {
    "cpu": 32,
    "heap": 145632,
    "uptime": 3600
  }
}
```

#### 2. `dmx.port_status` (2-5 Hz per port)
```json
{
  "type": "dmx.port_status",
  "ts": 102350,
  "data": {
    "port": 1,
    "universe": 10,
    "enabled": true,
    "fps": 40
  }
}
```

#### 3. `network.link` (Event-based)
```json
{
  "type": "network.link",
  "ts": 102400,
  "data": {
    "iface": "eth",
    "status": "up"
  }
}
```

#### 4. `system.event` (Event-based)
```json
{
  "type": "system.event",
  "ts": 102500,
  "data": {
    "code": "CONFIG_APPLIED",
    "level": "info"
  }
}
```

### Backend Implementation Flow

```
SYS_MOD Event
    │
    ▼
esp_event_post(SYS_EVENT_BASE, SYS_EVT_*)
    │
    ▼
MOD_WEB Event Handler
    │
    ▼
ws_broadcast_message()
    │
    ├─► Create cJSON envelope
    ├─► Serialize to string
    ├─► httpd_ws_send_frame_async()
    └─► Free JSON & string
```

---

## 📦 Static File Serving

### Build Process

1. **Frontend Build:**
   ```bash
   cd frontend
   npm run build
   # Output: dist/index.html, dist/assets/*.js, dist/assets/*.css
   ```

2. **Gzip Compression:**
   ```bash
   cd dist
   gzip -9k index.html
   gzip -9k assets/*.js
   gzip -9k assets/*.css
   ```

3. **Convert to C Arrays:**
   ```bash
   xxd -i index.html.gz > ../components/mod_web/assets/index_html.c
   xxd -i assets/index-*.js.gz > ../components/mod_web/assets/app_js.c
   xxd -i assets/index-*.css.gz > ../components/mod_web/assets/app_css.c
   ```

### CMakeLists.txt Configuration

```cmake
idf_component_register(
    SRCS 
        "src/mod_web.c"
        "src/mod_web_server.c"
        # ... other sources
    INCLUDE_DIRS 
        "include"
    EMBED_FILES 
        "assets/index.html.gz"
        "assets/app.js.gz"
        "assets/style.css.gz"
)
```

### Handler Implementation

```c
static esp_err_t static_handler_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    
    extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
    extern const uint8_t index_html_gz_end[] asm("_binary_index_html_gz_end");
    
    size_t len = index_html_gz_end - index_html_gz_start;
    httpd_resp_send(req, (const char*)index_html_gz_start, len);
    return ESP_OK;
}
```

---

## 🔗 Integration Points

### 1. Frontend → Backend

#### API Client Configuration
- **Base URL**: Dynamic discovery hoặc manual IP
- **Timeout**: 5000ms
- **Content-Type**: `application/json`
- **Error Handling**: Interceptor với fallback

#### WebSocket Client
- **Auto-reconnect**: Exponential backoff
- **Fallback**: REST polling nếu WS fail
- **Message Handler**: Type-safe parsing

### 2. Backend → SYS_MOD

#### API Mapping
```c
// System
sys_get_system_status() → GET /api/sys/info
sys_system_control(REBOOT) → POST /api/sys/reboot

// DMX
sys_get_dmx_status() → GET /api/dmx/status
sys_apply_dmx_config() → POST /api/dmx/config

// Network
sys_get_network_status() → GET /api/network/status
sys_apply_network_config() → POST /api/network/config
```

#### Event Subscription
```c
esp_event_handler_register(
    SYS_EVENT_BASE,
    ESP_EVENT_ANY_ID,
    ws_event_handler,
    NULL
);
```

---

## 🛠️ Implementation Steps

### Phase 1: Backend Core (Week 1)

- [ ] **1.1** Tạo cấu trúc thư mục `components/mod_web/`
- [ ] **1.2** Implement `mod_web.c` - Module initialization
- [ ] **1.3** Implement `mod_web_server.c` - HTTP server setup
- [ ] **1.4** Implement `mod_web_routes.c` - URI registration
- [ ] **1.5** Tạo `CMakeLists.txt` với cấu hình đúng

### Phase 2: REST API Handlers (Week 2)

- [ ] **2.1** Implement `api_system.c`
  - [ ] `GET /api/sys/info`
  - [ ] `POST /api/sys/reboot`
  - [ ] `POST /api/sys/factory`
- [ ] **2.2** Implement `api_dmx.c`
  - [ ] `GET /api/dmx/status`
  - [ ] `POST /api/dmx/config`
- [ ] **2.3** Implement `api_network.c`
  - [ ] `GET /api/network/status`
  - [ ] `POST /api/network/config`
- [ ] **2.4** Implement `json_utils.c` - JSON parsing helpers
- [ ] **2.5** Implement `validation.c` - Input validation
- [ ] **2.6** Implement `error_handler.c` - Error responses

### Phase 3: WebSocket (Week 3)

- [ ] **3.1** Implement `ws_handler.c` - Connection management
- [ ] **3.2** Implement `ws_broadcast.c` - Message broadcasting
- [ ] **3.3** Implement `ws_events.c` - Event subscription
- [ ] **3.4** Tích hợp với SYS_MOD event system
- [ ] **3.5** Test WebSocket với frontend client

### Phase 4: Static File Serving (Week 4)

- [ ] **4.1** Setup build script để gzip frontend assets
- [ ] **4.2** Convert gzip files thành C arrays
- [ ] **4.3** Implement `static_handler.c`
- [ ] **4.4** Test serving `index.html`, `app.js`, `style.css`
- [ ] **4.5** Verify Gzip decompression trong browser

### Phase 5: Frontend Integration (Week 5)

- [ ] **5.1** Verify API endpoints mapping
- [ ] **5.2** Test API client với real backend
- [ ] **5.3** Test WebSocket connection
- [ ] **5.4** Test error handling và reconnection
- [ ] **5.5** Test static file loading

### Phase 6: Testing & Validation (Week 6)

- [ ] **6.1** Unit tests cho API handlers
- [ ] **6.2** Integration tests với SYS_MOD
- [ ] **6.3** Stress test (concurrent requests)
- [ ] **6.4** Memory leak testing
- [ ] **6.5** Performance testing (response time < 200ms)

---

## 🧪 Testing Strategy

### 1. Backend Unit Tests

```c
// Test API handler
void test_api_system_info(void) {
    httpd_req_t req = {0};
    // Mock SYS_MOD response
    esp_err_t ret = api_system_info(&req);
    TEST_ASSERT_EQUAL(ESP_OK, ret);
}
```

### 2. Integration Tests

- **Mock SYS_MOD**: Tạo mock implementation để test MOD_WEB độc lập
- **Frontend Mock Server**: Node.js/Python server để test frontend

### 3. End-to-End Tests

- **Real Hardware**: Test trên ESP32-S3 thật
- **Network Simulation**: Test với network issues
- **Stress Test**: 100+ concurrent requests

---

## 📊 Performance Requirements

| Metric | Target | Measurement |
|--------|--------|-------------|
| API Response Time | < 200ms | Average over 100 requests |
| WebSocket Latency | < 50ms | Event to client delivery |
| Memory Usage | < 8KB stack | Task stack size |
| CPU Usage | < 5% | Average during operation |
| Concurrent Connections | 4 clients | HTTP + WS |

---

## 🔒 Security Considerations

1. **Input Validation**: Tất cả input phải được validate
2. **Range Checking**: Port, universe, timing values
3. **Memory Safety**: Không có buffer overflow
4. **Error Handling**: Không expose internal errors
5. **Rate Limiting**: (Optional) Prevent DoS

---

## 📝 Code Quality Rules

Theo **User Rules** của dự án:

1. ✅ Code rõ ràng, dễ đọc
2. ✅ Không lặp lại (DRY)
3. ✅ Tổ chức module rõ ràng
4. ✅ Xử lý lỗi đầy đủ
5. ✅ Comment thông minh
6. ✅ Hàm gọn nhẹ (< 30 dòng)
7. ✅ Type safety (TypeScript/C types)

---

## 🚀 Deployment Process

### 1. Build Frontend
```bash
cd frontend
npm install
npm run build
```

### 2. Generate Assets
```bash
./scripts/generate_assets.sh
```

### 3. Build Firmware
```bash
idf.py build
```

### 4. Flash Firmware
```bash
idf.py flash monitor
```

### 5. Verify
- Access `http://<device-ip>/` → Should load SPA
- Test API endpoints với `curl`
- Test WebSocket connection

---

## 📚 Tài liệu tham khảo

1. **API-Contract.md** - System types và module interfaces
2. **MOD_WEB.md** - Backend specification
3. **web_socket_spec_v_1.md** - WebSocket protocol
4. **WEB.md** - Web layer requirements
5. **MOD_WEB_Backend.md** - Detailed backend design

---

## ✅ Definition of Done

Backend được coi là hoàn thành khi:

- [x] Tất cả API endpoints hoạt động đúng
- [x] WebSocket realtime status stream hoạt động
- [x] Static files được serve đúng (Gzip)
- [x] Frontend load và hoạt động hoàn chỉnh
- [x] Integration tests pass
- [x] Performance requirements đạt
- [x] Memory leak free
- [x] Documentation đầy đủ

---

**Ngày tạo**: 2024
**Phiên bản**: 1.0
**Trạng thái**: Draft - Ready for Implementation

