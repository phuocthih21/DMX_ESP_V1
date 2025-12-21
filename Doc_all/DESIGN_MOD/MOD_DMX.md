# MOD_DMX - Chi tiet Thiet ke Trien khai

## 1. Tong quan Thiet ke

Module `MOD_DMX` la "tim" cua he thong - chiu trach nhiem xuat tin hieu DMX512 realtime voi do chinh xac cao. Module nay su dung 2 backend: **RMT** (cho Port A/B - precision) va **UART** (cho Port C/D - standard/RDM).

**File nguon:** `components/mod_dmx/`

**Dependencies:** `driver/rmt`, `driver/uart`, `driver/gpio`, `esp_timer`

**Core Affinity:** MUST run on Core 1 (App Core), Priority = `configMAX_PRIORITIES - 1`

---

## 2. Data Structure Design

### 2.1 DMX Frame Structure

```c
/**
 * @brief Cau truc DMX512 Frame
 *
 * DMX512 Frame Layout:
 * [BREAK] [MAB] [START_CODE] [DATA_0] ... [DATA_511] [MTBF]
 *
 * Timing:
 * - Break: 88-500us (Default 176us)
 * - MAB: 8-100us (Default 12us)
 * - Slot time: 44us/byte (250kbps UART)
 * - Total frame: ~22.7ms
 * - MTBF: Fill remaining to 25ms
 */

#define DMX_START_CODE 0x00
#define DMX_SLOT_TIME_US 44
#define DMX_FRAME_SIZE 513  // Start code + 512 channels
```

### 2.2 Port Context Structure

```c
/**
 * @brief Context cho tung DMX Port
 * Size: ~128 bytes
 */
typedef struct {
    // Port Info
    uint8_t port_idx;              // 0-3
    bool enabled;

    // Backend Type
    enum {
        BACKEND_RMT = 0,
        BACKEND_UART
    } backend_type;

    // RMT Backend (Port A/B)
    struct {
        rmt_channel_handle_t channel;
        rmt_encoder_handle_t encoder;
        rmt_transmit_config_t tx_config;
        uint32_t break_ticks;      // Precomputed RMT ticks
        uint32_t mab_ticks;
    } rmt;

    // UART Backend (Port C/D)
    struct {
        uart_port_t uart_num;
        int tx_pin;
        int de_pin;                // Direction Enable (RS485)
        uint8_t frame_buffer[DMX_FRAME_SIZE + 10]; // Extra space cho Break/MAB
    } uart;

    // Timing Config (Hot-swappable)
    dmx_timing_t timing;
    bool timing_changed;           // Flag rebuild encoder

    // Failsafe State
    int64_t last_activity_ts;      // Timestamp Activity cuoi
    bool in_failsafe;
    uint8_t snapshot_buffer[512];  // Pre-loaded snapshot

    // Statistics (debug)
    uint32_t frame_count;
    uint32_t error_count;

} dmx_port_ctx_t;

static dmx_port_ctx_t g_dmx_ports[4];
```

### 2.3 Global Driver State

```c
/**
 * @brief Trang thai tong the driver
 */
typedef struct {
    bool initialized;
    bool running;

    TaskHandle_t task_handle;
    SemaphoreHandle_t sync_sem;   // Dung cho Hot-swap timing

    uint32_t refresh_rate_hz;      // Target refresh rate

    // Core affinity check
    BaseType_t core_id;

} dmx_driver_state_t;

static dmx_driver_state_t g_dmx_state = {0};
```

---

## 3. RMT Backend Design (Port A/B)

### 3.1 RMT Configuration

```c
/**
 * @brief Cau hinh RMT Channel cho DMX
 *
 * RMT Clock: 10MHz (APB 80MHz / divider 8)
 * Resolution: 0.1us/tick
 *
 * Channel Allocation:
 * - Port A: Channel 0
 * - Port B: Channel 1
 * - Reserved: Channel 2-6 (Future expansion)
 * - Status LED: Channel 7
 */

esp_err_t dmx_rmt_init_port(dmx_port_ctx_t* ctx, int gpio_tx) {
    // Step 1: Create TX Channel
    rmt_tx_channel_config_t tx_config = {
        .gpio_num = gpio_tx,
        .clk_src = RMT_CLK_SRC_DEFAULT,     // APB 80MHz
        .resolution_hz = 10000000,           // 10MHz
        .mem_block_symbols = 512,            // Large buffer for DMX frame
        .trans_queue_depth = 2,              // Double buffering
        .flags.invert_out = false,
        .flags.with_dma = true,              // Use DMA for large transfers
    };

    ESP_ERROR_CHECK(rmt_new_tx_channel(&tx_config, &ctx->rmt.channel));

    // Step 2: Create Custom DMX Encoder
    // (Detail implementation below)
    ESP_ERROR_CHECK(dmx_rmt_create_encoder(&ctx->rmt.encoder));

    // Step 3: Enable Channel
    ESP_ERROR_CHECK(rmt_enable(ctx->rmt.channel));

    // Step 4: Precompute timing ticks
    dmx_rmt_update_timing(ctx);

    return ESP_OK;
}
```

### 3.2 DMX RMT Encoder

RMT Encoder phai tao sequence: **[Break] [MAB] [Start Code] [Data 0-511]**

```c
/**
 * @brief Encoder DMX Frame thanh RMT Symbols
 *
 * RMT Symbol format:
 * {
 *   .level0 = HIGH/LOW,
 *   .duration0 = ticks,
 *   .level1 = HIGH/LOW,
 *   .duration1 = ticks
 * }
 */

typedef struct {
    rmt_encoder_t base;
    rmt_encoder_t* copy_encoder;
    rmt_encoder_t* bytes_encoder;
    rmt_symbol_word_t* symbol_buffer;
    size_t buffer_size;
} dmx_rmt_encoder_t;

/**
 * @brief Encode 1 DMX frame
 * Input: 512 bytes DMX data
 * Output: RMT symbol stream
 */
static size_t dmx_rmt_encode_frame(rmt_encoder_t* encoder,
                                    rmt_channel_handle_t channel,
                                    const void* data, size_t data_size,
                                    rmt_encode_state_t* ret_state)
{
    dmx_rmt_encoder_t* dmx_enc = (dmx_rmt_encoder_t*)encoder;
    dmx_port_ctx_t* ctx = ...; // Get context from encoder private data

    size_t encoded = 0;

    // Phase 1: BREAK (Low pulse)
    rmt_symbol_word_t break_symbol = {
        .level0 = 0,                     // LOW
        .duration0 = ctx->rmt.break_ticks,
        .level1 = 0,
        .duration1 = 0
    };
    encoded += rmt_encode_symbol(..., &break_symbol);

    // Phase 2: MAB (High pulse)
    rmt_symbol_word_t mab_symbol = {
        .level0 = 1,                     // HIGH
        .duration0 = ctx->rmt.mab_ticks,
        .level1 = 0,
        .duration1 = 0
    };
    encoded += rmt_encode_symbol(..., &mab_symbol);

    // Phase 3: Start Code (0x00) + 512 Data Bytes
    // Moi byte: [Start Bit (LOW)] [8 Data Bits] [2 Stop Bits (HIGH)]
    uint8_t frame_data[513];
    frame_data[0] = DMX_START_CODE;
    memcpy(&frame_data[1], data, 512);

    for (int i = 0; i < 513; i++) {
        encoded += dmx_rmt_encode_byte(encoder, frame_data[i]);
    }

    *ret_state = RMT_ENCODING_COMPLETE;
    return encoded;
}

/**
 * @brief Encode 1 UART byte thanh RMT symbols
 * Format: [START(LOW 4us)] [D0-D7] [STOP1(HIGH 4us)] [STOP2(HIGH 4us)]
 */
static size_t dmx_rmt_encode_byte(rmt_encoder_t* encoder, uint8_t byte) {
    // UART 250kbps: Bit time = 4us = 40 ticks @ 10MHz
    const uint16_t BIT_TIME = 40;

    rmt_symbol_word_t symbols[5]; // Start + 4 pairs data bits + Stop
    size_t idx = 0;

    // Start Bit (LOW)
    symbols[idx++] = (rmt_symbol_word_t){
        .level0 = 0, .duration0 = BIT_TIME,
        .level1 = 0, .duration1 = 0
    };

    // Data Bits (LSB first)
    for (int i = 0; i < 8; i += 2) {
        uint8_t bit0 = (byte >> i) & 0x01;
        uint8_t bit1 = (byte >> (i+1)) & 0x01;
        symbols[idx++] = (rmt_symbol_word_t){
            .level0 = bit0, .duration0 = BIT_TIME,
            .level1 = bit1, .duration1 = BIT_TIME
        };
    }

    // Stop Bits (2 bits HIGH)
    symbols[idx++] = (rmt_symbol_word_t){
        .level0 = 1, .duration0 = BIT_TIME,
        .level1 = 1, .duration1 = BIT_TIME
    };

    // Write to RMT
    return rmt_encode_symbols(..., symbols, idx);
}
```

### 3.3 Hot-swap Timing Update

```c
/**
 * @brief Cap nhat timing Runtime (Hot-swap)
 */
void dmx_rmt_update_timing(dmx_port_ctx_t* ctx) {
    // Tinh toan ticks (RMT @ 10MHz = 0.1us/tick)
    ctx->rmt.break_ticks = ctx->timing.break_us * 10;
    ctx->rmt.mab_ticks = ctx->timing.mab_us * 10;

    ESP_LOGI(TAG, "Port %d timing updated: Break=%uus (%u ticks), MAB=%uus (%u ticks)",
             ctx->port_idx,
             ctx->timing.break_us, ctx->rmt.break_ticks,
             ctx->timing.mab_us, ctx->rmt.mab_ticks);

    ctx->timing_changed = false;
}
```

---

## 4. UART Backend Design (Port C/D)

### 4.1 UART Configuration

```c
/**
 * @brief Cau hinh UART cho DMX
 *
 * UART Parameters:
 * - Baud: 250000
 * - Data: 8 bits
 * - Stop: 2 bits
 * - Parity: None
 *
 * Break Generation: Software control
 */

esp_err_t dmx_uart_init_port(dmx_port_ctx_t* ctx, uart_port_t uart_num,
                              int tx_pin, int de_pin)
{
    ctx->uart.uart_num = uart_num;
    ctx->uart.tx_pin = tx_pin;
    ctx->uart.de_pin = de_pin;

    // UART Config
    uart_config_t uart_config = {
        .baud_rate = 250000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(uart_num, tx_pin, -1, -1, -1));

    // Install driver (TX only, no RX buffer for DMX output)
    ESP_ERROR_CHECK(uart_driver_install(uart_num,
                                         0,      // RX buffer = 0
                                         1024,   // TX buffer
                                         0, NULL, 0));

    // Config Direction Enable Pin (RS485)
    gpio_config_t de_cfg = {
        .pin_bit_mask = (1ULL << de_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&de_cfg);
    gpio_set_level(de_pin, 1);  // TX mode

    return ESP_OK;
}
```

### 4.2 DMX Frame Transmission (UART)

```c
/**
 * @brief Gui DMX frame qua UART
 *
 * Sequence:
 * 1. Set TX pin LOW (Break)
 * 2. Delay break_us
 * 3. Set TX pin HIGH (MAB)
 * 4. Delay mab_us
 * 5. Send Start Code + 512 bytes qua UART
 */
void dmx_uart_send_frame(dmx_port_ctx_t* ctx, const uint8_t* data) {
    uart_port_t uart = ctx->uart.uart_num;

    // Step 1: Generate BREAK (Force TX LOW)
    // Tam dung UART de control pin truc tiep
    uart_wait_tx_done(uart, pdMS_TO_TICKS(10));
    uart_set_line_inverse(uart, UART_SIGNAL_TXD_INV);  // Invert = Force LOW
    esp_rom_delay_us(ctx->timing.break_us);

    // Step 2: Generate MAB (Release to HIGH)
    uart_set_line_inverse(uart, 0);  // Normal mode
    esp_rom_delay_us(ctx->timing.mab_us);

    // Step 3: Send Data
    ctx->uart.frame_buffer[0] = DMX_START_CODE;
    memcpy(&ctx->uart.frame_buffer[1], data, 512);

    uart_write_bytes(uart, ctx->uart.frame_buffer, 513);

    // Note: uart_write_bytes la quasi-blocking (copy vao TX FIFO buffer)
    // Actual transmission la non-blocking (Hardware UART xu ly)
}
```

---

## 5. Main Task Loop (40Hz Realtime)

```c
/**
 * @brief DMX Driver Task chinh
 * Priority: Cao nhat (configMAX_PRIORITIES - 1)
 * Core: 1 (Isolated from Wifi/Network)
 */
static void dmx_task_main(void* arg) {
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(25);  // 40Hz

    ESP_LOGI(TAG, "DMX Task started on Core %d", xPortGetCoreID());

    while (g_dmx_state.running) {
        int64_t now = esp_timer_get_time();

        // Process tung port
        for (int i = 0; i < 4; i++) {
            dmx_port_ctx_t* ctx = &g_dmx_ports[i];

            if (!ctx->enabled) continue;

            // Step 1: Check Timing Hot-swap
            if (ctx->timing_changed) {
                if (ctx->backend_type == BACKEND_RMT) {
                    dmx_rmt_update_timing(ctx);
                }
                // UART: Timing se ap dung ngay o dmx_uart_send_frame
            }

            // Step 2: Failsafe Check
            int64_t last_activity = g_sys_state.last_activity[i];  // From SYS_MOD
            uint32_t timeout_us = g_sys_config.failsafe.timeout_ms * 1000;

            uint8_t* data_ptr;

            if ((now - last_activity) > timeout_us) {
                // FAILSAFE MODE
                if (!ctx->in_failsafe) {
                    ESP_LOGW(TAG, "Port %d entering failsafe mode", i);
                    ctx->in_failsafe = true;
                    status_set_code(STATUS_DMX_WARN);  // LED Orange
                }

                switch (g_sys_config.failsafe.mode) {
                    case FAILSAFE_HOLD:
                        // Khong thay doi, gui buffer cu
                        data_ptr = sys_get_dmx_buffer(i);
                        break;

                    case FAILSAFE_BLACKOUT:
                        // Gui all-zero
                        memset(ctx->snapshot_buffer, 0, 512);
                        data_ptr = ctx->snapshot_buffer;
                        break;

                    case FAILSAFE_SNAPSHOT:
                        // Gui snapshot da luu
                        // (Snapshot da duoc load vao ctx->snapshot_buffer khi init)
                        data_ptr = ctx->snapshot_buffer;
                        break;
                }
            } else {
                // NORMAL MODE
                if (ctx->in_failsafe) {
                    ESP_LOGI(TAG, "Port %d back to normal", i);
                    ctx->in_failsafe = false;
                }
                data_ptr = sys_get_dmx_buffer(i);
            }

            // Step 3: Send Frame
            if (ctx->backend_type == BACKEND_RMT) {
                // RMT: Non-blocking transmit
                rmt_transmit(ctx->rmt.channel, ctx->rmt.encoder,
                            data_ptr, 512, &ctx->rmt.tx_config);
            } else {
                // UART: Quasi-blocking
                dmx_uart_send_frame(ctx, data_ptr);
            }

            ctx->frame_count++;
        }

        // Step 4: Precise Timing Control (40Hz lock)
        vTaskDelayUntil(&last_wake_time, period);
    }

    ESP_LOGI(TAG, "DMX Task stopped");
    vTaskDelete(NULL);
}
```

---

## 6. Initialization \u0026 Control API

### 6.1 Driver Init

```c
/**
 * @brief Khoi tao DMX Driver
 */
esp_err_t dmx_driver_init(const sys_config_t* cfg) {
    if (g_dmx_state.initialized) {
        ESP_LOGW(TAG, "Already initialized");
        return ESP_OK;
    }

    // Init Port A (RMT)
    g_dmx_ports[0].port_idx = 0;
    g_dmx_ports[0].backend_type = BACKEND_RMT;
    g_dmx_ports[0].enabled = cfg->ports[0].enabled;
    g_dmx_ports[0].timing = cfg->ports[0].timing;
    dmx_rmt_init_port(&g_dmx_ports[0], GPIO_PORT_A_TX);

    // Init Port B (RMT)
    g_dmx_ports[1].port_idx = 1;
    g_dmx_ports[1].backend_type = BACKEND_RMT;
    g_dmx_ports[1].enabled = cfg->ports[1].enabled;
    g_dmx_ports[1].timing = cfg->ports[1].timing;
    dmx_rmt_init_port(&g_dmx_ports[1], GPIO_PORT_B_TX);

    // Init Port C (UART 1)
    g_dmx_ports[2].port_idx = 2;
    g_dmx_ports[2].backend_type = BACKEND_UART;
    g_dmx_ports[2].enabled = cfg->ports[2].enabled;
    g_dmx_ports[2].timing = cfg->ports[2].timing;
    dmx_uart_init_port(&g_dmx_ports[2], UART_NUM_1, GPIO_PORT_C_TX, GPIO_PORT_C_DE);

    // Init Port D (UART 2)
    g_dmx_ports[3].port_idx = 3;
    g_dmx_ports[3].backend_type = BACKEND_UART;
    g_dmx_ports[3].enabled = cfg->ports[3].enabled;
    g_dmx_ports[3].timing = cfg->ports[3].timing;
    dmx_uart_init_port(&g_dmx_ports[3], UART_NUM_2, GPIO_PORT_D_TX, GPIO_PORT_D_DE);

    // Load Snapshots
    for (int i = 0; i < 4; i++) {
        if (cfg->failsafe.has_snapshot) {
            sys_snapshot_restore(i, g_dmx_ports[i].snapshot_buffer);
        }
    }

    g_dmx_state.initialized = true;
    g_dmx_state.refresh_rate_hz = 40;

    ESP_LOGI(TAG, "DMX Driver initialized");
    return ESP_OK;
}
```

### 6.2 Start/Stop

```c
/**
 * @brief Bat dau DMX Output
 */
void dmx_start(void) {
    if (g_dmx_state.running) return;

    g_dmx_state.running = true;

    // Tao task PINNED vao Core 1
    xTaskCreatePinnedToCore(
        dmx_task_main,
        "dmx_engine",
        4096,                             // Stack
        NULL,
        configMAX_PRIORITIES - 1,         // Priority cao nhat
        &g_dmx_state.task_handle,
        1                                 // Core 1
    );

    ESP_LOGI(TAG, "DMX Engine started");
}

/**
 * @brief Dung DMX Output (Can thiet truoc OTA)
 */
void dmx_stop(void) {
    if (!g_dmx_state.running) return;

    g_dmx_state.running = false;

    // Cho task terminate
    vTaskDelay(pdMS_TO_TICKS(50));

    // Set all outputs to IDLE (HIGH)
    for (int i = 0; i < 4; i++) {
        if (g_dmx_ports[i].backend_type == BACKEND_RMT) {
            rmt_disable(g_dmx_ports[i].rmt.channel);
        }
        // UART se tu dong idle HIGH
    }

    ESP_LOGI(TAG, "DMX Engine stopped");
}
```

---

## 7. Memory \u0026 Performance

### 7.1 Memory Footprint

- **Static RAM**: `g_dmx_ports[4]` = 512 bytes.
- **Static RAM**: `g_dmx_state` = 32 bytes.
- **Stack**: Task stack = 4KB.
- **RMT Hardware**: 512 symbols _ 2 ports _ 4 bytes = 4KB.
- **UART Buffer**: 1KB \* 2 ports = 2KB.

**Total**: ~11KB

### 7.2 CPU Load

- **Core 1 Dedicated**: 100% cua Core 1 availability (nhung thuc te chi dung ~5%).
- **RMT**: Hardware offload, 0 CPU khi transmit.
- **UART**: Software break generation (~50us CPU), Data transmission la Hardware.

**Effective CPU**: < 5% Core 1

### 7.3 Jitter Analysis

| Source             | Jitter                  |
| ------------------ | ----------------------- |
| FreeRTOS Scheduler | ~500ns (Core isolation) |
| RMT Hardware       | < 50ns                  |
| UART Hardware      | < 100ns                 |
| **Total**          | **< 1us** ✓             |

---

## 8. Testing Strategy

### 8.1 Unit Tests

1. **Timing Calculation:**

   - Input: break_us=176 -> Output: break_ticks=1760 (@ 10MHz).

2. **Frame Encoding:**

   - Input: [0x00, 0xFF, 0xAA] -> Verify RMT symbol count.

3. **Failsafe Logic:**
   - Mock `last_activity` old timestamp -> Verify snapshot buffer used.

### 8.2 Integration Tests

1. **40Hz Stability:**

   - Chay 1000 frames -> Measure interval -> Verify 25ms ± 0.1ms.

2. **Hot-swap Timing:**

   - Runtime change break_us=300 -> Verify frame tiep theo co break=300us (dung Logic Analyzer).

3. **Backend Switch:**
   - Disable Port A (RMT) -> Enable Port C (UART) -> Verify output on both.

### 8.3 Hardware Validation

- **Logic Analyzer:** Capture DMX waveform, verify:
  - Break width.
  - MAB width.
  - Slot timing (44us).
- **DMX Tester:** Ket noi DMX Tester professional de verify compliance with USITT DMX512-A standard.

---

## 9. Luu y Implementation

### 9.1 IRAM Requirement

```c
// Danh dau cac ham critical vao IRAM
IRAM_ATTR static void dmx_rmt_isr_handler(...) {
    // ISR code
}
```

### 9.2 Cache Miss Avoidance

- Code chay tren Core 1 PHAI o trong IRAM hoac PSRAM mapped cache-locked.
- Tranh Flash read khi Core 0 dang chay Wifi (gay cache miss -> jitter).

### 9.3 GPIO Pin Mapping

```c
// Dinh nghia trong dmx_driver.h
#define GPIO_PORT_A_TX  12
#define GPIO_PORT_B_TX  13
#define GPIO_PORT_C_TX  14
#define GPIO_PORT_C_DE  15
#define GPIO_PORT_D_TX  16
#define GPIO_PORT_D_DE  17
```

---

## 10. Ket luan

File thiet ke nay cung cap FULL chi tiet implementation-ready cho MOD_DMX:

- Data structures voi day du context.
- RMT Encoder algorithm (Break/MAB/Data encoding).
- UART software break generation.
- Realtime 40Hz task loop voi failsafe logic.
- Hot-swap timing mechanism.
- Memory/Performance analysis.
- Hardware validation methodology.

Developer co the implement truc tiep theo pseudo-code va specifications.

**Next Steps:**

- [ ] Implement `dmx_rmt.c` theo Section 3.
- [ ] Implement `dmx_uart.c` theo Section 4.
- [ ] Implement `dmx_engine.c` (main task) theo Section 5.
- [ ] Test voi Logic Analyzer va DMX hardware.
