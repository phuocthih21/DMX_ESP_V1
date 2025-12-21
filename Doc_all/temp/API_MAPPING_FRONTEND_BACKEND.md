# 🔗 API Mapping: Frontend ↔ Backend ↔ SYS_MOD

## 📋 Overview

Tài liệu này map các API endpoint từ Frontend qua Backend (MOD_WEB) đến SYS_MOD.

---

## 🔄 System APIs

### GET /api/sys/info

**Frontend Request:**
```typescript
GET /api/sys/info
```

**Backend Handler:** `api_system_info(httpd_req_t *req)`

**SYS_MOD Call:**
```c
const sys_config_t* cfg = sys_get_config();
// Also need: uptime, cpu_load, free_heap, network status
```

**Response Mapping:**
```json
{
  "ok": true,
  "data": {
    "device_id": cfg->device_label,           // From sys_get_config()
    "firmware_version": "4.0.0",              // Hardcoded
    "uptime": esp_timer_get_time() / 1000000, // From ESP-IDF
    "cpu_load": <calculate>,                  // From FreeRTOS stats
    "free_heap": esp_get_free_heap_size(),    // From ESP-IDF
    "eth_up": <from mod_net>,                 // Need mod_net API
    "wifi_up": <from mod_net>                 // Need mod_net API
  }
}
```

**Note:** Cần thêm API từ `mod_net` để lấy network status.

---

### POST /api/sys/reboot

**Frontend Request:**
```typescript
POST /api/sys/reboot
Body: {} // Empty or confirmation
```

**Backend Handler:** `api_system_reboot(httpd_req_t *req)`

**SYS_MOD Call:**
```c
// Need to add: sys_request_reboot() or use esp_restart()
esp_restart(); // Direct ESP-IDF call
```

**Response:**
```json
{
  "ok": true,
  "data": null
}
```

**Note:** Device sẽ reboot ngay, response có thể không được gửi.

---

### POST /api/sys/factory

**Frontend Request:**
```typescript
POST /api/sys/factory
Body: { "confirm": true }
```

**Backend Handler:** `api_system_factory(httpd_req_t *req)`

**SYS_MOD Call:**
```c
esp_err_t err = sys_factory_reset();
if (err == ESP_OK) {
    esp_restart();
}
```

**Response:**
```json
{
  "ok": true,
  "data": null
}
```

---

## 🎛️ DMX APIs

### GET /api/dmx/status

**Frontend Request:**
```typescript
GET /api/dmx/status
```

**Backend Handler:** `api_dmx_status(httpd_req_t *req)`

**SYS_MOD Call:**
```c
const sys_config_t* cfg = sys_get_config();
// Access: cfg->ports[0..3]
// Also need: fps, activity status
```

**Response Mapping:**
```json
{
  "ok": true,
  "data": {
    "ports": [
      {
        "port": 0,
        "universe": cfg->ports[0].universe,
        "enabled": cfg->ports[0].enabled,
        "fps": <calculate from activity>,      // Need activity tracking
        "backend": "RMT",                       // From port config or hardcoded
        "activity_counter": <from sys_get_last_activity()>
      },
      // ... ports 1-3
    ]
  }
}
```

**Note:** Cần tính FPS từ activity timestamps hoặc từ MOD_DMX.

---

### POST /api/dmx/config

**Frontend Request:**
```json
{
  "port": 0,
  "universe": 10,
  "enabled": true,
  "break_us": 200,
  "mab_us": 12
}
```

**Backend Handler:** `api_dmx_config(httpd_req_t *req)`

**Validation:**
```c
// Validate port: 0-3
if (port < 0 || port >= 4) return ESP_ERR_INVALID_ARG;

// Validate universe: 0-32767
if (universe > 32767) return ESP_ERR_INVALID_ARG;

// Validate break_us: 88-500
if (break_us < 88 || break_us > 500) return ESP_ERR_INVALID_ARG;

// Validate mab_us: 8-100
if (mab_us < 8 || mab_us > 100) return ESP_ERR_INVALID_ARG;
```

**SYS_MOD Call:**
```c
dmx_port_cfg_t new_cfg = {0};
new_cfg.enabled = enabled;
new_cfg.universe = universe;
new_cfg.protocol = PROTOCOL_ARTNET; // Default or from request
new_cfg.timing.break_us = break_us;
new_cfg.timing.mab_us = mab_us;
new_cfg.timing.refresh_rate = 40; // Default or from request

esp_err_t err = sys_update_port_cfg(port, &new_cfg);
```

**Response:**
```json
{
  "ok": true,
  "data": null
}
```

---

## 🌐 Network APIs

### GET /api/network/status

**Frontend Request:**
```typescript
GET /api/network/status
```

**Backend Handler:** `api_network_status(httpd_req_t *req)`

**SYS_MOD Call:**
```c
const sys_config_t* cfg = sys_get_config();
// Access: cfg->net (network config)
// Also need: current IP, MAC, link status from mod_net
```

**Response Mapping:**
```json
{
  "ok": true,
  "data": {
    "eth_up": <from mod_net_get_status()>,
    "wifi_up": <from mod_net_get_status()>,
    "eth_ip": <from mod_net_get_ip()>,
    "wifi_ip": <from mod_net_get_ip()>,
    "eth_mac": <from mod_net_get_mac()>,
    "wifi_mac": <from mod_net_get_mac()>,
    "wifi_ssid": cfg->net.wifi_ssid,
    "wifi_rssi": <from mod_net_get_rssi()>
  }
}
```

**Note:** Cần thêm API từ `mod_net` để lấy runtime network status.

---

### POST /api/network/config

**Frontend Request:**
```json
{
  "ethernet": {
    "enabled": true,
    "dhcp": false,
    "ip": "192.168.1.100",
    "netmask": "255.255.255.0",
    "gateway": "192.168.1.1"
  },
  "wifi_sta": {
    "enabled": true,
    "ssid": "MyWiFi",
    "password": "password123"
  }
}
```

**Backend Handler:** `api_network_config(httpd_req_t *req)`

**Validation:**
```c
// Validate IP format (basic check)
// Validate netmask format
// Validate gateway format
// Validate SSID length (max 32)
// Validate password length (max 64)
```

**SYS_MOD Call:**
```c
net_config_t new_net = {0};
strncpy(new_net.ip, ip, sizeof(new_net.ip) - 1);
strncpy(new_net.netmask, netmask, sizeof(new_net.netmask) - 1);
strncpy(new_net.gateway, gateway, sizeof(new_net.gateway) - 1);
new_net.dhcp_enabled = dhcp;

if (wifi_enabled) {
    strncpy(new_net.wifi_ssid, ssid, sizeof(new_net.wifi_ssid) - 1);
    strncpy(new_net.wifi_pass, password, sizeof(new_net.wifi_pass) - 1);
}

esp_err_t err = sys_update_net_cfg(&new_net);
// Note: Network changes require reboot
```

**Response:**
```json
{
  "ok": true,
  "data": {
    "reboot_required": true
  }
}
```

---

## 📁 File APIs (Optional)

### GET /api/file/export

**Frontend Request:**
```typescript
GET /api/file/export
```

**Backend Handler:** `api_file_export(httpd_req_t *req)`

**SYS_MOD Call:**
```c
const sys_config_t* cfg = sys_get_config();
// Serialize to JSON
// Return as application/json
```

**Response:**
```json
{
  "ok": true,
  "data": {
    "device_label": "...",
    "network": { ... },
    "ports": [ ... ],
    "failsafe": { ... }
  }
}
```

---

### POST /api/file/import

**Frontend Request:**
```typescript
POST /api/file/import
Content-Type: application/json
Body: { ... config JSON ... }
```

**Backend Handler:** `api_file_import(httpd_req_t *req)`

**SYS_MOD Call:**
```c
// Parse JSON
// Validate structure
// Apply config via sys_update_* functions
// Save: sys_save_config_now()
```

**Response:**
```json
{
  "ok": true,
  "data": null
}
```

---

## 🔌 WebSocket Events

### Event Subscription

**Backend Setup:**
```c
// Subscribe to SYS_MOD events
esp_event_handler_register(
    SYS_EVENT_BASE,
    ESP_EVENT_ANY_ID,
    ws_event_handler,
    NULL
);
```

### Event → WebSocket Message Mapping

| SYS_MOD Event | WebSocket Type | Data Structure |
|---------------|----------------|----------------|
| `SYS_EVT_CONFIG_APPLIED` | `system.event` | `{code: "CONFIG_APPLIED", level: "info"}` |
| `SYS_EVT_NET_CONNECTED` | `network.link` | `{iface: "eth", status: "up"}` |
| `SYS_EVT_NET_DISCONNECTED` | `network.link` | `{iface: "eth", status: "down"}` |
| `SYS_EVT_DMX_ACTIVE` | `dmx.port_status` | `{port, universe, enabled, fps}` |
| Periodic (1s) | `system.status` | `{cpu, heap, uptime}` |

### Periodic Status Updates

**Backend Task:**
```c
void ws_periodic_task(void *pvParameters) {
    while (1) {
        // Every 1 second
        vTaskDelay(pdMS_TO_TICKS(1000));
        
        // Get system status
        uint32_t uptime = esp_timer_get_time() / 1000000;
        uint32_t free_heap = esp_get_free_heap_size();
        uint8_t cpu_load = <calculate>;
        
        // Broadcast system.status
        ws_broadcast_system_status(uptime, free_heap, cpu_load);
    }
}
```

---

## ⚠️ Missing APIs / Gaps

### 1. Network Status API

**Current:** `mod_net` có thể chưa có API để lấy runtime status.

**Needed:**
```c
// In mod_net.h
typedef struct {
    bool eth_up;
    bool wifi_up;
    char eth_ip[16];
    char wifi_ip[16];
    char eth_mac[18];
    char wifi_mac[18];
    int8_t wifi_rssi;
} net_status_t;

esp_err_t mod_net_get_status(net_status_t *status);
```

### 2. System Status API

**Current:** Cần tính CPU load, uptime.

**Needed:**
```c
// Helper functions
uint32_t sys_get_uptime_seconds(void);
uint8_t sys_get_cpu_load(void); // From FreeRTOS stats
```

### 3. DMX Activity Tracking

**Current:** `sys_get_last_activity()` exists, but need FPS calculation.

**Needed:**
```c
// Calculate FPS from activity timestamps
uint16_t sys_calculate_port_fps(int port_idx);
```

---

## 📝 Implementation Notes

1. **Error Handling:**
   - Tất cả API phải return HTTP status code đúng
   - JSON error format: `{"ok": false, "error": "ERROR_CODE", "message": "..."}`

2. **Thread Safety:**
   - `sys_get_config()` là thread-safe (read-only pointer)
   - `sys_update_*()` functions là thread-safe (mutex protected)

3. **Memory Management:**
   - Tất cả `cJSON` objects phải được `cJSON_Delete()`
   - Không giữ pointer từ `sys_get_config()` sau khi response

4. **Validation:**
   - Validate tất cả input từ frontend
   - Range checks cho port, universe, timing values
   - String length checks cho SSID, password, IP

---

**Last Updated:** 2024
**Status:** Ready for Implementation

