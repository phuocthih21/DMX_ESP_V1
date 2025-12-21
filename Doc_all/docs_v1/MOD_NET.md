# MOD_NET (Connectivity Layer)

## 1. Thong tin Module

- **Module Name**: Connectivity Manager
- **Module ID**: `MOD_NET`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `MOD_NET.md` (Goc)

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Quan ly ket noi mang Ethernet (W5500) va Wifi (Station/AP).
- Quyet dinh Interface nao duoc su dung (Auto-switching).
- Cung cap dich vu mang: DHCP Client, DHCP Server (AP), mDNS.

**Module nay KHONG lam gi:**

- Khong xu ly noi dung goi tin (UDP/TCP Data payload).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-NET-01**: Khoi dong he thong mang dua tren config (Static/DHCP).
- **FR-NET-02**: Uu tien Ethernet. Neu mat Ethernet -> Thu ket noi Wifi Station.
- **FR-NET-03**: Neu Wifi Station that bai -> Bat Wifi Access Point (Rescue).
- **FR-NET-04**: Quang ba ten mien thiet bi qua mDNS (`.local`).
- **FR-NET-05**: Tu dong reconnect khi mang bi rot.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-NET-01**: Wifi Latency < 3ms (Bat buoc `WIFI_PS_NONE`).
- **NFR-NET-02**: Thoi gian chuyen doi interface (Switching time) < 2s.
- **NFR-NET-03**: Hot-plug Ethernet hoat dong tin cay.

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
esp_err_t net_init(const net_config_t *cfg);
esp_err_t net_switch_mode(net_mode_t mode);
void net_get_status(net_status_t *status);
// Callback event
ESP_EVENT_DECLARE_BASE(NET_EVENTS);
```

**Events:**

- `NET_EVENT_GOT_IP`: Bao cho Proto/Web biet da co mang.
- `NET_EVENT_LOST_IP`: Bao dung dich vu.

## 6. Cau truc du lieu (Data Structures)

**Public Structs:**

- `net_config_t`: Cau hinh IP/Wifi.
- `net_status_t`: IP hien tai, MAC, RSSI.

## 7. Luong xu ly (Processing Flow)

### 7.1 Khoi dong

1. Init W5500 Driver.
2. Check Link Ethernet.
   - Co Link -> Start DHCP Client -> Done.
   - Mat Link -> Init Wifi Driver.
3. Scan SSID.
   - Thay SSID -> Connect -> Done.
   - Khong thay -> Start AP Mode -> Done.

## 8. Quan ly loi va trang thai

- **ERR_NET_NO_IP**: Khong nhan duoc IP tu DHCP Server -> Assign Link-Local IP hoac Retry.
- **ERR_NET_WIFI_AUTH**: Sai mat khau Wifi -> Chuyen AP Mode ngay lap tuc de user config lai.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `esp_netif`, `esp_eth`, `esp_wifi` (ESP-IDF Component).
- **Phan cung**: W5500 (SPI).

## 10. Cau hinh (Configuration)

- `NET_WIFI_RETRY_MAX`: 3 lan.
- `NET_AP_IP`: 192.168.4.1.

## 11. Rang buoc ky thuat (Constraints)

- **W5500**: SPI Clock <= 20MHz (do day cau dai/nhieu).
- **Power**: Wifi Max TX Power yeu cau dong dinh cao.

## 12. Kich ban su dung (Use Case)

**UC-01: Auto-switch**

1. Dang chay Wifi (Status: Wifi Connected).
2. User cam day mang vao.
3. Event `ETH_LINK_UP` ban ra.
4. `MOD_NET` stop Wifi, start Ethernet DHCP.
5. `MOD_NET` ban Event `NET_EVENT_GOT_IP` (IP moi).
6. `MOD_PROTO` bind lai socket theo IP moi.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Implement `net_manager.c` State Machine.
- [ ] Test driver W5500 voi `esp_eth`.
- [ ] Test Wifi Performance Mode (ping test).
- [ ] Kiem tra mDNS Discovery.

## 14. Lien ket cheo tai lieu

- `ARCHITECUTRE.md`: Deployment view.
- `MOD_PROTO.md`: Consumer cua event mang.

## 15. Pham vi mo rong ve sau

- Ho tro IPv6.
- Ho tro nhieu Profile Wifi.

## 16. Tieu chi hoan thanh (Definition of Done)

- Ping den thiet bi on dinh < 3ms.
- Rut day mang/Rut Wifi tu dong recovery.
- Truy cap duoc bang ten mien .local.
