# PROMPT SINH CODE MAIN.C (SYSTEM ORCHESTRATOR) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Lead Embedded Integrator** (Trưởng nhóm tích hợp hệ thống).
Nhiệm vụ: Viết file `main/main.c` để liên kết tất cả 7 module lại với nhau thành một firmware hoàn chỉnh.
File `main.c` phải sạch, dễ đọc và tuân thủ chặt chẽ trình tự khởi động.

## 2. Ngữ cảnh hệ thống (Context)

Hệ thống bao gồm các module đã thiết kế:

1.  **MOD_STARTUP**: Quyết định chế độ boot (`startup_decide_mode`).
2.  **SYS_MOD**: Quản lý Config & Data (`sys_setup_all`).
3.  **MOD_STATUS**: LED báo hiệu (`status_init`).
4.  **MOD_NET**: Mạng (`net_init`).
5.  **MOD_PROTO**: Giao thức ArtNet/sACN (`proto_init`).
6.  **MOD_DMX**: Driver DMX (`dmx_init`).
7.  **MOD_WEB**: Web Server (`web_init`).

## 3. Logic luồng chương trình (Flowchart)

Trong hàm `app_main()`:

1.  **Bước 1: Pre-Boot Check**

    - Gọi `startup_decide_mode()` để lấy `boot_mode_t`.
    - Nếu là `BOOT_MODE_FACTORY_RESET` -> (Module Startup đã tự xử lý erase & reboot) -> Return.

2.  **Bước 2: Global Init**

    - Khởi tạo NVS Partition (`nvs_flash_init`).
    - Khởi tạo Netif (`esp_netif_init`).
    - Khởi tạo Event Loop (`esp_event_loop_create_default`).

3.  **Bước 3: System Core Init**

    - Gọi `sys_setup_all()` (Load config, init buffer).
    - Gọi `status_init()` (Để bắt đầu báo đèn Booting).

4.  **Bước 4: Branching (Phân nhánh theo Mode)**

    - **TRƯỜNG HỢP A: BOOT_MODE_RESCUE (Cứu hộ)**

      - Log cảnh báo "ENTERING RESCUE MODE".
      - Chớp đèn LED màu Tím (`STATUS_NET_AP`).
      - Chỉ khởi tạo Wifi AP (`net_init_rescue_mode` hoặc config tay Wifi AP).
      - Khởi tạo Web Server (`web_init`).
      - ❌ KHÔNG khởi tạo DMX, KHÔNG khởi tạo Proto (ArtNet/sACN).

    - **TRƯỜNG HỢP B: BOOT_MODE_NORMAL (Bình thường)**
      - Log "ENTERING NORMAL MODE".
      - Khởi tạo Network (`net_init`).
      - Khởi tạo Protocol Stack (`proto_init`).
      - Khởi tạo Web Server (`web_init`).
      - Khởi tạo DMX Driver (`dmx_init`) -> Start (`dmx_start`).
      - Đặt đèn LED trạng thái (Ví dụ: Vàng nếu chưa có mạng, Xanh nếu có Ethernet).
      - **Quan trọng:** Khởi tạo `Stability Timer`.

5.  **Bước 5: Stability Monitor**
    - Tạo một Software Timer (One-shot, 30 giây).
    - Callback của timer sẽ gọi `startup_mark_stable()`.
    - Mục đích: Nếu chạy được 30s mà không crash, coi như boot thành công (reset crash counter).

## 4. Include Headers

Bạn cần include đầy đủ headers của các module:

```c
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_log.h"
#include "startup.h"
#include "sys_setup.h"
#include "sys_mod.h"     // Để lấy sys_send_event
#include "mod_status.h"
#include "mod_net.h"
#include "mod_proto.h"
#include "mod_dmx.h"
#include "mod_web.h"

```

## 5. Yêu cầu chi tiết code

- **Log Tag:** `MAIN`.
- **Error Handling:** Với các module quan trọng (SYS, NET), nếu init thất bại -> Log Error và set LED `STATUS_ERROR` -> có thể `esp_restart()` sau 5s nếu lỗi nghiêm trọng.
- **Wifi AP Rescue:** Nếu trong `MOD_NET` chưa có hàm `net_start_rescue()`, hãy giả định cấu hình thủ công Wifi AP trong main bằng `net_wifi_start_ap("DMX-RESCUE", "")`.

## 6. Output mong muốn

1. File `main/main.c` hoàn chỉnh.
2. Code logic rõ ràng, chia làm các block comment: `// --- INIT ---`, `// --- NORMAL MODE ---`, `// --- RESCUE MODE ---`.
3. Đăng ký timer ổn định hệ thống.
