# SYS_MOD API Contract (Frozen)

## Purpose
SYS_MOD defines the canonical system-level API between MOD_WEB and the embedded system modules (DMX, Network, System). This document is the source-of-truth for the firmware API and the mock server used by frontend developers.

**Source of Truth:** Web.md v1.0, Frontend.md v1.0, MOD_WEB.md v1.0, web_socket_spec_v_1.md

---

## 1. Data Types

```c
typedef enum {
    SYS_OK = 0,
    SYS_ERR_INVALID,
    SYS_ERR_BUSY,
    SYS_ERR_UNSUPPORTED,
} sys_status_t;

typedef enum {
    SYS_CMD_SET_DMX_CONFIG,
    SYS_CMD_SET_NETWORK,
    SYS_CMD_REBOOT,
    SYS_CMD_FACTORY_RESET
} sys_cmd_type_t;
```

Network config (canonical type used by sys_apply_network_config):

```c
typedef struct {
    bool dhcp;
    char ssid[32];
    char psk[64];
    bool eth_enabled;
    // Implementation MUST copy the data; no pointers are retained across API boundary
} network_cfg_t;
```

---

## 2. Snapshot Structs (READ-ONLY)

```c
typedef struct {
    uint32_t uptime;      // seconds
    uint8_t cpu_load;     // 0-100
    uint32_t free_heap;   // bytes
    bool eth_up;
    bool wifi_up;
} sys_system_status_t;

typedef struct {
    uint8_t port;         // DMX port index
    uint16_t universe;    // 0..63999
    bool enabled;
    uint16_t fps;
} sys_dmx_port_status_t;
```

---

## 3. Public API (C prototypes)

```c
sys_status_t sys_get_system_status(sys_system_status_t *out);

// out: array of sys_dmx_port_status_t, max_port: number of entries available
sys_status_t sys_get_dmx_status(sys_dmx_port_status_t *out, size_t max_port);

sys_status_t sys_apply_dmx_config(const sys_dmx_port_status_t *cfg, size_t count);
sys_status_t sys_apply_network_config(const network_cfg_t *cfg);

sys_status_t sys_system_control(sys_cmd_type_t cmd);
```

API semantics (MUST):
- Non-blocking: return quickly — enqueue work, do not block the caller task.
- Copy all input data; never retain pointers from caller.
- Safe to call from the Web task (no blocking waits, no heap allocations in best-effort path).
- Return status codes describing acceptance: SYS_OK (accepted), SYS_ERR_INVALID (validation failed), SYS_ERR_BUSY (queue full), SYS_ERR_UNSUPPORTED.

---

## 4. State Machine (Text diagram)

```
[IDLE]
  ↓ cmd (validate)
[APPLY_PENDING]
  ↓ validation OK
[APPLYING]
  ↓ done
[IDLE]

  ↓ error
[ERROR]
  ↓ recover
[IDLE]
```

Notes:
- APPLYING denotes background processing of the command (communication with subsystems).
- State transitions must emit events (see Event Model).

---

## 5. Event Model

```c
typedef enum {
    SYS_EVT_CONFIG_APPLIED,
    SYS_EVT_LINK_UP,
    SYS_EVT_LINK_DOWN,
    SYS_EVT_ERROR
} sys_event_t;

typedef struct {
    sys_event_t type;
    uint32_t timestamp; // epoch seconds or uptime seconds
    union {
        uint32_t error_code;
        struct { uint8_t port; } config_applied;
    } payload;
} sys_evt_msg_t;
```

Event requirements:
- Lightweight; avoid heap allocation.
- Include timestamp.
- Emitted on important transitions and errors.

---

## 6. Mock Server Requirements

The mock server (Node.js or Python) SHALL:
- Implement the REST endpoints and WS endpoint with payloads matching the firmware structs exactly.
- Emit events over WebSocket matching the event schema.
- Produce realistic mock values (uptime, cpu_load, fps, links).
- Allow frontend to develop without device.

REST endpoints:
- GET /api/sys/info -> { uptime, cpu_load, free_heap, eth_up, wifi_up }
- GET /api/dmx/status -> [ { port, universe, enabled, fps }, ... ]
- GET /api/network/status -> { eth_up, wifi_up, ip_eth?, ip_wifi? }
- WS /ws/status -> event stream of sys_evt_msg_t and periodic snapshots

---

## 7. Compatibility Guarantees

- Changes to internals are allowed, but API and payload shapes are frozen until explicit version bump.
- Mock server payloads must match firmware payloads 1:1.

---

## 8. Files to include in repo

- components/sys_mod/sys_mod.h
- components/sys_mod/sys_mod.c (stubs with TODO)
- components/sys_mod/sys_event.h
- tools/mock_sys_mod/ (nodejs mock server)
- docs_v1/SYS_MOD_API.md (this file)

---

## Appendix: Version

Frozen: v1.0 (2025-12-21)

Change log:
- v1.0 initial freeze
