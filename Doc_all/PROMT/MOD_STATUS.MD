# PROMPT SINH CODE MOD_STATUS (STATUS LED) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Embedded Driver Engineer** chuyên về điều khiển ngoại vi thời gian thực trên ESP32-S3.
Nhiệm vụ: Xây dựng **MOD_STATUS** - Module điều khiển LED trạng thái (WS2812B/NeoPixel) để phản hồi thị giác cho người dùng.

## 2. Ngữ cảnh hệ thống (Context)

- **Hardware:** ESP32-S3.
- **LED Hardware:** 1 bóng LED WS2812B nối vào **GPIO 28**.
- **System Constraints:**
  - Module DMX (MOD_DMX) đã sử dụng **RMT Channel 0, 1, 2, 3**.
  - Do đó, MOD_STATUS **BẮT BUỘC phải dùng RMT Channel 7** để tránh xung đột tài nguyên.
- **Logic:** Module hoạt động dựa trên máy trạng thái (State Machine), hiển thị màu sắc và nhịp nháy khác nhau tùy theo `status_code`.

## 3. Data Contracts (INPUT BẮT BUỘC)

Sử dụng các định nghĩa sau cho mã trạng thái và màu sắc:

```c
// components/mod_status/include/status_types.h

typedef enum {
    STATUS_BOOTING = 0,      // Màu: Vàng (Yellow) - Solid
    STATUS_NET_ETH,          // Màu: Xanh lá (Green) - Solid (Mạng ngon)
    STATUS_NET_WIFI,         // Màu: Xanh dương (Blue) - Solid
    STATUS_NET_AP,           // Màu: Tím (Purple) - Slow Blink (Rescue mode)
    STATUS_NO_NET,           // Màu: Cam (Orange) - Slow Blink
    STATUS_DMX_WARN,         // Màu: Đỏ (Red) - Fast Blink (Mất tín hiệu DMX)
    STATUS_OTA,              // Màu: Cyan - Rotating/Rainbow
    STATUS_ERROR,            // Màu: Đỏ (Red) - Solid
    STATUS_IDENTIFY,         // Màu: Trắng (White) - Strobe (Chớp nhanh để tìm thiết bị)
    STATUS_OFF               // Tắt LED
} status_code_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} led_color_t;

typedef struct {
    status_code_t code;
    led_color_t color;
    uint16_t blink_interval_ms; // 0 = Solid (Sáng đứng)
} status_pattern_t;

```

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **RMT Channel Selection:**

- BẮT BUỘC cấu hình RMT Driver sử dụng **Channel 7**.
- Tuyệt đối không dùng `RMT_CHANNEL_MAX` hay auto-allocate có thể nhảy vào channel 0-3.

2. **WS2812B Protocol:**

- Dùng bộ mã hóa RMT (RMT Encoder) để tạo xung chuẩn WS2812B (T0H, T0L, T1H, T1L).
- Timing tham khảo: T0H=400ns, T0L=850ns, T1H=800ns, T1L=450ns (+/- 150ns).

3. **Non-blocking logic:**

- Tạo một **Task riêng** (`status_task`) để xử lý logic nháy (blink).
- Không dùng `vTaskDelay` trong các hàm API setter. API chỉ cập nhật biến trạng thái, Task sẽ tự đổi màu.

4. **Priority:**

- Task Priority: Thấp (1 hoặc 2). LED không được làm ảnh hưởng đến DMX Realtime (Core 1).

## 5. Phạm vi code cần sinh

Sinh code component `components/mod_status` theo cấu trúc:

```text
components/mod_status/
├── include/
│   ├── status_types.h    (Enum & Struct như mục 3)
│   └── mod_status.h      (Public API)
├── mod_status.c          (Main logic, FreeRTOS Task, API Setters)
├── led_driver.c          (RMT Driver, encode dữ liệu gửi ra GPIO)
├── led_patterns.c        (Mapping từ Enum -> Color/Interval)
└── CMakeLists.txt

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `mod_status.h`

- `esp_err_t status_init(void);`
- `void status_set_code(status_code_t code);`
- `void status_trigger_identify(uint32_t duration_ms);` (Chớp trắng trong X giây rồi quay lại trạng thái cũ).

### 6.2 `led_driver.c`

- Init RMT trên GPIO 28, Channel 7.
- Hàm `led_driver_send(uint8_t r, uint8_t g, uint8_t b)`: Gửi dữ liệu màu xuống LED.
- Lưu ý: WS2812B thường gửi theo thứ tự GRB (Green-Red-Blue). Hãy xử lý conversion này.

### 6.3 `mod_status.c` (Task Logic)

- Trong `status_task`:
- Kiểm tra `blink_interval_ms`.
- Nếu `interval > 0`: Bật/Tắt LED theo chu kỳ.
- Nếu `interval == 0`: Luôn gửi màu (refresh rate thấp, ví dụ 1Hz để giữ màu).

- Xử lý logic `Identify`: Khi được kích hoạt, override trạng thái hiện tại, hết giờ thì restore trạng thái cũ.

### 6.4 `led_patterns.c`

- Chứa một mảng lookup table `const status_pattern_t PATTERNS[]`.
- Định nghĩa màu sắc đẹp, dễ nhìn (đừng để max brightness 255 chói mắt, tầm 30-50 là đủ).

## 7. Output mong muốn

1. Code C chuẩn ESP-IDF v5.x.
2. Sử dụng `driver/rmt_tx.h` (API mới của IDF v5) hoặc `driver/rmt.h` (legacy) nhưng phải consistent. Ưu tiên **API mới `rmt_tx.h**`.
3. Comment giải thích rõ timing của WS2812B.

## 8. Nguyên tắc

Nếu mất tính hiệu DMX (`STATUS_DMX_WARN`), đèn phải nháy Đỏ nhanh (Fast Blink) để gây chú ý ngay lập tức.
