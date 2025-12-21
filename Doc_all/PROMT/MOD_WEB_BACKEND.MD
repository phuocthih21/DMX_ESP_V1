Đây là **Prompt chuẩn kỹ thuật (Engineering Prompt)** dành cho việc sinh code module **MOD_WEB** (Backend Firmware).

Module này đóng vai trò là cầu nối giữa người dùng (React Frontend) và hệ thống nhúng. Prompt này được thiết kế để giải quyết 3 thách thức lớn nhất của Web Server trên ESP32:

1. **Phục vụ file Gzip tĩnh** (để load trang Web nhanh).
2. **Xử lý API JSON** không gây tràn bộ nhớ (Memory Leak).
3. **WebSocket Realtime** mà không làm treo hệ thống.

# PROMPT SINH CODE MOD_WEB (WEB BACKEND) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Senior Embedded Web Developer** chuyên về `esp_http_server` trên ESP-IDF v5.x.
Nhiệm vụ: Xây dựng **MOD_WEB** - Web Server hiệu năng cao, non-blocking, phục vụ giao diện React (SPA) và cung cấp REST API + WebSocket.

## 2. Ngữ cảnh hệ thống (Context)

- **Hardware:** ESP32-S3 (Core 0 dành riêng cho Wifi & Web Task).
- **Architecture:** - Frontend (React) đã được build thành các file tĩnh (`index.html.gz`, `app.js.gz`...).
  - `MOD_WEB` giao tiếp hoàn toàn với `SYS_MOD` để lấy dữ liệu.
  - **CẤM** truy cập trực tiếp `MOD_DMX` hay phần cứng khác.
- **Protocol:** HTTP/1.1 RESTful API & WebSocket (One-way push).

## 3. Data Contracts (INPUT BẮT BUỘC)

Sử dụng các định nghĩa API và WebSocket từ tài liệu `WEB.md` và `web_socket_spec_v_1.md`.

**Cấu trúc JSON WebSocket Envelope (Bắt buộc):**

```c
// Mọi gói tin WS gửi đi phải bọc trong JSON này
{
  "type": "string",   // VD: "system.status", "dmx.port_status"
  "ts": 12345678,     // Uptime in ms
  "data": { ... }     // Payload chi tiết
}
```

````

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **Static File Serving (Gzip):**

- Web Server phải phục vụ các file đã nén Gzip (`Content-Encoding: gzip`).
- Sử dụng cơ chế **Embedding Binary** của ESP-IDF (`CMakeLists.txt` EMBED_FILES) để nhúng file vào Flash. Không dùng SPIFFS/LittleFS (để tối ưu tốc độ).

2. **Memory Safety (JSON):**

- Sử dụng thư viện `cJSON` (có sẵn trong IDF).
- **BẮT BUỘC** phải `cJSON_Delete()` sau khi sử dụng xong để tránh Memory Leak (Lỗi chết người số 1 trên ESP32 Web).
- Với HTTP Request Body lớn, phải check `content_len` trước khi cấp phát buffer.

3. **Non-blocking WebSocket:**

- WebSocket chỉ dùng để **PUSH** (Server -> Client).
- Sử dụng `httpd_ws_send_frame_async()` để không block main loop.
- Nếu Client không đọc kịp (Socket full) -> **Drop gói tin** ngay lập tức, không chờ đợi (Zero-wait policy).

4. **Kiến trúc:**

- Task Priority: Thấp (để nhường CPU cho DMX).
- Mọi thay đổi config (POST request) phải gọi hàm `sys_update_config()` của `SYS_MOD`.

## 5. Phạm vi code cần sinh

Sinh code component `components/mod_web` theo cấu trúc:

```text
components/mod_web/
├── include/
│   └── mod_web.h         (Public API: web_init, web_stop)
├── mod_web.c             (Khởi tạo Server, Đăng ký URI Handler)
├── web_api.c             (Xử lý các REST API: GET/POST Config)
├── web_static.c          (Phục vụ file index.html.gz, app.js.gz...)
├── web_ws.c              (Quản lý WebSocket session & Push logic)
└── CMakeLists.txt        (Quan trọng: Logic nhúng file binary)

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `web_static.c` & `CMakeLists.txt`

- **CMake:** Phải có đoạn `idf_component_register(... EMBED_FILES "dist/index.html.gz" "dist/style.css.gz" ...)`
- **C Code:** Khai báo `extern const uint8_t index_html_gz_start[]`...
- Handler GET `/` trả về `index_html_gz_start` với header `Content-Encoding: gzip` và `Content-Type: text/html`.

### 6.2 `web_api.c`

Implement các API sau (gọi sang `SYS_MOD`):

- `GET /api/sys/info`: Trả về CPU, Uptime, RAM.
- `GET /api/dmx/status`: Trả về config 4 cổng.
- `POST /api/dmx/config`: Parse JSON -> Validate -> Gọi `sys_update_config()`.
- `POST /api/network/config`: Parse JSON -> Gọi `sys_update_config()`.
- **Lưu ý:** Trả về HTTP 200 OK hoặc 400 Bad Request rõ ràng.

### 6.3 `web_ws.c`

- Đăng ký lắng nghe sự kiện từ `SYS_MOD` (`esp_event_handler_register`).
- Khi nhận sự kiện (VD: `SYS_EVT_DMX_ACTIVE`):

1. Tạo `cJSON` object.
2. Tạo Envelope (type, ts, data).
3. Serialize ra string (`cJSON_PrintUnformatted`).
4. Gửi `httpd_ws_send_frame_async` tới tất cả client đang kết nối.
5. Free JSON và String.

### 6.4 `mod_web.c`

- Hàm `web_init()`:
- Cấu hình `httpd_config_t`.
- Tăng `max_uri_handlers` lên ít nhất 12.
- Tăng Stack Size của Web Task lên 8192 (8KB) để đủ parse JSON.

## 7. Output mong muốn

1. Code C chuẩn ESP-IDF v5.x.
2. Xử lý lỗi `cJSON` parse (check NULL).
3. File `CMakeLists.txt` mẫu giả định các file static nằm trong thư mục `dist/`.
4. Comment nhắc nhở về việc Frontend phải build Gzip trước.
````
