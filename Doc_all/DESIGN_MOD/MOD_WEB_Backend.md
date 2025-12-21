# MOD_WEB.md – Backend Web Server Specification (v1.0)

## 1. Muc dich

`MOD_WEB` la module backend Web Server cho DMX Node, cung cap:

- REST API cho Frontend (React SPA)
- Phuc vu static web (gzip)
- Lop trung gian **non-realtime** ket noi UI ↔ SYS_MOD

MOD_WEB **khong duoc**:
- Truy cap truc tiep MOD_DMX
- Block task realtime
- Xu ly logic dieu khien DMX truc tiep

---

## 2. Vi tri trong he thong

```
[ Frontend (SPA) ]
        │ HTTP / JSON
        ▼
[ MOD_WEB ]  (Core 0, priority thap)
        │ Command / Query
        ▼
[ SYS_MOD ]  (Config, State Machine)
        │
        ▼
[ MOD_DMX ]  (Realtime Core 1)
```

MOD_WEB chi giao tiep voi **SYS_MOD API**.

---

## 3. Nhiem vu chinh

- Khoi tao HTTP server
- Dang ky URI handlers
- Parse / validate JSON
- Chuyen request thanh lenh he thong
- Tra JSON response
- Phuc vu static assets (gzip)

---

## 4. Rang buoc Realtime

- Chay tren Core 0
- Task priority: thap hon SYS_MOD
- Khong cap phat bo nho lon trong ISR / callback
- Moi API handler phai < 50ms
- Khong truy cap RMT / UART / DMA

---

## 5. Cau truc thu muc

```
components/mod_web/
├── mod_web.c                # Entry + init
├── mod_web.h
│
├── web_server.c             # HTTP server setup
├── web_server.h
│
├── api/
│   ├── api_system.c         # /system APIs
│   ├── api_dmx.c            # /dmx APIs
│   ├── api_network.c        # /network APIs
│   └── api_file.c           # import/export
│
├── assets/
│   ├── index_html.c         # index.html.gz
│   ├── app_js.c             # js bundle.gz
│   └── app_css.c            # css.gz
│
└── internal/
    ├── json_utils.c
    ├── auth.c
    └── web_types.h
```

---

## 6. Task Model

### 6.1 Web Task

- Task: `mod_web_task`
- Core: 0
- Priority: thấp (configurable)
- Stack: ~6–8KB

Nhiem vu:
- Khoi dong HTTP server
- Lang nghe request
- Khong vong lap xu ly nang

HTTP callbacks chay trong context cua esp_http_server.

---

## 7. HTTP Server Setup

### 7.1 Cau hinh

```c
httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
cfg.stack_size = 4096;
cfg.max_open_sockets = 4;
cfg.lru_purge_enable = true;
cfg.core_id = 0;
```

---

## 8. API Architecture

### 8.1 Nguyen tac

- `/api/...`
- Stateless
- JSON only
- Validate input truoc khi goi SYS_MOD

### 8.2 Flow xu ly API

```
HTTP Request
   ↓
Parse JSON
   ↓
Validate range / enum
   ↓
Map → sys_cmd_t
   ↓
SYS_MOD enqueue
   ↓
Wait result (timeout)
   ↓
HTTP JSON Response
```

MOD_WEB **khong tu thay doi state he thong**.

---

## 9. API Nhom chi tiet

### 9.1 System APIs

- GET /api/sys/info
- POST /api/sys/reboot
- POST /api/sys/factory

Mapping:
- `sys_get_info()`
- `sys_request_reboot()`
- `sys_request_factory_reset()`

---

### 9.2 DMX APIs

- GET /api/dmx/status
- POST /api/dmx/config

MOD_WEB:
- Chi gui config (enable, universe, protocol)
- Khong tac dong DMX frame

---

### 9.3 Network APIs

- GET /api/network/status
- POST /api/network/config

Network restart duoc thuc hien boi SYS_MOD (async).

---

### 9.4 File APIs

- GET /api/file/export
- POST /api/file/import

Gioi han kich thuoc file (VD: < 8KB).

---

## 10. Static Asset Serving

- File duoc embed dang `const uint8_t[]`
- Gzip bat buoc
- Header:
  - Content-Encoding: gzip
  - Cache-Control: max-age=86400

MOD_WEB khong parse HTML.

---

## 11. Security (Optional)

- Token-based (header Authorization)
- Enable/disable bang compile flag
- Chi cho phep LAN IP

---

## 12. Error Handling

- HTTP 400: invalid input
- HTTP 500: internal error
- JSON error format:

```json
{
  "error": "INVALID_PARAM",
  "detail": "break_us out of range"
}
```

---

## 13. Logging

- ESP_LOGI: request received
- ESP_LOGW: invalid input
- ESP_LOGE: SYS_MOD error

Khong log moi polling request.

---

## 14. Testing & Validation

- curl test tung endpoint
- Stress test polling 1s
- Verify khong drop DMX frame

---

## 15. Mo rong tuong lai

- WebSocket status stream
- User role
- HTTPS (neu MCU du bo nho)

---

**KET LUAN:** MOD_WEB la lop backend an toan, tach biet realtime, lam cau noi chuan giua Web UI va he thong DMX.
