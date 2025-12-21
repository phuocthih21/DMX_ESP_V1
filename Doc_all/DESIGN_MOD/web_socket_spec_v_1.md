# WEBSOCKET SPECIFICATION – DMX NODE (v1.0)

---

## 1. Mục đích

Tài liệu này định nghĩa **chuẩn giao tiếp WebSocket realtime** giữa Firmware và Web UI trong dự án **DMX Node**.

WebSocket dùng để:
- Đẩy **trạng thái realtime nhẹ** (status / event)
- Giảm polling REST
- Hỗ trợ giám sát trong commissioning

WebSocket **KHÔNG** dùng để:
- Gửi command cấu hình
- Điều khiển realtime DMX
- Thay thế REST API

---

## 2. Vị trí trong kiến trúc

```
SYS_MOD  ──event──▶  MOD_WEB  ──WS──▶  Frontend SPA
```

- SYS_MOD: nguồn phát event hệ thống
- MOD_WEB: adapter WS, không xử lý logic
- Frontend: chỉ đọc, không phản hồi

---

## 3. Nguyên tắc thiết kế

- One-way push (server → client)
- Stateless
- JSON text frame
- Không ACK
- Không retry logic
- Rate-limited

WebSocket **không ảnh hưởng realtime DMX**.

---

## 4. Endpoint

```
ws://<device-ip>/ws/status
```

- Không authentication phức tạp
- Chỉ cho phép LAN

---

## 5. Message Envelope (chuẩn bắt buộc)

Mọi message đều dùng envelope thống nhất:

```json
{
  "type": "string",
  "ts": 12345678,
  "data": { }
}
```

### Field

| Field | Mô tả |
|------|------|
| type | Loại message |
| ts | Timestamp (ms từ boot) |
| data | Payload theo type |

---

## 6. Event Types

### 6.1 system.status

Định kỳ gửi trạng thái hệ thống.

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

Rate: **1 Hz**

---

### 6.2 dmx.port_status

Trạng thái DMX từng port.

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

Rate: **2–5 Hz / port**

---

### 6.3 network.link

Thông báo trạng thái link mạng.

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

Event-based

---

### 6.4 system.event

Event hệ thống không định kỳ.

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

---

## 7. Mapping từ SYS_MOD Event

| SYS_EVT | WS type |
|-------|--------|
| SYS_EVT_CONFIG_APPLIED | system.event |
| SYS_EVT_LINK_UP | network.link (up) |
| SYS_EVT_LINK_DOWN | network.link (down) |
| SYS_EVT_ERROR | system.event |

---

## 8. Rate Limit & QoS

- Tổng WS message ≤ **20 msg/sec**
- Gộp event nếu overload
- Bỏ frame cũ nếu queue đầy

Không block task SYS_MOD.

---

## 9. Client Behavior (Frontend)

- Không gửi message lên server
- Tự reconnect khi mất WS
- Nếu WS mất → fallback REST polling

---

## 10. Error Handling

- WS disconnect không ảnh hưởng hệ thống
- Không guarantee delivery
- Không resend

---

## 11. Versioning

- Version hiện tại: **v1.0**
- Thay đổi schema → bump version

---

## 12. Phạm vi mở rộng tương lai

- Per-universe summary
- sACN priority monitor
- Merge engine status

---

**END OF DOCUMENT – WebSocket Spec v1.0**

