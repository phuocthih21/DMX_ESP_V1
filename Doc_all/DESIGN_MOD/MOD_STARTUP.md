# MOD_STARTUP - Chi tiet Thiet ke Trien khai

## 1. Tong quan Thiet ke

Module `MOD_STARTUP` la "cuc gac cua" (Gatekeeper) cua toan he thong. No chay DAU TIEN khi power on va quyet dinh xem he thong se khoi dong o che do nao: Normal Operation, Rescue Mode (Wifi AP), hay Factory Reset.

**File nguon:** `main/startup/`

**Dependencies:** `nvs_flash`, `driver/gpio`, `esp_timer`

**Entry Point:** `app_main()` trong `main/main.c`

---

## 2. Data Structure Design

### 2.1 Boot Mode Enum

```c
/**
 * @file main/startup/include/startup.h
 * @brief Dinh nghia che do khoi dong
 */

typedef enum {
    BOOT_MODE_NORMAL = 0,      // Khoi dong binh thuong
    BOOT_MODE_RESCUE,          // Che do cuu ho (Wifi AP only)
    BOOT_MODE_FACTORY_RESET    // Reset ve mac dinh
} boot_mode_t;
```

### 2.2 Boot Protection Data

```c
/**
 * @brief Du lieu luu trong NVS de bao ve boot loop
 * NVS Namespace: "boot_cfg"
 * NVS Key: "crash_cnt"
 */

#define BOOT_CRASH_THRESHOLD 3
#define BOOT_STABLE_TIME_MS 60000   // 60 seconds

typedef struct {
    uint8_t crash_counter;         // So lan crash lien tiep
    uint32_t last_boot_time;       // Timestamp boot cuoi (epoch)
    uint32_t magic;                // Magic number check valid
} boot_protect_data_t;

#define BOOT_PROTECT_MAGIC 0xB007CAFE
```

### 2.3 Startup State

```c
/**
 * @brief Trang thai startup (Chi ton tai trong runtime)
 */
typedef struct {
    boot_mode_t mode;              // Che do da quyet dinh
    bool stable_timer_running;
    esp_timer_handle_t stable_timer;

    // Boot reason analysis
    esp_reset_reason_t reset_reason;
    bool was_crash;                // Co phai do crash khong

} startup_state_t;

static startup_state_t g_startup_state = {0};
```

---

## 3. Boot Loop Protection Design

### 3.1 Crash Counter Logic

```c
/**
 * @brief Doc crash counter tu NVS
 * @return Gia tri counter (0 neu chua co)
 */
static uint8_t boot_protect_read_counter(void) {
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open("boot_cfg", NVS_READONLY, &nvs);
    if (ret != ESP_OK) return 0;  // Lan dau tien

    boot_protect_data_t data;
    size_t size = sizeof(data);
    ret = nvs_get_blob(nvs, "crash_cnt", &data, &size);
    nvs_close(nvs);

    if (ret != ESP_OK || data.magic != BOOT_PROTECT_MAGIC) {
        return 0;
    }

    return data.crash_counter;
}

/**
 * @brief Ghi crash counter vao NVS
 */
static void boot_protect_write_counter(uint8_t counter) {
    nvs_handle_t nvs;
    ESP_ERROR_CHECK(nvs_open("boot_cfg", NVS_READWRITE, &nvs));

    boot_protect_data_t data = {
        .crash_counter = counter,
        .last_boot_time = (uint32_t)time(NULL),
        .magic = BOOT_PROTECT_MAGIC
    };

    ESP_ERROR_CHECK(nvs_set_blob(nvs, "crash_cnt", &data, sizeof(data)));
    ESP_ERROR_CHECK(nvs_commit(nvs));
    nvs_close(nvs);
}

/**
 * @brief Tang crash counter (Goi ngay khi boot)
 * @return Counter sau khi tang
 */
static uint8_t boot_protect_increment(void) {
    uint8_t counter = boot_protect_read_counter();
    counter++;
    boot_protect_write_counter(counter);
    ESP_LOGW(TAG, "Crash counter incremented to %d", counter);
    return counter;
}

/**
 * @brief Reset counter ve 0 (Goi khi he thong on dinh)
 */
void startup_mark_as_stable(void) {
    boot_protect_write_counter(0);
    ESP_LOGI(TAG, "System marked as stable, crash counter reset");
}
```

### 3.2 Stable Timer Callback

```c
/**
 * @brief Callback sau 60s de danh dau he thong on dinh
 */
static void stable_timer_callback(void* arg) {
    startup_mark_as_stable();
    g_startup_state.stable_timer_running = false;
}

/**
 * @brief Start timer 60s
 */
static void boot_protect_start_stable_timer(void) {
    const esp_timer_create_args_t timer_args = {
        .callback = &stable_timer_callback,
        .name = "boot_stable"
    };

    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &g_startup_state.stable_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(g_startup_state.stable_timer,
                                         BOOT_STABLE_TIME_MS * 1000));  // Convert to us

    g_startup_state.stable_timer_running = true;
    ESP_LOGI(TAG, "Stable timer started (60s)");
}
```

---

## 4. Button Detection Design

### 4.1 GPIO Button Check

```c
/**
 * @brief Kiem tra nut bam BOOT
 * @return Thoi gian giu nut (ms), 0 neu khong nhan
 *
 * Logic:
 * - Neu giu < 3s: Ignore
 * - Neu giu 3-10s: RESCUE_MODE
 * - Neu giu > 10s: FACTORY_RESET
 */

#define BOOT_BUTTON_PIN GPIO_NUM_0  // Nut BOOT tren ESP32
#define RESCUE_HOLD_MS 3000
#define FACTORY_HOLD_MS 10000

static boot_mode_t check_button_on_boot(void) {
    // Init GPIO as input with pullup
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);

    // Check if button pressed (Active LOW)
    if (gpio_get_level(BOOT_BUTTON_PIN) == 1) {
        // Khong nhan
        return BOOT_MODE_NORMAL;
    }

    ESP_LOGI(TAG, "Boot button detected, waiting...");
    status_set_code(STATUS_BOOTING);  // LED trang tho

    // Cho 3s
    vTaskDelay(pdMS_TO_TICKS(RESCUE_HOLD_MS));

    if (gpio_get_level(BOOT_BUTTON_PIN) == 1) {
        // Da tha trong vong 3s -> Ignore
        ESP_LOGI(TAG, "Button released early, normal boot");
        return BOOT_MODE_NORMAL;
    }

    // Van dang nhan sau 3s -> RESCUE MODE
    ESP_LOGW(TAG, "Rescue mode requested (button 3s)");
    status_set_code(STATUS_NET_AP);  // LED xanh duong

    // Cho them 7s (Tong 10s)
    vTaskDelay(pdMS_TO_TICKS(FACTORY_HOLD_MS - RESCUE_HOLD_MS));

    if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {
        // Van con nhan sau 10s -> FACTORY RESET
        ESP_LOGE(TAG, "Factory reset requested (button 10s)");
        status_set_code(STATUS_ERROR);  // LED do
        return BOOT_MODE_FACTORY_RESET;
    }

    return BOOT_MODE_RESCUE;
}
```

---

## 5. Boot Mode Decision Algorithm

```c
/**
 * @brief Quyet dinh Boot Mode
 * Priority: Button > Crash Counter > Reset Reason
 */
boot_mode_t startup_decide_mode(void) {
    boot_mode_t mode = BOOT_MODE_NORMAL;

    // Step 1: Analyze Reset Reason
    g_startup_state.reset_reason = esp_reset_reason();

    switch (g_startup_state.reset_reason) {
        case ESP_RST_POWERON:
            ESP_LOGI(TAG, "Reset reason: Power On");
            g_startup_state.was_crash = false;
            break;
        case ESP_RST_SW:
            ESP_LOGI(TAG, "Reset reason: Software");
            g_startup_state.was_crash = false;
            break;
        case ESP_RST_PANIC:
        case ESP_RST_INT_WDT:
        case ESP_RST_TASK_WDT:
            ESP_LOGE(TAG, "Reset reason: CRASH (Panic/WDT)");
            g_startup_state.was_crash = true;
            break;
        default:
            ESP_LOGW(TAG, "Reset reason: Other (%d)", g_startup_state.reset_reason);
            break;
    }

    // Step 2: Increment Crash Counter (Always)
    uint8_t crash_count = boot_protect_increment();

    // Step 3: Check Crash Counter Threshold
    if (crash_count >= BOOT_CRASH_THRESHOLD) {
        ESP_LOGE(TAG, "Crash counter exceeded (%d >= %d), entering RESCUE mode",
                 crash_count, BOOT_CRASH_THRESHOLD);
        mode = BOOT_MODE_RESCUE;
        // Note: Khong return, van phai check button (co the user muon Factory Reset)
    }

    // Step 4: Check Physical Button (Highest Priority)
    boot_mode_t button_mode = check_button_on_boot();
    if (button_mode != BOOT_MODE_NORMAL) {
        mode = button_mode;  // Override
    }

    g_startup_state.mode = mode;
    ESP_LOGI(TAG, "Boot mode decided: %s",
             mode == BOOT_MODE_NORMAL ? "NORMAL" :
             mode == BOOT_MODE_RESCUE ? "RESCUE" : "FACTORY_RESET");

    return mode;
}
```

---

## 6. Initialization Sequence by Mode

### 6.1 Normal Mode

```c
/**
 * @brief Khoi dong FULL he thong
 */
static void startup_init_normal_mode(void) {
    ESP_LOGI(TAG, "=== Starting NORMAL mode ===");

    // Level 0: Hardware Abstraction
    ESP_LOGI(TAG, "Level 0: HAL Init");
    // NVS da init o startup_decide_mode

    // Level 1: System Core
    ESP_LOGI(TAG, "Level 1: System Core");
    ESP_ERROR_CHECK(sys_mod_init());

    // Level 2: Connectivity
    ESP_LOGI(TAG, "Level 2: Network");
    const sys_config_t* cfg = sys_get_config();
    ESP_ERROR_CHECK(net_init(&cfg->net));

    // Level 3: Application Layer
    ESP_LOGI(TAG, "Level 3: DMX \u0026 Protocol");
    ESP_ERROR_CHECK(dmx_driver_init(cfg));
    dmx_start();
    ESP_ERROR_CHECK(proto_start());

    // Level 4: User Interface
    ESP_LOGI(TAG, "Level 4: Web \u0026 Status");
    ESP_ERROR_CHECK(web_init());
    ESP_ERROR_CHECK(status_init(STATUS_LED_PIN));

    // Start stable timer
    boot_protect_start_stable_timer();

    ESP_LOGI(TAG, "=== System fully initialized ===");
}
```

### 6.2 Rescue Mode

```c
/**
 * @brief Khoi dong CHE DO CUU HO (Chi Wifi AP + Web)
 */
static void startup_init_rescue_mode(void) {
    ESP_LOGW(TAG, "=== Starting RESCUE mode ===");

    // Chi khoi tao SYS_MOD voi default config
    ESP_ERROR_CHECK(sys_mod_init());

    // Force Wifi AP mode
    net_config_t rescue_net_cfg = {
        .dhcp_enabled = false,
        .ip = "192.168.4.1",
        .netmask = "255.255.0.0",
        .gateway = "192.168.4.1",
        .wifi_ssid = "DMX-Node-Rescue",
        .wifi_pass = "dmxrescue123",
        .wifi_channel = 6
    };

    ESP_LOGW(TAG, "Starting Wifi AP: %s", rescue_net_cfg.wifi_ssid);
    ESP_ERROR_CHECK(net_init(&rescue_net_cfg));
    ESP_ERROR_CHECK(net_switch_mode(NET_MODE_AP));  // Force AP

    // Web Server (Limited, chi cho config/OTA)
    ESP_ERROR_CHECK(web_init());

    // Status LED (Blue blinking)
    ESP_ERROR_CHECK(status_init(STATUS_LED_PIN));
    status_set_code(STATUS_NET_AP);

    // KHONG khoi tao DMX va Protocol

    ESP_LOGW(TAG, "=== Rescue mode ready. Connect to Wifi AP to recover ===");

    // Khong start stable timer (He thong se o rescue cho den khi user fix)
}
```

### 6.3 Factory Reset

```c
/**
 * @brief Thuc hien Factory Reset
 */
static void startup_do_factory_reset(void) {
    ESP_LOGE(TAG, "=== Performing FACTORY RESET ===");

    status_set_code(STATUS_ERROR);  // LED do

    // Step 1: Erase NVS (Xoa CONFIG + SNAPSHOT, GIU BOOT_CFG)
    nvs_flash_erase_partition("nvs");  // Xoa toan bo
    nvs_flash_init();                  // Init lai

    // Re-create boot_cfg namespace voi counter=0
    boot_protect_write_counter(0);

    ESP_LOGE(TAG, "NVS erased, rebooting...");

    // Step 2: Delay 2s cho user thay LED
    vTaskDelay(pdMS_TO_TICKS(2000));

    // Step 3: Reboot
    esp_restart();
}
```

---

## 7. Main Entry Point

```c
/**
 * @file main/main.c
 * @brief ESP-IDF Entry Point
 */

void app_main(void) {
    // Print banner
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  DMX Node V4.0 - Firmware Booting");
    ESP_LOGI(TAG, "  Build: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    // Init NVS (Can thiet cho tat ca module)
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Decide Boot Mode
    boot_mode_t mode = startup_decide_mode();

    // Execute based on mode
    switch (mode) {
        case BOOT_MODE_NORMAL:
            startup_init_normal_mode();
            break;

        case BOOT_MODE_RESCUE:
            startup_init_rescue_mode();
            break;

        case BOOT_MODE_FACTORY_RESET:
            startup_do_factory_reset();
            // Never returns
            break;
    }

    // Main loop (Monitor heap, log stats)
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));  // 10s

        ESP_LOGI(TAG, "Heap free: %d bytes, Min free: %d bytes",
                 esp_get_free_heap_size(),
                 esp_get_minimum_free_heap_size());
    }
}

/**
 * @brief Tra ve boot mode hien tai
 */
boot_mode_t startup_get_mode(void) {
    return g_startup_state.mode;
}
```

---

## 8. Memory \u0026 Performance

### 8.1 Memory Footprint

- **Static RAM**: `g_startup_state` = 32 bytes.
- **NVS Storage**: `boot_protect_data_t` = 12 bytes.
- **Stack**: app_main stack = Default 4KB.

**Total**: ~32 bytes RAM, 12 bytes NVS

### 8.2 Boot Time Analysis

| Phase                     | Time          |
| ------------------------- | ------------- |
| NVS Init                  | ~50ms         |
| Crash Counter Read/Write  | ~20ms         |
| Button Check (if pressed) | 3000-10000ms  |
| Mode Decision             | < 10ms        |
| **Total (no button)**     | **< 100ms** ✓ |

---

## 9. Testing Strategy

### 9.1 Unit Tests

1. **Crash Counter:**

   - Increment 3 lan -> Verify counter = 3.
   - Mark stable -> Verify counter = 0.

2. **Button Timing:**
   - Hold 2s -> Release -> Mode = NORMAL.
   - Hold 5s -> Release -> Mode = RESCUE.
   - Hold 12s -> Mode = FACTORY_RESET.

### 9.2 Integration Tests

1. **Boot Loop Scenario:**

   - Flash firmware loi (cause panic).
   - Verify: Sau 3 lan crash -> Tu dong Rescue mode.

2. **Factory Reset:**
   - Luu config vao NVS.
   - Nhan nut 10s.
   - Reboot -> Verify config ve default.

---

## 10. Luu y Implementation

### 10.1 Brownout Protection

```c
// Trong startup_decide_mode(), neu nguon yeu:
#include "soc/rtc_cntl_reg.h"

// Tam thoi disable Brownout Detector
WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

// Sau khi Wifi init xong, enable lai (Optional)
```

### 10.2 Watchdog Timeout

```c
// Tang Task Watchdog timeout trong qua trinh init
esp_task_wdt_config_t twdt_config = {
    .timeout_ms = 10000,  // 10s (Mac dinh la 5s)
    .trigger_panic = true
};
esp_task_wdt_init(&twdt_config);
```

---

## 11. Ket luan

`MOD_STARTUP` la module quan trong nhat de dam bao he thong khong bao gio bien thanh "brick". Implementation phai:

- Bao ve boot loop bang crash counter.
- Phat hien nut nhan chinh xac (3s/10s).
- Khoi tao dung thu tu module.
- Cung cap Rescue mode de user phuc hoi.

**Next Steps:**

- [ ] Implement `boot_protect.c` voi NVS crash counter.
- [ ] Implement `startup_mgr.c` voi mode decision.
- [ ] Implement `app_main()` voi initialization sequence.
- [ ] Test boot loop recovery tren hardware.
