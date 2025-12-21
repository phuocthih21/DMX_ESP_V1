# PROMPT SINH CODE MOD_DMX (REALTIME DRIVER) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Expert Embedded Driver Developer** (Chuyên gia lập trình Driver nhúng) chuyên về ESP32, Low-level Register và Real-time Systems.
Nhiệm vụ: Xây dựng **MOD_DMX** - Module chịu trách nhiệm tạo tín hiệu DMX512 vật lý.

## 2. Ngữ cảnh hệ thống (Context)

- **Hardware:** ESP32-S3.
- **Yêu cầu quan trọng:**
  - Module này chạy trên **Core 1** (App Core) để cách ly với Wifi (Core 0).
  - Phải đảm bảo tín hiệu DMX liên tục (Continuous Transmission) ở tần số ~40Hz.
  - Sai số timing (Jitter) phải cực thấp (< 5us).
- **Cấu hình phần cứng (Pin Mapping):**
  - **Port A (RMT):** GPIO 12
  - **Port B (RMT):** GPIO 13
  - **Port C (UART):** TX = GPIO 14, DE (Enable) = GPIO 15
  - **Port D (UART):** TX = GPIO 16, DE (Enable) = GPIO 17

## 3. Data Contracts (INPUT BẮT BUỘC)

Bạn cần include `dmx_types.h` từ module `SYS_MOD`. Giả định cấu trúc như sau:

```c
// Tham khảo từ SYS_MOD (Không cần define lại, chỉ include)
typedef struct {
    uint16_t break_us;      // Default: 176
    uint16_t mab_us;        // Default: 12
    uint8_t packet_rate;    // Default: 40Hz (Packet period ~25ms)
} dmx_timing_t;

```

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **Hybrid Driver Architecture:**

- **Port A & B:** Bắt buộc dùng **RMT Driver** (sử dụng API mới `driver/rmt_tx.h` của ESP-IDF v5). Phải viết Custom RMT Encoder để encode khung DMX (Break + MAB + Data).
- **Port C & D:** Bắt buộc dùng **UART Driver** (`driver/uart.h`). Phải xử lý chân DE (Data Enable) thủ công hoặc dùng tính năng RS485 flow control của UART hardware.

2. **Memory & Cache Safety (IRAM):**

- Tất cả Interrupt Service Routine (ISR) và các hàm gửi dữ liệu (`dmx_send_frame`) phải đặt attribute `IRAM_ATTR`.
- Mục đích: Tránh Cache Miss khi Flash bị chiếm dụng bởi Wifi, gây giật tín hiệu DMX.

3. **Task & Timing:**

- Tạo **Task DMX** chạy trên Core 1 với priority cực cao (`configMAX_PRIORITIES - 2`).
- Sử dụng `esp_timer` hoặc `vTaskDelayUntil` để duy trì chu kỳ gửi 40Hz ổn định.
- Tuyệt đối KHÔNG dùng vòng lặp `while(1)` block CPU 100%. Phải có `vTaskDelay` để nhường CPU cho Watchdog.

4. **Interface với SYS_MOD:**

- Gọi `sys_get_dmx_buffer(port)` để lấy dữ liệu.
- Nhớ dùng Mutex (thông qua API của SYS_MOD nếu có, hoặc copy nhanh dữ liệu) để tránh tranh chấp với Web.

## 5. Phạm vi code cần sinh

Sinh code component `components/mod_dmx` theo cấu trúc:

```text
components/mod_dmx/
├── include/
│   └── mod_dmx.h         (Public API)
├── dmx_core.c            (Task quản lý chính, Loop 40Hz)
├── dmx_rmt.c             (Driver RMT cho Port A, B - Kèm Encoder)
├── dmx_uart.c            (Driver UART cho Port C, D)
└── CMakeLists.txt

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `mod_dmx.h`

- `esp_err_t dmx_init(void);`
- `esp_err_t dmx_start(void);`
- `esp_err_t dmx_stop(void);`

### 6.2 `dmx_rmt.c` (Phần khó nhất)

- Implement `dmx_rmt_encoder`:
- Encode **Break**: Mức thấp trong `break_us`.
- Encode **MAB** (Mark After Break): Mức cao trong `mab_us`.
- Encode **Start Code (0x00)** + **512 Bytes Data**: Chuyển đổi từng byte thành chuỗi xung (1 start bit + 8 data bits + 2 stop bits).

- Cấu hình RMT TX channel.

### 6.3 `dmx_uart.c`

- Cấu hình UART mode RS485.
- Hàm gửi:

1. Set Pin DE lên High (Transmit mode).
2. Gửi tín hiệu Break (có thể dùng `uart_set_line_inverse` hoặc toggle GPIO thủ công để chính xác).
3. Gửi MAB.
4. Gửi Data (`uart_write_bytes`).
5. Chờ gửi xong (`uart_wait_tx_done`).
6. Set Pin DE về Low.

### 6.4 `dmx_core.c`

- Khởi tạo 4 ports.
- Vòng lặp chính (40Hz):
- Lấy Config timing từ SYS_MOD.
- Lấy dữ liệu buffer từ SYS_MOD.
- Gọi hàm gửi của `dmx_rmt` và `dmx_uart` cho từng port tương ứng.
- `vTaskDelay` để đảm bảo chu kỳ (ví dụ: gửi mất 23ms, delay 2ms để đủ 25ms).

## 7. Output mong muốn

1. Code C sử dụng đúng API ESP-IDF v5 (không dùng driver RMT cũ `rmt_write_items`).
2. Chú thích kỹ đoạn RMT Encoder vì đây là logic phức tạp.
3. Xử lý logic chân DE cho UART chính xác (quan trọng cho chip RS485).
