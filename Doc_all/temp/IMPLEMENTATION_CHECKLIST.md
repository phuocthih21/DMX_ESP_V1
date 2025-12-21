# ✅ Implementation Checklist - MOD_WEB Backend

## 📋 Quick Reference

### Phase 1: Core Infrastructure ✅

- [ ] **1.1** Tạo cấu trúc thư mục
  ```
  components/mod_web/
  ├── include/
  ├── src/
  │   ├── api/
  │   ├── ws/
  │   ├── static/
  │   └── utils/
  └── assets/
  ```

- [ ] **1.2** `mod_web.h` - Public API
  - [ ] `esp_err_t web_init(void);`
  - [ ] `esp_err_t web_stop(void);`

- [ ] **1.3** `mod_web.c` - Module init
  - [ ] HTTP server config
  - [ ] Task creation (Core 0, low priority)
  - [ ] Error handling

- [ ] **1.4** `mod_web_server.c` - HTTP server
  - [ ] `httpd_config_t` setup
  - [ ] Server start/stop
  - [ ] URI handler registration

- [ ] **1.5** `CMakeLists.txt`
  - [ ] Component registration
  - [ ] Dependencies (esp_http_server, cJSON)
  - [ ] EMBED_FILES configuration

---

### Phase 2: REST API Handlers ✅

#### System APIs

- [ ] **2.1** `api_system.c`
  - [ ] `GET /api/sys/info`
    - [ ] Call `sys_get_system_status()`
    - [ ] Format JSON response
    - [ ] Error handling
  - [ ] `POST /api/sys/reboot`
    - [ ] Validate request
    - [ ] Call `sys_system_control(REBOOT)`
    - [ ] Return success response
  - [ ] `POST /api/sys/factory`
    - [ ] Validate request (confirmation)
    - [ ] Call `sys_system_control(FACTORY)`
    - [ ] Return success response

#### DMX APIs

- [ ] **2.2** `api_dmx.c`
  - [ ] `GET /api/dmx/status`
    - [ ] Call `sys_get_dmx_status()`
    - [ ] Format ports array
    - [ ] Return JSON
  - [ ] `POST /api/dmx/config`
    - [ ] Parse JSON body
    - [ ] Validate port (0-3)
    - [ ] Validate universe (0-32767)
    - [ ] Validate break_us (88-500)
    - [ ] Validate mab_us (8-100)
    - [ ] Call `sys_apply_dmx_config()`
    - [ ] Return success/error

#### Network APIs

- [ ] **2.3** `api_network.c`
  - [ ] `GET /api/network/status`
    - [ ] Call `sys_get_network_status()`
    - [ ] Format IP, MAC, link status
    - [ ] Return JSON
  - [ ] `POST /api/network/config`
    - [ ] Parse JSON body
    - [ ] Validate IP format
    - [ ] Validate netmask
    - [ ] Validate gateway
    - [ ] Call `sys_apply_network_config()`
    - [ ] Return success/error

#### Utilities

- [ ] **2.4** `json_utils.c`
  - [ ] `parse_json_body()` - Parse HTTP request body
  - [ ] `create_success_response()` - Format success JSON
  - [ ] `create_error_response()` - Format error JSON
  - [ ] Memory cleanup helpers

- [ ] **2.5** `validation.c`
  - [ ] `validate_port()` - Port range check
  - [ ] `validate_universe()` - Universe range check
  - [ ] `validate_timing()` - Break/MAB range check
  - [ ] `validate_ip()` - IP format check

- [ ] **2.6** `error_handler.c`
  - [ ] `send_error()` - HTTP error response
  - [ ] `send_400()` - Bad request
  - [ ] `send_500()` - Internal error

---

### Phase 3: WebSocket Implementation ✅

- [ ] **3.1** `ws_handler.c`
  - [ ] WebSocket upgrade handler
  - [ ] Client connection tracking
  - [ ] Disconnect handling
  - [ ] Session management

- [ ] **3.2** `ws_broadcast.c`
  - [ ] `ws_broadcast_message()` - Send to all clients
  - [ ] `ws_send_frame_async()` - Non-blocking send
  - [ ] Queue management (drop if full)
  - [ ] Rate limiting (max 20 msg/sec)

- [ ] **3.3** `ws_events.c`
  - [ ] Subscribe to SYS_MOD events
  - [ ] Event to WS message conversion
  - [ ] Envelope creation (type, ts, data)
  - [ ] Message serialization

- [ ] **3.4** Event Types Implementation
  - [ ] `system.status` - 1 Hz periodic
  - [ ] `dmx.port_status` - 2-5 Hz per port
  - [ ] `network.link` - Event-based
  - [ ] `system.event` - Event-based

---

### Phase 4: Static File Serving ✅

- [ ] **4.1** Build Script
  - [ ] `scripts/generate_assets.sh`
  - [ ] Gzip compression
  - [ ] C array conversion (xxd)
  - [ ] Copy to assets/

- [ ] **4.2** `static_handler.c`
  - [ ] `GET /` → index.html.gz
  - [ ] `GET /app.js` → app.js.gz
  - [ ] `GET /style.css` → style.css.gz
  - [ ] Set Content-Encoding: gzip
  - [ ] Set Cache-Control header

- [ ] **4.3** Asset Embedding
  - [ ] Update CMakeLists.txt EMBED_FILES
  - [ ] Declare extern arrays
  - [ ] Test file serving

---

### Phase 5: Frontend Integration ✅

- [ ] **5.1** API Endpoint Verification
  - [ ] Test all GET endpoints
  - [ ] Test all POST endpoints
  - [ ] Verify JSON format matches frontend types

- [ ] **5.2** WebSocket Connection
  - [ ] Test connection from frontend
  - [ ] Test message reception
  - [ ] Test reconnection logic

- [ ] **5.3** Error Handling
  - [ ] Test invalid requests
  - [ ] Test network errors
  - [ ] Test timeout handling

- [ ] **5.4** Static Files
  - [ ] Load index.html in browser
  - [ ] Verify JS/CSS loading
  - [ ] Check Gzip decompression

---

### Phase 6: Testing & Validation ✅

- [ ] **6.1** Unit Tests
  - [ ] API handler tests
  - [ ] JSON parsing tests
  - [ ] Validation tests

- [ ] **6.2** Integration Tests
  - [ ] SYS_MOD integration
  - [ ] WebSocket event flow
  - [ ] Static file serving

- [ ] **6.3** Performance Tests
  - [ ] Response time < 200ms
  - [ ] Memory usage < 8KB stack
  - [ ] CPU usage < 5%
  - [ ] Concurrent connections (4)

- [ ] **6.4** Stress Tests
  - [ ] 100+ concurrent requests
  - [ ] WebSocket message flood
  - [ ] Memory leak detection

---

## 🔍 Code Review Checklist

### Code Quality

- [ ] Tuân thủ User Rules
- [ ] Functions < 30 lines
- [ ] Clear naming (snake_case)
- [ ] Comments for complex logic
- [ ] Error handling everywhere

### Memory Safety

- [ ] All `cJSON_Delete()` calls
- [ ] No memory leaks
- [ ] Buffer size checks
- [ ] No use-after-free

### Thread Safety

- [ ] Mutex protection for shared data
- [ ] Non-blocking API calls
- [ ] No blocking in HTTP handlers

### API Compliance

- [ ] All endpoints match spec
- [ ] JSON format matches frontend types
- [ ] Error codes correct
- [ ] WebSocket envelope format correct

---

## 📝 Documentation Checklist

- [ ] Code comments for public APIs
- [ ] README for mod_web component
- [ ] API endpoint documentation
- [ ] WebSocket message format docs
- [ ] Build instructions

---

## 🚀 Deployment Checklist

- [ ] Frontend build successful
- [ ] Assets generated and embedded
- [ ] Firmware builds without errors
- [ ] Flash successful
- [ ] Device accessible via IP
- [ ] Web UI loads correctly
- [ ] All APIs functional
- [ ] WebSocket connects

---

**Last Updated**: 2024
**Status**: In Progress

