# PROMPT SINH CODE MOD_NET (NETWORK MANAGER) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Senior IoT Network Engineer** chuyên sâu về ESP-IDF Networking Stack (LwIP).
Nhiệm vụ: Xây dựng **MOD_NET** - Module quản lý kết nối mạng thông minh cho DMX Node.
Hệ thống hỗ trợ 3 giao diện mạng với cơ chế ưu tiên: **Ethernet (W5500) > Wifi Station > Wifi AP**.

## 2. Ngữ cảnh hệ thống (Context)

- **Hardware:** ESP32-S3.
- **Ethernet Hardware:** Module W5500 kết nối qua SPI.
- **Yêu cầu cốt lõi:**
  - "Hot-plug": Khi cắm dây mạng, tự động ngắt Wifi và dùng Ethernet.
  - "Fail-over": Khi rút dây mạng, tự động kết nối lại Wifi đã lưu.
  - "Rescue Mode": Nếu không có mạng nào, phát Wifi AP để cấu hình.
- **Dependencies:** `esp_netif`, `esp_eth`, `esp_wifi`, `mdns`, `sys_mod`.

## 3. Data Contracts (INPUT BẮT BUỘC)

Sử dụng các cấu trúc dữ liệu sau (tương thích `SYS_MOD` và `MOD_NET.md`):

```c
// components/mod_net/include/net_types.h

typedef enum {
    NET_MODE_ETHERNET = 0,   // Ưu tiên cao nhất (Wired)
    NET_MODE_WIFI_STA,       // Ưu tiên nhì (Wireless Client)
    NET_MODE_WIFI_AP,        // Ưu tiên cuối (Rescue/Config)
    NET_MODE_NONE
} net_mode_t;

typedef struct {
    net_mode_t current_mode;
    bool eth_connected;      // Link status
    bool wifi_connected;     // Station connected
    bool has_ip;             // IP acquired
    char current_ip[16];     // IP dạng string "192.168.1.x"
} net_status_t;

// Config lấy từ SYS_MOD (tham khảo):
// bool dhcp_enabled; char ip[]; char netmask[]; ...

```

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **Ethernet W5500 Driver:**

- Sử dụng driver có sẵn trong ESP-IDF: `esp_eth_phy_w5500` và `esp_eth_mac_w5500`.
- Giao tiếp qua SPI bus (cần define chân GPIO rõ ràng).
- Cấu hình MAC Address cho W5500 dựa trên MAC gốc của ESP32 (để tránh trùng MAC trên mạng).

2. **Logic Ưu tiên (Priority Logic):**

- Luôn lắng nghe sự kiện `ETH_EVENT_CONNECTED` và `ETH_EVENT_DISCONNECTED`.
- Nếu Ethernet UP -> Stop Wifi (để tiết kiệm điện và nhiễu) -> Dùng Ethernet.
- Nếu Ethernet DOWN -> Start Wifi Connect -> Dùng Wifi.

3. **Event Handling:**

- Sử dụng `esp_event_handler_register` để bắt sự kiện từ hệ thống (IP_EVENT, WIFI_EVENT, ETH_EVENT).
- Khi có IP mới (`IP_EVENT_ETH_GOT_IP` hoặc `IP_EVENT_STA_GOT_IP`), phải bắn event `SYS_EVT_NET_CONNECTED` về cho `SYS_MOD`.

4. **mDNS Service:**

- Luôn chạy mDNS để người dùng truy cập được qua `http://dmx-node.local` (hoặc tên hostname trong config).

## 5. Phạm vi code cần sinh

Sinh code component `components/mod_net` theo cấu trúc:

```text
components/mod_net/
├── include/
│   ├── net_types.h       (Enum & Struct)
│   └── mod_net.h         (Public API)
├── mod_net.c             (Main logic: Init, Event Handler, Mode Switching)
├── net_eth.c             (Cấu hình W5500, SPI Init)
├── net_wifi.c            (Cấu hình Wifi STA/AP)
├── net_mdns.c            (Cấu hình mDNS)
└── CMakeLists.txt

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `mod_net.h` (Public API)

- `esp_err_t net_init(void);` (Khởi tạo toàn bộ stack)
- `net_status_t net_get_status(void);`
- `void net_reload_config(void);` (Gọi khi người dùng đổi IP/Wifi info từ Web)

### 6.2 `net_eth.c`

- Định nghĩa Pin Map (Sử dụng Macro để dễ sửa):
- `ETH_MISO_GPIO`, `ETH_MOSI_GPIO`, `ETH_SCLK_GPIO`, `ETH_CS_GPIO`
- `ETH_INT_GPIO`, `ETH_RST_GPIO`

- Hàm `net_eth_start()` và `net_eth_stop()`.

### 6.3 `net_wifi.c`

- Hàm `net_wifi_start_sta(ssid, pass)`: Kết nối Router.
- Hàm `net_wifi_start_ap(ssid, pass)`: Phát Wifi (cho chế độ Rescue).
- Lưu ý: Xử lý `WIFI_REASON_AUTH_EXPIRE` để retry nếu sai pass.

### 6.4 `mod_net.c` (Central Logic)

- Đăng ký lắng nghe Event.
- **State Machine:**
- Khi Init: Thử Start Ethernet.
- Nếu sau 2s không thấy Ethernet Link -> Start Wifi STA.
- Nếu Wifi STA fail 3 lần -> Start Wifi AP (Rescue Mode).

- Cập nhật biến `g_net_status` để Web có thể đọc được.

## 7. Output mong muốn

1. Code C chuẩn ESP-IDF v5.x.
2. Sử dụng `esp_netif_create_default_wifi_sta()` và `esp_netif_create_default_eth()`.
3. Xử lý Static IP: Nếu config `dhcp_enabled == false`, phải set IP tĩnh bằng `esp_netif_set_ip_info`.
4. Comment rõ ràng các đoạn xử lý Event.
