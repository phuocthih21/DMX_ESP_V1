# PROMPT SINH SYS_MOD API CONTRACT & MOCK SERVER – DMX NODE

## 1. Vai trò của AI

Bạn là **Lead System Architect cho hệ thống nhúng công nghiệp**, chuyên:

- Thiết kế module boundary rõ ràng
- API contract giữa task / module
- State machine & message-driven system
- Mock server để test frontend độc lập firmware

Nhiệm vụ của bạn là **định nghĩa và đóng băng SYS_MOD API contract**, đồng thời sinh **mock implementation** để frontend và backend Web phát triển song song.

---

## 2. Source of Truth (BẮT BUỘC)

Bạn phải tuân thủ **tuyệt đối** các tài liệu sau:

- **Web.md v1.0** – luật Web layer
- **Frontend.md v1.0** – nhu cầu frontend
- **MOD_WEB.md v1.0** – backend Web
- **WebSocket Spec v1.0** – realtime status

⚠️ SYS_MOD là **điểm giao tiếp duy nhất**:

- Web → System
- KHÔNG gọi trực tiếp MOD_DMX
- KHÔNG logic UI trong SYS_MOD

---

## 3. Mục tiêu SYS_MOD

SYS_MOD chịu trách nhiệm:

- Nhận **request chuẩn hoá** từ MOD_WEB
- Validate + chuyển thành **system command**
- Gửi tới module con (DMX / Network / System)
- Trả về **status snapshot**
- Phát event cho WebSocket

SYS_MOD **KHÔNG**:

- Chạy realtime DMX
- Xử lý HTTP / WS
- Render UI

---

## 4. Kiến trúc tổng thể

```
MOD_WEB
 ├── REST Handler
 ├── WS Handler
 └── SYS_MOD API
       ├── sys_get_status()
       ├── sys_apply_config()
       ├── sys_get_dmx_status()
       ├── sys_set_network()
       └── sys_system_control()
```

```
SYS_MOD
 ├── Command Queue
 ├── State Machine
 ├── Snapshot Builder
 └── Event Dispatcher
```

---

## 5. API Contract (BẮT BUỘC ĐỊNH NGHĨA)

### 5.1 Data Types

```c
typedef enum {
    SYS_OK = 0,
    SYS_ERR_INVALID,
    SYS_ERR_BUSY,
    SYS_ERR_UNSUPPORTED,
} sys_status_t;
```

```c
typedef enum {
    SYS_CMD_SET_DMX_CONFIG,
    SYS_CMD_SET_NETWORK,
    SYS_CMD_REBOOT,
    SYS_CMD_FACTORY_RESET
} sys_cmd_type_t;
```

---

### 5.2 Snapshot Struct (READ-ONLY)

```c
typedef struct {
    uint32_t uptime;
    uint8_t cpu_load;
    uint32_t free_heap;
    bool eth_up;
    bool wifi_up;
} sys_system_status_t;
```

```c
typedef struct {
    uint8_t port;
    uint16_t universe;
    bool enabled;
    uint16_t fps;
} sys_dmx_port_status_t;
```

---

### 5.3 Public API

```c
sys_status_t sys_get_system_status(sys_system_status_t *out);
sys_status_t sys_get_dmx_status(sys_dmx_port_status_t *out, size_t max_port);

sys_status_t sys_apply_dmx_config(const sys_dmx_port_status_t *cfg, size_t count);
sys_status_t sys_apply_network_config(const network_cfg_t *cfg);

sys_status_t sys_system_control(sys_cmd_type_t cmd);
```

⚠️ API:

- **Non-blocking**
- Copy data, không giữ pointer
- Safe cho Web task

---

## 6. State Machine SYS_MOD (BẮT BUỘC)

```
[IDLE]
  ↓ cmd
[APPLY_PENDING]
  ↓ validate ok
[APPLYING]
  ↓ done
[IDLE]

  ↓ error
[ERROR]
  ↓ recover
[IDLE]
```

---

## 7. Event Model

SYS_MOD phát event cho WS:

```c
typedef enum {
    SYS_EVT_CONFIG_APPLIED,
    SYS_EVT_LINK_UP,
    SYS_EVT_LINK_DOWN,
    SYS_EVT_ERROR
} sys_event_t;
```

Event phải:

- Nhẹ
- Không cấp phát heap
- Có timestamp

---

## 8. Mock Server (BẮT BUỘC)

Bạn phải sinh **mock SYS_MOD** chạy được trên:

- Node.js **hoặc**
- Python

Mock server phải:

- Implement **đúng API contract**
- Sinh dữ liệu giả (CPU, DMX FPS, link)
- Emit event & WS message
- Cho phép frontend chạy **không cần ESP32**

---

## 9. Mock REST + WS

Mock phải expose:

- `/api/sys/info`
- `/api/dmx/status`
- `/api/network/status`
- `/ws/status`

Payload **giống 100% firmware thật**

---

## 10. Output yêu cầu

### A. Tài liệu

- SYS_MOD_API.md (contract đóng băng)
- State machine diagram (text-based)

### B. Firmware-side

- sys_mod.h
- sys_mod.c (stub + TODO)
- event interface

### C. Mock Server

- Source code
- Run instruction
- Sample output

⚠️ Không pseudo-code
⚠️ Interface phải compile được
⚠️ Mock chạy được thật

---

## 11. Nguyên tắc thiết kế

- SYS_MOD = “luật giao thông”
- Không logic trùng lặp
- Thay module con không đổi API
- Debug được bằng log

---

## KẾT LUẬN PROMPT

> “Nếu SYS_MOD đúng,
> Web – DMX – Network **không bao giờ chạm nhau trực tiếp**.”
