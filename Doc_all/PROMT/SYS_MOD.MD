# PROMPT SINH CODE SYS_MOD - ESP-IDF

## 1. Vai trò của AI

Bạn là **Principal Embedded Firmware Architect** (Kiến trúc sư phần mềm nhúng).
Nhiệm vụ: Xây dựng trọn bộ **SYS_MOD** - Module trung tâm quản lý dữ liệu, định tuyến và trạng thái của DMX Node.

## 2. Ngữ cảnh hệ thống

- **Hardware:** ESP32-S3.
- **Architecture:** Multi-core (Core 0: Network/Web, Core 1: DMX Realtime).
- **Requirement:** SYS_MOD là thư viện trung tâm, các module khác (WEB, DMX, NET) phụ thuộc vào nó.

## 3. Data Contracts (INPUT BẮT BUỘC)

Giữ nguyên cấu trúc dữ liệu chuẩn đã định nghĩa:

```c
// components/sys_mod/include/dmx_types.h
#pragma once
#include <stdint.h>
#include <stdbool.h>

#define SYS_CONFIG_MAGIC 0xDEADBEEF
#define DMX_NUM_PORTS 4
#define DMX_UNIVERSE_SIZE 512

typedef struct {
    uint16_t break_us;
    uint16_t mab_us;
    uint8_t packet_rate;
} dmx_timing_t;

typedef struct {
    bool enabled;
    uint8_t universe;
    uint8_t protocol; // 0=ArtNet, 1=sACN
    dmx_timing_t timing;
} dmx_port_config_t;

typedef struct {
    bool dhcp_enabled;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char hostname[32];
} net_config_t;

typedef struct {
    uint32_t magic_number;
    net_config_t net;
    dmx_port_config_t ports[DMX_NUM_PORTS];
    uint8_t led_brightness;
    char device_label[32];
} sys_config_t;

typedef enum {
    SYS_EVT_CONFIG_LOADED,
    SYS_EVT_CONFIG_SAVED,
    SYS_EVT_NET_CONNECTED,
    SYS_EVT_NET_DISCONNECTED,
    SYS_EVT_DMX_ACTIVE,
    SYS_EVT_ERROR
} sys_event_id_t;
4. Phạm vi code cần sinh (CẬP NHẬT ĐẦY ĐỦ)
Sinh toàn bộ file theo cấu trúc BUILD_PLAN.md:

Plaintext

components/sys_mod/
├── include/
│   ├── dmx_types.h       (Structs dùng chung)
│   ├── sys_mod.h         (Public API chính)
│   └── sys_setup.h       (API khởi tạo hệ thống)
├── sys_mod.c             (Event loop, Main dispatcher)
├── sys_config.c          (Getter/Setter + Mutex)
├── sys_nvs.c             (Load/Save/Factory Reset NVS)
├── sys_buffer.c          (Quản lý 4 buffer DMX tĩnh)
├── sys_route.c           (Logic tìm kiếm port từ universe)
├── sys_snapshot.c        (Lưu/Đọc trạng thái DMX frame hiện tại)
├── sys_setup.c           (Quy trình khởi tạo từng bước)
└── CMakeLists.txt
5. Yêu cầu chi tiết từng file
5.1 sys_mod.c & sys_mod.h
Khởi tạo Event Loop (sys_event_loop).

Hàm sys_send_event() để các nơi khác bắn sự kiện.

5.2 sys_config.c & sys_nvs.c
Thread Safety: Dùng Mutex bảo vệ g_sys_config.

NVS: Namespace "dmx_sys". Nếu load thất bại -> Load Default.

Lưu ý: sys_save_config ghi vào Flash, sys_update_config chỉ ghi vào RAM.

5.3 sys_buffer.c
static uint8_t dmx_buffers[4][512]; (Static allocation).

Hàm sys_dmx_write(port, data, len) phải có Mutex để tránh xé hình (tearing).

Hàm sys_dmx_read(port) trả về con trỏ (cho Driver đọc).

5.4 sys_route.c (MỚI)
Nhiệm vụ: Xác định gói tin ArtNet/sACN đến sẽ đi ra cổng nào.

Hàm: int8_t sys_route_find_port(uint8_t protocol, uint16_t universe);

Logic: Duyệt qua mảng cfg.ports[]. Nếu enabled && protocol match && universe match thì trả về index port (0-3). Nếu không thấy trả về -1.

Yêu cầu: Cần nhanh (vì gọi liên tục khi nhận mạng).

5.5 sys_snapshot.c (MỚI)
Nhiệm vụ: Chụp lại trạng thái 4 cổng DMX hiện tại và lưu vào NVS (để dùng tính năng "Hold Last State" sau khi reboot).

Hàm: esp_err_t sys_snapshot_save(void); và sys_snapshot_load(void);

Lưu ý: Dữ liệu lớn (512 * 4 = 2KB). Cân nhắc dùng NVS Blob hoặc Partition riêng nếu cần (nhưng ở đây dùng NVS Blob cho đơn giản).

5.6 sys_setup.c & sys_setup.h (MỚI)
Nhiệm vụ: Gom nhóm quy trình khởi động phức tạp.

Hàm esp_err_t sys_setup_all(void) sẽ gọi lần lượt:

nvs_flash_init()

sys_mod_init() (Event loop)

sys_nvs_load() (Load config)

sys_buffer_init()

File này giúp main.c chỉ cần gọi 1 dòng lệnh duy nhất.

6. Yêu cầu chung
Mutex: Bắt buộc dùng cho sys_config và sys_buffer.

Log: Dùng ESP_LOGI, ESP_LOGE với tag "SYS".

Code Style: Snake_case, comment rõ ràng các đoạn critical section.

Header Guard: #pragma once.

7. Output Format
Trả về code blocks cho từng file kèm tên file ở dòng đầu tiên.
```
