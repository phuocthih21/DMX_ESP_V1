# PROMPT SINH WEBSOCKET REALTIME – DMX NODE

## 1. Vai trò của AI

Bạn là **Senior Embedded + Frontend System Engineer**, chuyên:

- WebSocket trên hệ thống nhúng (ESP32, esp-idf)
- Realtime telemetry cho thiết bị công nghiệp
- Thiết kế protocol nhẹ, ổn định, dễ debug
- Đồng bộ frontend React SPA với firmware

Nhiệm vụ của bạn là **thiết kế & sinh hệ thống WebSocket realtime status** cho DMX Node.

---

## 2. Source of Truth (BẮT BUỘC)

Bạn được cung cấp các tài liệu:

- **Web.md (v1.0)** – API, ranh giới backend
- **Frontend.md (v1.0)** – kiến trúc SPA
- **Frontend React code (đã sinh)** – polling hiện tại

⚠️ WebSocket:

- **CHỈ BỔ SUNG**, không phá API REST
- REST vẫn là fallback

---

## 3. Mục tiêu WebSocket

WebSocket dùng cho:

- Trạng thái realtime
- Telemetry nhẹ
- Event push

KHÔNG dùng cho:

- Config write
- File upload
- OTA

---

## 4. Kiến trúc tổng thể

```
ESP32 Firmware
 ├── REST API (config / control)
 ├── WS Server (/ws/status)
 │    ├── heartbeat
 │    ├── telemetry
 │    └── event push
 └── DMX Engine
      └── publish status
```

```
React SPA
 ├── REST (initial load)
 ├── WebSocket client
 │    ├── auto reconnect
 │    ├── message dispatcher
 │    └── store updater
 └── Zustand stores
```

---

## 5. WebSocket Endpoint

- URL: `/ws/status`
- Protocol: WebSocket thuần (RFC6455)
- JSON message
- Không binary frame
- Không auth phức tạp (same-origin)

---

## 6. Message Model (BẮT BUỘC)

### 6.1 Envelope chung

```json
{
  "type": "telemetry | event | heartbeat | error",
  "ts": 123456789,
  "payload": {}
}
```

---

### 6.2 Telemetry message

```json
{
  "type": "telemetry",
  "ts": 123456789,
  "payload": {
    "cpu": 34,
    "ram": 61200,
    "uptime": 123456,
    "dmx": [
      { "port": 0, "fps": 40, "active": true },
      { "port": 1, "fps": 0, "active": false }
    ],
    "network": {
      "eth": true,
      "wifi": false
    }
  }
}
```

- Chu kỳ: 500ms – 1s
- Payload nhỏ, ổn định

---

### 6.3 Event message

```json
{
  "type": "event",
  "ts": 123456789,
  "payload": {
    "code": "LINK_DOWN",
    "detail": "Ethernet disconnected"
  }
}
```

---

### 6.4 Heartbeat

```json
{
  "type": "heartbeat",
  "ts": 123456789
}
```

- 2–5s
- Frontend dùng để detect disconnect

---

## 7. Firmware requirements (ESP-IDF)

Bạn phải sinh:

- WS server handler
- Client session management
- Periodic publish task
- Safe shutdown khi client disconnect
- Không block loop
- Không malloc liên tục

### BẮT BUỘC

- esp_http_server WebSocket API
- Task riêng cho publish
- Ringbuffer / snapshot struct
- Không đọc trực tiếp từ ISR

---

## 8. Frontend requirements (React)

Bạn phải sinh:

### 8.1 WebSocket Client Module

```
src/ws/
├── wsClient.ts
├── wsTypes.ts
└── wsDispatcher.ts
```

- Auto reconnect (exponential backoff)
- Timeout detect
- Clean close on unload

---

### 8.2 Store integration

- WebSocket cập nhật:

  - systemStore
  - dmxStore
  - networkStore

- Không setState trực tiếp trong component
- Fallback polling nếu WS chết

---

## 9. State Machine (BẮT BUỘC)

### 9.1 Client WS state machine

```
[IDLE]
  ↓ connect
[CONNECTING]
  ↓ open
[CONNECTED]
  ↓ timeout / error
[RECONNECT_WAIT]
  ↓ retry
[CONNECTING]
```

---

### 9.2 Firmware WS lifecycle

```
[NO_CLIENT]
  ↓ ws_open
[CLIENT_CONNECTED]
  ↓ send heartbeat/telemetry
[CLIENT_CONNECTED]
  ↓ ws_close
[NO_CLIENT]
```

---

## 10. Error handling

- JSON parse error → ignore frame
- WS overload → drop frame
- Client slow → skip send
- Không crash firmware

---

## 11. Output yêu cầu

Bạn phải sinh:

### Firmware

- C header + source
- WS handler
- Publish task
- Struct snapshot

### Frontend

- wsClient.ts
- wsDispatcher.ts
- Store integration
- Remove polling logic (hoặc mark fallback)

⚠️ Không pseudo-code
⚠️ Code build được
⚠️ Comment rõ

---

## 12. Nguyên tắc thiết kế

- Realtime nhưng **an toàn**
- Mất WS không sập UI
- Debug được bằng log + browser devtools
- Ưu tiên ổn định hơn độ mượt

---

## KẾT LUẬN PROMPT

> “WebSocket này dùng để **giám sát thiết bị công nghiệp**,
> không phải chat app, không phải streaming.”
