# MOD_STATUS - Chi tiet Thiet ke Trien khai

## 1. Tong quan Thiet ke

Module `MOD_STATUS` chiu trach nhiem phan hoi trang thai he thong bang thi giac (Visual Feedback) thong qua LED WS2812B. No la module don gian nhung quan trong cho UX (User Experience), giup ky thuat vien nhan dien ngay trang thai hoac loi cua thiet bi ma khong can ket noi may tinh.

**File nguon:** `components/mod_status/`

**Dependencies:** `driver/rmt`, `esp_timer`

**Hardware:** 1x LED WS2812B (Neopixel) @ GPIO 28

---

## 2. Data Structure Design

### 2.1 Status Code Enum

```c
/**
 * @file common/dmx_types.h
 * @brief Dinh nghia cac ma trang thai he thong
 */

typedef enum {
    STATUS_BOOTING = 0,      // Dang khoi dong
    STATUS_NET_ETH,          // Ket noi Ethernet thanh cong
    STATUS_NET_WIFI,         // Ket noi Wifi thanh cong
    STATUS_NET_AP,           // Che do Wifi AP (Rescue)
    STATUS_NO_NET,           // Mat mang
    STATUS_DMX_WARN,         // Canh bao DMX (Failsafe active)
    STATUS_OTA,              // Dang update firmware
    STATUS_ERROR,            // Loi nghiem trong
    STATUS_IDENTIFY,         // Identify device (tu Web)
    STATUS_MAX               // Sentinel
} status_code_t;
```

### 2.2 Color Definition

```c
/**
 * @brief Dinh nghia mau RGB cho moi trang thai
 * Size: 3 bytes
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

// Bang mau cho tung trang thai
static const rgb_color_t STATUS_COLORS[] = {
    [STATUS_BOOTING]   = {255, 255, 255},  // Trang (White)
    [STATUS_NET_ETH]   = {0, 255, 0},      // Xanh la (Green)
    [STATUS_NET_WIFI]  = {0, 255, 255},    // Xanh lo (Cyan)
    [STATUS_NET_AP]    = {0, 0, 255},      // Xanh duong (Blue)
    [STATUS_NO_NET]    = {255, 255, 0},    // Vang (Yellow)
    [STATUS_DMX_WARN]  = {255, 128, 0},    // Cam (Orange)
    [STATUS_OTA]       = {255, 0, 255},    // Tim (Purple/Magenta)
    [STATUS_ERROR]     = {255, 0, 0},      // Do (Red)
    [STATUS_IDENTIFY]  = {255, 255, 255}   // Trang (White)
};
```

### 2.3 Pattern Type Enum

```c
/**
 * @brief Dinh nghia cac kieu hieu ung nhay
 */
typedef enum {
    PATTERN_SOLID = 0,       // Sang co dinh
    PATTERN_BREATHING,       // Tho (Sin wave)
    PATTERN_BLINK_SLOW,      // Nhay cham 1Hz
    PATTERN_BLINK_FAST,      // Nhay nhanh 4Hz
    PATTERN_DOUBLE_BLINK,    // Nhay kep (2 lan roi nghi)
    PATTERN_STROBE,          // Chop nhanh 10Hz
    PATTERN_MAX
} status_pattern_t;

// Bang pattern cho tung trang thai
static const status_pattern_t STATUS_PATTERNS[] = {
    [STATUS_BOOTING]   = PATTERN_BREATHING,
    [STATUS_NET_ETH]   = PATTERN_SOLID,
    [STATUS_NET_WIFI]  = PATTERN_SOLID,
    [STATUS_NET_AP]    = PATTERN_DOUBLE_BLINK,
    [STATUS_NO_NET]    = PATTERN_BLINK_SLOW,
    [STATUS_DMX_WARN]  = PATTERN_BLINK_FAST,
    [STATUS_OTA]       = PATTERN_STROBE,
    [STATUS_ERROR]     = PATTERN_BLINK_SLOW,
    [STATUS_IDENTIFY]  = PATTERN_STROBE
};
```

### 2.4 Module State

```c
/**
 * @brief Trang thai runtime cua module
 * Size: ~64 bytes
 */
typedef struct {
    status_code_t current_code;      // Ma trang thai hien tai
    status_code_t pending_code;      // Ma trang thai pending (cho transition)

    uint8_t global_brightness;       // Do sang toan cuc 0-100

    rmt_channel_handle_t rmt_channel; // Handle RMT channel
    rmt_encoder_handle_t led_encoder; // Encoder WS2812B

    bool is_transitioning;           // Dang chuyen trang thai
    float transition_progress;       // 0.0 -> 1.0

    int64_t pattern_start_time;      // Timestamp bat dau pattern (us)
    uint32_t pattern_cycle_count;    // So chu ky da chay

    TaskHandle_t task_handle;        // Handle task LED

    uint8_t reserved[16];            // Du phong
} status_state_t;

static status_state_t g_status_state = {0};
```

---

## 3. State Machine Design

### 3.1 Status Code Priority

Khi co nhieu module cung yeu cau set status code, he thong phai uu tien theo bang sau:

```
Priority Table (0 = Cao nhat):
0: STATUS_OTA          (Bat buoc hien thi, khong duoc ghi de)
1: STATUS_ERROR        (Loi nghiem trong)
2: STATUS_IDENTIFY     (User request tu Web)
3: STATUS_DMX_WARN     (Canh bao Failsafe)
4: STATUS_NO_NET       (Mat mang)
5: STATUS_BOOTING      (Khoi dong)
6: STATUS_NET_ETH      (Ket noi Ethernet)
7: STATUS_NET_WIFI     (Ket noi Wifi)
8: STATUS_NET_AP       (Rescue mode)
```

**Logic Overriding:**

```c
/**
 * @brief Set status code voi priority check
 * @param new_code Ma trang thai moi
 */
void status_set_code(status_code_t new_code) {
    // OTA mode la highest priority, khong cho override
    if (g_status_state.current_code == STATUS_OTA && new_code != STATUS_OTA) {
        ESP_LOGW(TAG, "Cannot override OTA status");
        return;
    }

    // Kiem tra priority
    uint8_t current_priority = get_priority(g_status_state.current_code);
    uint8_t new_priority = get_priority(new_code);

    if (new_priority <= current_priority) {
        // Accept change
        g_status_state.pending_code = new_code;
        g_status_state.is_transitioning = true;
        g_status_state.transition_progress = 0.0f;
        ESP_LOGI(TAG, "Status changed: %d -> %d", g_status_state.current_code, new_code);
    } else {
        ESP_LOGD(TAG, "Status change ignored (lower priority)");
    }
}

static uint8_t get_priority(status_code_t code) {
    const uint8_t priority_map[] = {
        [STATUS_OTA] = 0,
        [STATUS_ERROR] = 1,
        [STATUS_IDENTIFY] = 2,
        [STATUS_DMX_WARN] = 3,
        [STATUS_NO_NET] = 4,
        [STATUS_BOOTING] = 5,
        [STATUS_NET_ETH] = 6,
        [STATUS_NET_WIFI] = 7,
        [STATUS_NET_AP] = 8
    };
    return priority_map[code];
}
```

### 3.2 Transition State Machine

```
[STABLE]
   |
   | status_set_code() voi priority cao hon
   v
[FADE_OUT] (Giam do sang ve 0 trong 200ms)
   |
   | brightness == 0
   v
[SWITCH_COLOR] (Doi mau va pattern)
   |
   v
[FADE_IN] (Tang do sang len target trong 200ms)
   |
   | brightness == target
   v
[STABLE]
```

---

## 4. RMT \u0026 WS2812B Implementation

### 4.1 WS2812B Protocol Timing

WS2812B su dung protocol 1-wire de nhan du lieu RGB:

```
Bit "0":
  T0H: 0.35us (High)
  T0L: 0.8us  (Low)
  Total: 1.15us

Bit "1":
  T1H: 0.7us  (High)
  T1L: 0.6us  (Low)
  Total: 1.3us

Reset: >50us Low
```

### 4.2 RMT Encoding

RMT chay o 10MHz (0.1us/tick):

```c
/**
 * @brief Encode 1 byte RGB thanh RMT items (24 bits = 24*2 = 48 RMT items)
 */
static void encode_rgb_to_rmt(uint8_t r, uint8_t g, uint8_t b, rmt_symbol_word_t* items) {
    // WS2812B format: GRB (khong phai RGB!)
    uint32_t grb = ((uint32_t)g << 16) | ((uint32_t)r << 8) | b;

    for (int i = 0; i < 24; i++) {
        uint8_t bit = (grb >> (23 - i)) & 0x01;

        if (bit) {
            // Bit "1": 0.7us High, 0.6us Low
            items[i].level0 = 1;
            items[i].duration0 = 7;  // 0.7us
            items[i].level1 = 0;
            items[i].duration1 = 6;  // 0.6us
        } else {
            // Bit "0": 0.35us High, 0.8us Low
            items[i].level0 = 1;
            items[i].duration0 = 3;  // 0.3us (gan dung)
            items[i].level1 = 0;
            items[i].duration1 = 9;  // 0.9us (gan dung)
        }
    }
}
```

### 4.3 RMT Channel Allocation

```c
/**
 * @brief Khoi tao RMT cho LED
 */
esp_err_t status_init(int gpio_pin) {
    // Cau hinh RMT Channel (Dung channel 7 de tranh xung dot voi DMX)
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = gpio_pin,
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10000000,  // 10MHz = 0.1us/tick
        .mem_block_symbols = 64,
        .trans_queue_depth = 4,
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &g_status_state.rmt_channel));

    // Tao encoder (Co the dung led_strip component hoac custom encoder)
    // ... (Detail implementation using ESP-IDF led_strip component)

    ESP_ERROR_CHECK(rmt_enable(g_status_state.rmt_channel));

    // Tao task LED loop
    xTaskCreatePinnedToCore(
        status_task_loop,
        "status_led",
        2048,                    // Stack size
        NULL,
        1,                       // Priority thap nhat (IDLE + 1)
        &g_status_state.task_handle,
        0                        // Core 0 (vi DMX chay Core 1)
    );

    ESP_LOGI(TAG, "Status LED initialized on GPIO %d", gpio_pin);
    return ESP_OK;
}
```

---

## 5. Pattern Generation Algorithms

### 5.1 Breathing Effect (Exponential Sin Wave)

```c
/**
 * @brief Tinh do sang cho hieu ung "tho"
 * @param phase Goc pha 0 -> 2*PI
 * @return Brightness 0-255
 */
static uint8_t calculate_breathing_brightness(float phase) {
    // Cong thuc: brightness = (exp(sin(t)) - 0.36787944) * 108.0
    // Cho ket qua tu ~0 -> ~255
    float val = exp(sin(phase)) - 0.36787944f;
    val *= 108.0f;

    // Clamp
    if (val < 0) val = 0;
    if (val > 255) val = 255;

    return (uint8_t)val;
}
```

**Usage in Task Loop:**

```c
// Trong status_task_loop
static float breathing_phase = 0.0f;

while (1) {
    if (STATUS_PATTERNS[current_code] == PATTERN_BREATHING) {
        uint8_t breathe_val = calculate_breathing_brightness(breathing_phase);

        // Apply vao mau goc
        rgb_color_t color = STATUS_COLORS[current_code];
        color.r = (color.r * breathe_val) / 255;
        color.g = (color.g * breathe_val) / 255;
        color.b = (color.b * breathe_val) / 255;

        set_led_color(color);

        breathing_phase += 0.05f;  // Tang cham de muot
        if (breathing_phase > 2 * M_PI) breathing_phase = 0;
    }

    vTaskDelay(pdMS_TO_TICKS(20));  // 50Hz update
}
```

### 5.2 Blink Pattern

```c
/**
 * @brief Tinh trang thai ON/OFF cho blink
 * @param frequency_hz Tan so nhay (Hz)
 * @return true = ON, false = OFF
 */
static bool calculate_blink_state(float frequency_hz) {
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - g_status_state.pattern_start_time;

    // Tinh chu ky
    float period_us = 1000000.0f / frequency_hz;
    float phase = fmod((float)elapsed, period_us) / period_us;

    // ON trong 50% chu ky dau
    return (phase < 0.5f);
}
```

### 5.3 Double Blink Pattern

```c
/**
 * @brief Double blink: Nhay 2 lan nhanh, roi nghi
 * Sequence: ON(100ms) OFF(100ms) ON(100ms) OFF(700ms)
 */
static bool calculate_double_blink_state(void) {
    int64_t now = esp_timer_get_time();
    int64_t elapsed = now - g_status_state.pattern_start_time;

    // Chu ky toan bo: 1000ms = 1s
    int64_t cycle_time = elapsed % 1000000;

    if (cycle_time < 100000) return true;        // 0-100ms: ON
    else if (cycle_time < 200000) return false;  // 100-200ms: OFF
    else if (cycle_time < 300000) return true;   // 200-300ms: ON
    else return false;                           // 300-1000ms: OFF
}
```

---

## 6. Brightness Control

### 6.1 Global Brightness Application

```c
/**
 * @brief Ap dung global brightness vao mau
 * @param color Mau nguon
 * @param brightness 0-100
 * @return Mau sau khi dim
 */
static rgb_color_t apply_brightness(rgb_color_t color, uint8_t brightness) {
    rgb_color_t result;
    result.r = (color.r * brightness) / 100;
    result.g = (color.g * brightness) / 100;
    result.b = (color.b * brightness) / 100;
    return result;
}

/**
 * @brief Set do sang toan cuc (Goi tu Web)
 * @param brightness 0-100
 */
void status_set_brightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    g_status_state.global_brightness = brightness;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
}
```

### 6.2 Transition Fade Effect

```c
/**
 * @brief Xu ly fade in/out khi chuyen trang thai
 * @param delta_time Thoi gian tu frame truoc (ms)
 */
static void process_transition(uint32_t delta_time) {
    if (!g_status_state.is_transitioning) return;

    const float TRANSITION_DURATION_MS = 200.0f;  // 200ms

    g_status_state.transition_progress += (float)delta_time / TRANSITION_DURATION_MS;

    if (g_status_state.transition_progress >= 1.0f) {
        // Transition hoan tat
        g_status_state.current_code = g_status_state.pending_code;
        g_status_state.is_transitioning = false;
        g_status_state.transition_progress = 1.0f;
        g_status_state.pattern_start_time = esp_timer_get_time();
    }

    // Tinh fade alpha
    float fade_alpha;
    if (g_status_state.transition_progress < 0.5f) {
        // Fade out (0.5 -> 0)
        fade_alpha = 1.0f - (g_status_state.transition_progress * 2.0f);
    } else {
        // Fade in (0 -> 0.5)
        fade_alpha = (g_status_state.transition_progress - 0.5f) * 2.0f;
    }

    // Apply fade alpha vao brightness
    uint8_t effective_brightness = (uint8_t)(g_status_state.global_brightness * fade_alpha);
    // ... (Update LED voi brightness nay)
}
```

---

## 7. Main Task Loop

```c
/**
 * @brief Task chinh dieu khien LED
 */
static void status_task_loop(void* arg) {
    int64_t last_time = esp_timer_get_time();

    while (1) {
        int64_t now = esp_timer_get_time();
        uint32_t delta_ms = (now - last_time) / 1000;
        last_time = now;

        // Step 1: Xu ly transition
        process_transition(delta_ms);

        // Step 2: Lay mau va pattern hien tai
        status_code_t code = g_status_state.is_transitioning ?
                             g_status_state.pending_code :
                             g_status_state.current_code;

        rgb_color_t base_color = STATUS_COLORS[code];
        status_pattern_t pattern = STATUS_PATTERNS[code];

        // Step 3: Tinh mau cuoi cung dua tren pattern
        rgb_color_t final_color = base_color;

        switch (pattern) {
            case PATTERN_SOLID:
                // Khong thay doi
                break;

            case PATTERN_BREATHING:
                float phase = (float)(now - g_status_state.pattern_start_time) * 0.000001f * 2.0f;
                uint8_t breath = calculate_breathing_brightness(phase);
                final_color.r = (base_color.r * breath) / 255;
                final_color.g = (base_color.g * breath) / 255;
                final_color.b = (base_color.b * breath) / 255;
                break;

            case PATTERN_BLINK_SLOW:
                if (!calculate_blink_state(1.0f)) {
                    final_color = (rgb_color_t){0, 0, 0};
                }
                break;

            case PATTERN_BLINK_FAST:
                if (!calculate_blink_state(4.0f)) {
                    final_color = (rgb_color_t){0, 0, 0};
                }
                break;

            case PATTERN_DOUBLE_BLINK:
                if (!calculate_double_blink_state()) {
                    final_color = (rgb_color_t){0, 0, 0};
                }
                break;

            case PATTERN_STROBE:
                if (!calculate_blink_state(10.0f)) {
                    final_color = (rgb_color_t){0, 0, 0};
                }
                break;
        }

        // Step 4: Apply global brightness
        final_color = apply_brightness(final_color, g_status_state.global_brightness);

        // Step 5: Gui xuong LED thong qua RMT
        set_led_color(final_color);

        // Step 6: Delay (50Hz update rate)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief Gui mau xuong LED WS2812B
 */
static void set_led_color(rgb_color_t color) {
    // Su dung led_strip component cua ESP-IDF
    // led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b);
    // led_strip_refresh(led_strip);

    // Hoac dung RMT truc tiep
    rmt_symbol_word_t items[24];
    encode_rgb_to_rmt(color.r, color.g, color.b, items);

    rmt_transmit_config_t tx_config = {
        .loop_count = 0  // No loop
    };

    rmt_transmit(g_status_state.rmt_channel, g_status_state.led_encoder,
                 items, sizeof(items), &tx_config);
}
```

---

## 8. Memory \u0026 Performance

### 8.1 Memory Footprint

- **Static RAM**: `g_status_state` = 64 bytes.
- **Stack**: Task stack = 2KB.
- **RMT Buffer**: 64 symbols \* 4 bytes = 256 bytes (Hardware).

**Total**: ~2.3KB

### 8.2 CPU Load

- Task chay Priority = 1 (Thap nhat).
- Update rate: 50Hz (20ms/frame).
- Tinh toan pattern: < 100us/frame.
- RMT transmit: Hardware offload, ~0 CPU.

**Total CPU**: < 0.5% (Negligible)

---

## 9. Testing Strategy

### 9.1 Unit Tests

1. **Color Mapping Test:**

   - Verify `STATUS_COLORS[STATUS_NET_ETH]` = Green (0, 255, 0).

2. **Priority Test:**

   - Set STATUS_NET_ETH -> Set STATUS_OTA -> Current phai la OTA.
   - Set STATUS_OTA -> Set STATUS_NET_ETH -> Current van la OTA (khong override).

3. **Brightness Test:**

   - Input color (255, 255, 255), brightness 50 -> Output (127, 127, 127).

4. **Pattern Calculation:**
   - Blink 1Hz: Kiem tra ON/OFF dung chu ky 1s.

### 9.2 Integration Tests

1. **Boot Sequence:**

   - Khoi dong -> LED trang tho -> Ket noi Ethernet -> LED xanh solid.

2. **Failsafe Warning:**

   - Rut day mang -> Sau 2s -> LED cam nhay nhanh.

3. **OTA Override:**
   - Bat dau OTA -> LED tim chop -> Rut day mang -> LED van tim (khong doi).

---

## 10. Luu y Implementation

### 10.1 RMT Channel Conflict

- DMX Module dung RMT Channel 0-3 cho 4 Ports DMX.
- **Status LED dung Channel 7** (Channel cuoi cung) de tranh conflict.

### 10.2 Non-blocking RMT

- `rmt_transmit()` la non-blocking, tra ve ngay lap tuc.
- Driver copy data vao DMA buffer va truyen background.
- Khong can cho transmit complete truoc khi update color lan sau.

### 10.3 Identify Feature

```c
/**
 * @brief Kich hoat Identify mode (Goi tu Web)
 * @param duration_ms Thoi gian hien thi (ms)
 */
void status_trigger_identify(uint32_t duration_ms) {
    status_code_t old_code = g_status_state.current_code;

    status_set_code(STATUS_IDENTIFY);

    // Sau duration_ms, quay lai trang thai cu
    // Su dung timer hoac task delay
    vTaskDelay(pdMS_TO_TICKS(duration_ms));

    status_set_code(old_code);
}
```

---

## 11. Ket luan

File thiet ke nay cung cap full implementation details cho MOD_STATUS, bao gom:

- Cau truc du lieu day du (Colors, Patterns, State).
- Thuat toan pattern generation (Breathing, Blink, Double-blink).
- RMT encoding chi tiet cho WS2812B.
- State machine priority va transition effects.
- Memory/Performance analysis.

Developer co the implement truc tiep theo pseudo-code va algorithms da cung cap.

**Next Steps:**

- [ ] Implement `status_led.c` theo Section 7.
- [ ] Test tren hardware voi LEd WS2812B thuc.
- [ ] Verify timing bang Logic Analyzer (optional).
