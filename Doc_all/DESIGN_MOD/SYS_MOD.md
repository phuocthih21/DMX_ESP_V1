# SYS_MOD - Chi tiet Thiet ke Trien khai

## 1. Tong quan Thiet ke

Module `SYS_MOD` la trung tam du lieu va dieu phoi cua toan he thong. No quan ly:

- Configuration lifecycle (Load/Save/Validate).
- DMX Data Buffers (512 bytes \* 4 ports).
- Routing table (Universe -> Port mapping).
- OTA Update state machine.
- Snapshot storage/retrieval.

**File nguon:** `components/sys_mod/`

**Dependencies:** `nvs_flash`, `esp_partition`, `esp_ota_ops`, `freertos/semphr.h`

---

## 2. Data Structure Design

### 2.1 Core Configuration Struct

```c
/**
 * @file common/dmx_types.h
 * @brief Dinh nghia cau truc cau hinh chinh
 */

#define SYS_CONFIG_MAGIC 0xDEADBEEF  // De kiem tra NVS hop le
#define SYS_MAX_PORTS 4
#define DMX_UNIVERSE_SIZE 512

/**
 * @brief Cau hinh timing DMX tung port
 * Size: 6 bytes
 */
typedef struct {
    uint16_t break_us;       // 88-500us, Default: 176
    uint16_t mab_us;         // 8-100us, Default: 12
    uint16_t refresh_rate;   // 20-44Hz, Default: 40
} dmx_timing_t;

/**
 * @brief Cau hinh fail-safe
 * Size: 8 bytes
 */
typedef struct {
    uint8_t mode;            // 0=HOLD, 1=BLACKOUT, 2=SNAPSHOT
    uint8_t reserved[3];     // Padding cho alignment
    uint16_t timeout_ms;     // Default: 2000ms
    bool has_snapshot;       // Flag check snapshot co san hay khong
    uint8_t reserved2;       // Padding
} dmx_failsafe_t;

/**
 * @brief Cau hinh tung port DMX
 * Size: 16 bytes (can alignment 4)
 */
typedef struct {
    bool enabled;            // Port co hoat dong khong
    uint8_t protocol;        // 0=ArtNet, 1=sACN
    uint16_t universe;       // Universe ID (0-32767)
    bool rdm_enabled;        // RDM on/off
    uint8_t reserved[3];     // Padding
    dmx_timing_t timing;     // 6 bytes
    uint8_t reserved2[2];    // Padding de tron 16 bytes
} dmx_port_cfg_t;

/**
 * @brief Cau hinh Network
 * Size: ~256 bytes
 */
typedef struct {
    bool dhcp_enabled;
    char ip[16];             // "192.168.1.100"
    char netmask[16];
    char gateway[16];
    char wifi_ssid[32];
    char wifi_pass[64];
    uint8_t wifi_channel;    // 1-13
    int8_t wifi_tx_power;    // 0-78 (dBm * 4)
    char hostname[32];       // mDNS name
    uint8_t reserved[96];    // Mo rong tuong lai
} net_config_t;

/**
 * @brief Cau hinh toan cuc he thong
 * Size: ~512 bytes
 * Layout: Magic | Identity | Network | Ports | Failsafe | Reserved
 */
typedef struct {
    uint32_t magic_number;           // Offset 0: Magic check
    uint32_t version;                // Offset 4: Config version

    char device_label[32];           // Offset 8: Ten hien thi
    uint8_t led_brightness;          // Offset 40: 0-100
    uint8_t reserved1[23];           // Padding

    net_config_t net;                // Offset 64: ~256 bytes

    dmx_port_cfg_t ports[SYS_MAX_PORTS]; // Offset 320: 16*4=64 bytes
    dmx_failsafe_t failsafe;         // Offset 384: 8 bytes

    uint8_t reserved2[120];          // Offset 392: Du phong mo rong

    uint32_t crc32;                  // Offset 508: Checksum (4 bytes fix cuoi)
} sys_config_t;
// TOTAL SIZE: 512 bytes (1 sector Flash)
```

### 2.2 Runtime State Struct

```c
/**
 * @brief Trang thai runtime cua he thong (Chi ton tai trong RAM)
 * Size: ~64 bytes
 */
typedef struct {
    SemaphoreHandle_t config_mutex;  // Bao ve truy cap config

    bool config_dirty;               // Co thay doi chua luu
    int64_t last_change_time;        // Timestamp thay doi cuoi (us)
    esp_timer_handle_t save_timer;   // Timer Lazy Save

    bool ota_in_progress;            // Dang update firmware
    const esp_partition_t* ota_partition; // Partition dang ghi
    esp_ota_handle_t ota_handle;     // Handle OTA

    uint8_t* dmx_buffers[SYS_MAX_PORTS]; // Pointer toi 4 buffer DMX
    int64_t last_activity[SYS_MAX_PORTS]; // Timestamp activity cuoi (Watchdog)

    uint8_t reserved[16];            // Du phong
} sys_state_t;
```

### 2.3 Snapshot Data

```c
/**
 * @brief Du lieu snapshot cho 1 port (Luu trong NVS Blob)
 * Key format: "snap_portX" (X = 0-3)
 * Size: 512 bytes
 */
typedef struct {
    uint8_t dmx_data[DMX_UNIVERSE_SIZE]; // 512 bytes
} sys_snapshot_t;
```

---

## 3. State Machine Design

### 3.1 Config Lifecycle State Machine

```
[UNINITIALIZED]
      |
      | sys_mod_init()
      v
[LOADING_NVS] -- (NVS Empty) ------> [LOAD_DEFAULT]
      |                                    |
      | (NVS Valid)                        |
      v                                    v
[CONFIG_LOADED] <--------------------------+
      |
      | Application running
      v
[RUNTIME] <--(sys_update_xxx())--> [DIRTY]
      ^                               |
      |                               | (5s Timer)
      +------<--(Save Complete)--[SAVING_NVS]
                                      |
                                      | (Save Fail)
                                      v
                                  [ERROR_STATE]
```

**State Transition Table:**

| Current State | Event           | Next State    | Action                      |
| ------------- | --------------- | ------------- | --------------------------- |
| UNINITIALIZED | sys_mod_init()      | LOADING_NVS   | Open NVS, read config       |
| LOADING_NVS   | NVS_OK          | CONFIG_LOADED | Load vao RAM                |
| LOADING_NVS   | NVS_EMPTY       | LOAD_DEFAULT  | Memcpy default config       |
| LOAD_DEFAULT  | -               | CONFIG_LOADED | -                           |
| CONFIG_LOADED | -               | RUNTIME       | Mark ready                  |
| RUNTIME       | sys*update*\*() | DIRTY         | Set dirty=true, Start timer |
| DIRTY         | Timer(5s)       | SAVING_NVS    | nvs_set_blob()              |
| SAVING_NVS    | Save OK         | RUNTIME       | Clear dirty                 |
| SAVING_NVS    | Save Fail       | ERROR_STATE   | Log, retry                  |

### 3.2 OTA Update State Machine

```
[IDLE]
  |
  | sys_ota_begin()
  v
[OTA_STARTED]
  |
  | sys_ota_write() (multiple calls)
  v
[OTA_WRITING]
  |
  | sys_ota_finish()
  v
[OTA_VALIDATING]
  |
  +---(Checksum OK)---> [OTA_SUCCESS] --> esp_restart()
  |
  +---(Checksum Fail)-> [OTA_ABORT] --> Rollback
```

---

## 4. API Implementation Specification

### 4.1 `sys_mod_init()`

**Ham khoi tao chinh cua module.**

```c
/**
 * @brief Khoi tao SYS_MOD
 * @return ESP_OK neu thanh cong
 *
 * Call sequence:
 * 1. Init NVS Flash
 * 2. Create Mutex
 * 3. Load Config (NVS hoac Default)
 * 4. Allocate DMX Buffers
 * 5. Init Timers
 */
esp_err_t sys_mod_init(void) {
    esp_err_t ret;

    // Step 1: Init NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Step 2: Create Mutex
    g_sys_state.config_mutex = xSemaphoreCreateMutex();
    if (!g_sys_state.config_mutex) return ESP_ERR_NO_MEM;

    // Step 3: Load Config
    ret = sys_load_config_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS empty or corrupt, loading default");
        memcpy(&g_sys_config, &DEFAULT_CONFIG, sizeof(sys_config_t));
        g_sys_config.magic_number = SYS_CONFIG_MAGIC;
        sys_save_config_to_nvs(); // Ghi default lan dau
    }

    // Step 4: Allocate Buffers
    for (int i = 0; i < SYS_MAX_PORTS; i++) {
        g_sys_state.dmx_buffers[i] = heap_caps_malloc(DMX_UNIVERSE_SIZE, MALLOC_CAP_DMA);
        if (!g_sys_state.dmx_buffers[i]) {
            ESP_LOGE(TAG, "Failed to allocate DMX buffer %d", i);
            return ESP_ERR_NO_MEM;
        }
        memset(g_sys_state.dmx_buffers[i], 0, DMX_UNIVERSE_SIZE);
        g_sys_state.last_activity[i] = 0;
    }

    // Step 5: Create Lazy Save Timer
    const esp_timer_create_args_t timer_args = {
        .callback = &sys_lazy_save_callback,
        .name = "sys_save"
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_sys_state.save_timer));

    ESP_LOGI(TAG, "SYS_MOD initialized. Device: %s", g_sys_config.device_label);
    return ESP_OK;
}
```

**Pseudo-code cho `sys_load_config_from_nvs()`:**

```
FUNCTION sys_load_config_from_nvs():
    OPEN nvs_handle with namespace "sys_cfg"
    IF open fail:
        RETURN ESP_ERR_NVS_NOT_FOUND

    READ blob key "config" into temp_buffer
    IF read fail OR size != sizeof(sys_config_t):
        CLOSE nvs_handle
        RETURN ESP_ERR_INVALID_SIZE

    CHECK temp_buffer.magic_number == SYS_CONFIG_MAGIC
    IF not match:
        RETURN ESP_ERR_INVALID_CRC

    CALCULATE CRC32 of temp_buffer[0..507]
    IF CRC != temp_buffer.crc32:
        RETURN ESP_ERR_INVALID_CRC

    MEMCPY temp_buffer -> g_sys_config
    CLOSE nvs_handle
    RETURN ESP_OK
END FUNCTION
```

### 4.2 `sys_update_port_cfg()`

**Cap nhat cau hinh port Runtime (Hot-swap).**

```c
/**
 * @brief Cap nhat cau hinh port
 * @param port_idx Port 0-3
 * @param new_cfg Con tro cau hinh moi
 * @return ESP_OK neu hop le
 *
 * Thread-safety: YES (Mutex)
 * Validation: Break(88-500), MAB(8-100), Universe(0-32767)
 */
esp_err_t sys_update_port_cfg(int port_idx, const dmx_port_cfg_t *new_cfg) {
    // Input validation
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return ESP_ERR_INVALID_ARG;
    if (!new_cfg) return ESP_ERR_INVALID_ARG;

    // Range validation
    if (new_cfg->timing.break_us < 88 || new_cfg->timing.break_us > 500) {
        ESP_LOGE(TAG, "Invalid break_us: %d", new_cfg->timing.break_us);
        return ESP_ERR_INVALID_ARG;
    }
    if (new_cfg->timing.mab_us < 8 || new_cfg->timing.mab_us > 100) {
        return ESP_ERR_INVALID_ARG;
    }

    // Critical Section Start
    xSemaphoreTake(g_sys_state.config_mutex, portMAX_DELAY);

    // Apply changes
    memcpy(&g_sys_config.ports[port_idx], new_cfg, sizeof(dmx_port_cfg_t));

    // Mark dirty
    g_sys_state.config_dirty = true;
    g_sys_state.last_change_time = esp_timer_get_time();

    xSemaphoreGive(g_sys_state.config_mutex);
    // Critical Section End

    // Trigger Lazy Save Timer (5 seconds)
    esp_timer_stop(g_sys_state.save_timer);
    esp_timer_start_once(g_sys_state.save_timer, 5000000); // 5s in us

    // Notify DMX Driver (Signal config changed)
    // NOTE: Actual implementation se dung Event Group hoac Semaphore
    // dmx_notify_config_changed(port_idx);

    ESP_LOGI(TAG, "Port %d config updated", port_idx);
    return ESP_OK;
}
```

**Validation Decision Table:**

| Field        | Min | Max   | Default | Action if Invalid      |
| ------------ | --- | ----- | ------- | ---------------------- |
| break_us     | 88  | 500   | 176     | Return ERR_INVALID_ARG |
| mab_us       | 8   | 100   | 12      | Return ERR_INVALID_ARG |
| refresh_rate | 20  | 44    | 40      | Clamp to range         |
| universe     | 0   | 32767 | 0       | Accept (no check)      |

### 4.3 `sys_get_dmx_buffer()`

**Lay pointer toi DMX buffer.**

```c
/**
 * @brief Lay buffer DMX cho port
 * @param port_idx Port 0-3
 * @return Pointer toi mang 512 bytes, hoac NULL neu loi
 *
 * Thread-safety: YES (Read-only pointer, buffer phai duoc bao ve boi consumer)
 * Performance: O(1), khong block
 */
uint8_t* sys_get_dmx_buffer(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return NULL;
    return g_sys_state.dmx_buffers[port_idx];
}
```

**Important Notes:**

- Pointer tra ve la STABLE (khong bi free trong runtime).
- MOD_PROTO ghi vao buffer nay.
- MOD_DMX doc tu buffer nay.
- **Race Condition:** Co the xay ra khi MOD_PROTO dang ghi ma MOD_DMX doc (1 frame bi "tear"). Chap nhan duoc vi mat nguoi khong nhin ra 1 frame loi trong 40Hz.

### 4.4 `sys_notify_activity()`

**Reset Watchdog timer cho port.**

```c
/**
 * @brief Bao co Activity (MOD_PROTO goi moi khi nhan goi tin)
 * @param port_idx Port 0-3
 *
 * Thread-safety: YES (Atomic write 64-bit)
 * Performance: < 1us
 */
void sys_notify_activity(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return;
    g_sys_state.last_activity[port_idx] = esp_timer_get_time();
}
```

**Usage in MOD_PROTO:**

```c
// Trong ham xu ly Art-Net packet
void artnet_handle_dmx_packet(...) {
    // ... parse packet ...

    // Write to buffer
    memcpy(sys_get_dmx_buffer(port_idx), dmx_data, 512);

    // Kick watchdog
    sys_notify_activity(port_idx);
}
```

---

## 5. Memory Management

### 5.1 Static vs Dynamic Allocation

**Static (Global):**

- `g_sys_config` (512 bytes) - Luu trong `.bss` section.
- `g_sys_state` (64 bytes) - Luu trong `.bss`.

**Dynamic (Heap):**

- `dmx_buffers[4]` (512 \* 4 = 2KB) - Cap phat khi `sys_mod_init`, **NEVER FREE**.
- Snapshot data loading: Cap phat tam trong `sys_snapshot_restore`, free ngay sau copy.

**Total RAM Usage:**

- Static: ~576 bytes.
- Heap: ~2KB (permanent).
- Stack: Tuy function call, uoc tinh ~1KB cho deepest call.

### 5.2 Buffer Ownership Rules

| Buffer           | Owner   | Reader                          | Writer          |
| ---------------- | ------- | ------------------------------- | --------------- |
| `g_sys_config`   | SYS_MOD | All modules (read-only pointer) | SYS_MOD (Mutex) |
| `dmx_buffers[i]` | SYS_MOD | MOD_DMX                         | MOD_PROTO       |
| Snapshot NVS     | SYS_MOD | SYS_MOD                         | User via Web    |

**Pointer Safety:**

- Pointer tra ve boi `sys_get_config()` va `sys_get_dmx_buffer()` la READ-ONLY.
- Consumer KHONG DUOC `free()` cac pointer nay.

---

## 6. Timing & Performance Analysis

### 6.1 Critical Path

**Luong Hot-swap cau hinh (User thay doi Break Time):**

```
Web Request -> MOD_WEB parse (2ms)
            -> sys_update_port_cfg()
               - Mutex lock (< 10us)
               - Memcpy (< 1us)
               - Mutex unlock (< 10us)
            -> Notify DMX (via Event, < 50us)
            -> Response OK (1ms)
TOTAL: ~3ms (Acceptable)
```

**Luong doc buffer (MOD_DMX moi frame 25ms):**

```
sys_get_dmx_buffer() -> Array index lookup -> Return pointer
TOTAL: < 100ns (Negligible)
```

### 6.2 Timeout Values va Rationale

| Timeout          | Value         | Rationale                                            |
| ---------------- | ------------- | ---------------------------------------------------- |
| Lazy Save        | 5000ms        | User co the keo slider nhieu lan, chi luu 1 lan cuoi |
| Failsafe Timeout | 2000ms        | Art-Net/sACN co the gap 1-2 goi, 2s = an toan        |
| NVS Write        | 500ms         | Flash write cham, can du buffer                      |
| Mutex Wait       | portMAX_DELAY | Config access critical, khong timeout                |

---

## 7. Snapshot Feature Implementation

### 7.1 Luu Snapshot (Record)

```c
/**
 * @brief Luu snapshot buffer hien tai vao NVS
 * @param port_idx Port can luu
 * @return ESP_OK neu thanh cong
 */
esp_err_t sys_snapshot_record(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open("snapshots", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) return ret;

    char key[16];
    snprintf(key, sizeof(key), "snap_port%d", port_idx);

    // Luu buffer hien tai
    ret = nvs_set_blob(nvs, key, g_sys_state.dmx_buffers[port_idx], DMX_UNIVERSE_SIZE);
    if (ret == ESP_OK) {
        nvs_commit(nvs);
        g_sys_config.failsafe.has_snapshot = true;
        ESP_LOGI(TAG, "Snapshot port %d recorded", port_idx);
    }

    nvs_close(nvs);
    return ret;
}
```

### 7.2 Khoi phuc Snapshot (Restore)

```c
/**
 * @brief Doc snapshot tu NVS ra buffer tam
 * @param port_idx Port can doc
 * @param out_buffer Buffer output (512 bytes)
 * @return ESP_OK neu co snapshot
 */
esp_err_t sys_snapshot_restore(int port_idx, uint8_t* out_buffer) {
    if (!out_buffer) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("snapshots", NVS_READONLY, &nvs);
    if (ret != ESP_OK) return ESP_ERR_NOT_FOUND;

    char key[16];
    snprintf(key, sizeof(key), "snap_port%d", port_idx);

    size_t required_size = DMX_UNIVERSE_SIZE;
    ret = nvs_get_blob(nvs, key, out_buffer, &required_size);

    nvs_close(nvs);
    return ret;
}
```

### 7.3 Su dung trong MOD_DMX

```c
// Trong DMX task loop
void dmx_task_main(void* arg) {
    uint8_t snapshot_buffer[512];
    bool snapshot_loaded = false;

    while (1) {
        int64_t now = esp_timer_get_time();

        for (int i = 0; i < 4; i++) {
            int64_t last = g_sys_state.last_activity[i];
            uint16_t timeout = g_sys_config.failsafe.timeout_ms * 1000;

            if ((now - last) > timeout) {
                // FAILSAFE MODE
                if (g_sys_config.failsafe.mode == FAILSAFE_SNAPSHOT) {
                    if (!snapshot_loaded) {
                        sys_snapshot_restore(i, snapshot_buffer);
                        snapshot_loaded = true;
                    }
                    dmx_send(i, snapshot_buffer);
                } else if (g_sys_config.failsafe.mode == FAILSAFE_BLACKOUT) {
                    memset(snapshot_buffer, 0, 512);
                    dmx_send(i, snapshot_buffer);
                }
                // HOLD mode: Khong lam gi, tu dong gui buffer cu
            } else {
                // NORMAL MODE
                snapshot_loaded = false;
                uint8_t* live_buffer = sys_get_dmx_buffer(i);
                dmx_send(i, live_buffer);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(25)); // 40Hz
    }
}
```

---

## 8. Testing Strategy

### 8.1 Unit Test Cases

**Test Group: Config Validation**

- `test_break_us_min`: Input 87 -> Expect ERR_INVALID_ARG.
- `test_break_us_max`: Input 501 -> Expect ERR_INVALID_ARG.
- `test_break_us_valid`: Input 200 -> Expect ESP_OK.
- `test_mab_us_boundary`: Input 8, 100 -> Expect ESP_OK.

**Test Group: Concurrency**

- `test_mutex_protection`: 2 tasks dong thoi goi `sys_update_port_cfg` -> Config khong bi corrupt.
- `test_race_buffer_access`: MOD_PROTO write + MOD_DMX read -> Khong crash.

**Test Group: Snapshot**

- `test_snapshot_record_restore`: Record -> Restore -> Kiem tra data khop.
- `test_snapshot_not_exist`: Restore chua record -> Tra ve NOT_FOUND.

### 8.2 Integration Test Scenarios

1. **Scenario: Hot-swap Timing**

   - Khoi dong he thong.
   - Thay doi Break time = 300us qua Web.
   - Verify: DMX frame tiep theo co Break = 300us (dung Logic Analyzer).

2. **Scenario: Failsafe Timeout**

   - Chay DMX output binh thuong.
   - Rut day mang (simulate mat tin hieu).
   - Verify: Sau 2s, hien thi snapshot hoac blackout.

3. **Scenario: NVS Persistence**
   - Thay doi config.
   - Cho 6s (qua lazy save).
   - Power cycle.
   - Verify: Config giu nguyen sau reboot.

---

## 9. Luu y Implementation

### 9.1 Flash Wear Leveling

- NVS cua ESP-IDF tu dong lam wear leveling.
- Tranh ghi NVS qua nhieu (moi thay doi nho).
- Lazy Save 5s dam bao user keo slider 100 lan chi ghi Flash 1 lan.

### 9.2 CRC32 Calculation

```c
#include "esp_crc.h"

uint32_t sys_calculate_config_crc(const sys_config_t* cfg) {
    // CRC tinh tu byte 0 den byte 507 (bo qua field crc32 o cuoi)
    return esp_crc32_le(0, (uint8_t*)cfg, sizeof(sys_config_t) - sizeof(uint32_t));
}
```

### 9.3 Default Config Template

```c
static const sys_config_t DEFAULT_CONFIG = {
    .magic_number = SYS_CONFIG_MAGIC,
    .version = 1,
    .device_label = "DMX-Node-V4",
    .led_brightness = 50,

    .net = {
        .dhcp_enabled = true,
        .ip = "192.168.1.100",
        .netmask = "255.255.255.0",
        .gateway = "192.168.1.1",
        .wifi_ssid = "",
        .wifi_pass = "",
        .wifi_channel = 6,
        .wifi_tx_power = 78, // Max
        .hostname = "dmx-node"
    },

    .ports = {
        {.enabled = true, .protocol = 0, .universe = 0, .timing = {176, 12, 40}},
        {.enabled = true, .protocol = 0, .universe = 1, .timing = {176, 12, 40}},
        {.enabled = false, .protocol = 0, .universe = 2, .timing = {176, 12, 40}},
        {.enabled = false, .protocol = 0, .universe = 3, .timing = {176, 12, 40}}
    },

    .failsafe = {
        .mode = 0, // HOLD
        .timeout_ms = 2000,
        .has_snapshot = false
    },

    .crc32 = 0 // Se duoc tinh khi luu
};
```

---

## 10. Ket luan

File thiet ke nay cung cap **tat ca thong tin can thiet** de implement `SYS_MOD` ma khong can phai doan hoac suy luan. Developer chi can:

1. Copy cac struct definitions.
2. Implement cac ham theo pseudo-code.
3. Chay test cases de verify.

**Next Steps:**

- [ ] Implement `sys_config.c` theo Section 4.
- [ ] Implement `sys_nvs.c` cho NVS operations.
- [ ] Implement `sys_snapshot.c` cho Snapshot feature.
- [ ] Viet Unit Tests theo Section 8.
