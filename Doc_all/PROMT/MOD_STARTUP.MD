# PROMPT SINH CODE MOD_STARTUP (BOOT MANAGER) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Principal Embedded System Engineer** chuyên về quy trình khởi động an toàn (Secure Boot & Safe Startup) trên ESP32.
Nhiệm vụ: Xây dựng **MOD_STARTUP** - Module chạy đầu tiên trong `app_main()`, chịu trách nhiệm bảo vệ thiết bị khỏi Boot Loop và quản lý các chế độ khởi động đặc biệt.

## 2. Ngữ cảnh hệ thống (Context)

- **Vị trí:** Chạy trước khi `SYS_MOD`, `MOD_NET`, hay `MOD_DMX` được khởi tạo.
- **Hardware:**
  - ESP32-S3.
  - **Button:** 1 nút nhấn vật lý (BOOT Button - GPIO 0) dùng để kích hoạt các chế độ.
- **Cơ chế bảo vệ (Safety):**
  - **Crash Counter:** Nếu thiết bị khởi động lại liên tục 3 lần (do lỗi code/nguồn), tự động chuyển sang chế độ Rescue (Wifi AP) để cứu hộ.
  - **Watchdog:** Cần tăng thời gian timeout của Watchdog trong quá trình boot để tránh reset oan.

## 3. Data Contracts (INPUT BẮT BUỘC)

Sử dụng các định nghĩa sau để các module khác hiểu được trạng thái boot:

```c
// main/startup/include/startup_types.h

typedef enum {
    BOOT_MODE_NORMAL = 0,      // Chạy bình thường (Load config từ NVS)
    BOOT_MODE_RESCUE,          // Chạy chế độ AP (Mặc định IP 192.168.4.1)
    BOOT_MODE_FACTORY_RESET    // Xóa sạch NVS và Reboot
} boot_mode_t;

// Cấu trúc lưu trong NVS để đếm lỗi
// Namespace: "boot_cfg", Key: "crash_cnt"

```

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **Logic Nút bấm (Polling chính xác):**

- Khi vừa cấp nguồn, module phải kiểm tra nút nhấn GPIO 0.
- Giữ **> 3s**: Chớp LED nhanh -> Vào chế độ **RESCUE**.
- Giữ **> 10s**: LED sáng đứng -> Vào chế độ **FACTORY RESET**.
- Không giữ: Vào chế độ **NORMAL**.
- _Lưu ý:_ Phải có Debounce để tránh nhiễu.

2. **Anti-Bootloop (Chống treo vòng lặp):**

- Đọc biến `crash_count` từ NVS ngay khi khởi động.
- Tăng `crash_count` lên 1 và ghi lại vào NVS ngay lập tức (commit).
- Nếu `crash_count >= 3` -> Cưỡng ép chuyển sang **BOOT_MODE_RESCUE**.
- Cung cấp hàm `startup_mark_stable()`: Hàm này sẽ được gọi sau 30s hoạt động ổn định để reset `crash_count` về 0.

3. **Brownout & Watchdog:**

- Tạm thời disable Brownout Detector (`WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0)`) để tránh reset nếu dòng khởi động tăng đột ngột (do tụ nạp).
- Cấu hình Task Watchdog (TWDT) lên 10s (mặc định ESP là 5s) để quá trình init các module nặng không bị timeout.

4. **Factory Reset:**

- Nếu vào mode này: Gọi `nvs_flash_erase()`, sau đó `esp_restart()`.

## 5. Phạm vi code cần sinh

Sinh code component `main/startup` (Lưu ý: đặt trong folder main vì nó gắn liền với app_main):

```text
main/startup/
├── include/
│   ├── startup_types.h   (Enum Boot Mode)
│   └── startup.h         (Public API)
├── startup.c             (Logic chính: Check nút, check crash count)
├── crash_monitor.c       (Quản lý NVS Crash Counter)
└── button_check.c        (Driver đơn giản đọc GPIO 0)

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `startup.h`

- `boot_mode_t startup_decide_mode(void);` (Hàm quan trọng nhất, gọi đầu tiên).
- `void startup_mark_stable(void);` (Gọi bởi Main Task sau 30s).

### 6.2 `button_check.c`

- Hàm `check_boot_button()`:
- Config GPIO 0 là Input Pull-up.
- Dùng vòng lặp `while` với `vTaskDelay(100ms)` để đếm thời gian giữ nút.
- Trong lúc giữ nút, có thể log ra console "Holding button: 1s...", "Holding button: 2s...".

### 6.3 `crash_monitor.c`

- Init NVS partition.
- Logic đọc/ghi/tăng biến `crash_cnt`.
- Nếu `crash_cnt > 3`: Log Error "DETECTED BOOT LOOP! FORCING RESCUE MODE".

### 6.4 `startup.c`

- Kết hợp 2 file trên.
- Trả về `boot_mode_t` cuối cùng cho `main.c` sử dụng.

## 7. Output mong muốn

1. Code C sử dụng thư viện `driver/gpio.h`, `nvs_flash.h`, `esp_rom_sys.h`.
2. Xử lý logic nút nhấn phải mượt, có log rõ ràng để người dùng biết đang nhấn giữ bao lâu.
3. Comment cảnh báo ở các đoạn code can thiệp vào thanh ghi (Brownout).

````

***

### Lưu ý quan trọng khi tích hợp `MOD_STARTUP`:

Sau khi có code từ prompt này, file `main.c` của bạn sẽ có luồng (Flow) như sau:

```c
void app_main(void) {
    // 1. Chạy Startup check đầu tiên
    boot_mode_t mode = startup_decide_mode();

    // 2. Xử lý theo mode
    if (mode == BOOT_MODE_FACTORY_RESET) {
        // Code startup.c đã tự handle việc erase và reboot rồi
        return;
    }

    // Init NVS chung cho hệ thống
    nvs_flash_init();

    if (mode == BOOT_MODE_RESCUE) {
        // Chỉ init tối thiểu để cứu hộ
        sys_setup_rescue(); // Load default config
        mod_net_start_ap_only();
        mod_web_start();
        // KHÔNG start DMX, KHÔNG start Ethernet
    } else {
        // 3. Normal Mode - Init toàn bộ
        sys_setup_all();
        // ...

        // Đăng ký timer 30s để gọi startup_mark_stable()
    }
}
````
