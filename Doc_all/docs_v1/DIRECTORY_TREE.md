# Directory Tree & File Distribution

## 1. Cau truc thu muc tong quan

Cau truc du an tuan thu chuan ESP-IDF Component-based.

```
project_root/
├── CMakeLists.txt              # Root build script
├── partitions.csv              # Bang phan vung (Factory, Play, NVS, OTA)
├── main/                       # Application Entry
│   ├── CMakeLists.txt
│   └── app_main.c              # Ham khoi tao he thong (System Entry Point)
├── components/                 # Cac module chuc nang
│   ├── sys_mod/                # [CORE] System Core & Config
│   ├── mod_dmx/                # [DRIVER] DMX Realtime Driver
│   ├── mod_net/                # [NET] Network Manager
│   ├── mod_proto/              # [PROTO] Protocol Stack (ArtNet/sACN)
│   ├── mod_web/                # [UI] Web Server & API
│   ├── mod_status/             # [UI] LED Indicator
│   ├── mod_rdm/                # [PROTO] RDM Responder (Optional)
│   └── common/                 # [SHARED] Cac thu vien dung chung
└── docs_v1/                    # Tai lieu ky thuat (Version 1.0)
```

## 2. Chi tiet cac thanh phan (Components)

### 2.1 main/

- `app_main.c`: Diem khoi dau cua Firmware.
  - Khoi tao NVS.
  - Goi `sys_mod_init`.
  - Goi `startup_mgr` de quyet dinh Mode chay (Normal/Rescue).

### 2.2 components/sys_mod/ (System Core)

Quan ly toan bo cau hinh va du lieu trung tam.

- `include/sys_api.h`: API public.
- `sys_manager.c`: Quan ly vong doi he thong.
- `sys_config.c`: Quan ly Load/Save config tu NVS. Validate du lieu.
- `sys_buffer.c`: Quan ly bo dem DMX 512 bytes cho cac port.
- `sys_ota.c`: Quan ly nap Firmware qua mang.

### 2.3 components/mod_dmx/ (Driver Layer)

Engine xuat tin hieu DMX512 thoi gian thuc.

- `include/dmx_driver.h`: API public.
- `dmx_engine.c`: Task chinh (40Hz Loop), xu ly Fail-safe.
- `dmx_rmt.c`: Backend RMT (High Precision) cho Port A/B.
- `dmx_uart.c`: Backend UART (Standard/RDM) cho Port C/D.

### 2.4 components/mod_net/ (Connectivity)

Quan ly ket noi mang.

- `mod_net.c`: Tu dong chuyen doi Ethernet <-> Wifi.
- `net_eth.c`: Driver W5500 SPI.
- `net_wifi.c`: Driver Wifi (Station & AP).
- `net_mdns.c`: Dich vu ten mien noi bo (.local).

### 2.5 components/mod_proto/ (Protocol Stack)

Xu ly giao thuc anh sang qua IP.

- `proto_mgr.c`: Lang nghe UDP socket.
- `proto_artnet.c`: Giai ma Art-Net 4.
- `proto_sacn.c`: Giai ma sACN (E1.31) va Multicast.
- `proto_merge.c`: Thuat toan Merge HTP/LTP.

### 2.6 components/mod_web/ (User Interface)

Giao dien dieu khien.

- `mod_web.c`: HTTP Server.
- `web_api.c`: Xu ly cac request REST API JSON.
- `html_gz.c`: Chua giao dien Web Front-end da nen Gzip.

### 2.7 components/mod_status/ (Indicator)

Phan hoi trang thai cho nguoi dung.

- `status_led.c`: Dieu khien LED WS2812B bang RMT.
- `status_logic.c`: State machine mau sac va hieu ung.

## 3. Quy tac phan bo file

- **Header Files (.h)**:
  - `include/`: Chi chua cac file header Public ma module khac can dung.
  - `internal/` hoac tai thu muc goc component: Chua header Private.
- **Metadata**:
  - Moi component phai co file `CMakeLists.txt` de dang ky voi he thong build.
  - `idf_component_register(...)` phai khai bao ro `REQUIRES` (cac module phu thuoc).
