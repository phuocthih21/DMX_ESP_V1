# API-Contract (Toan bo he thong)

## 1. Tong quan

Tai lieu nay dinh nghia hop dong giao tiep (Interface Agreement) giua cac module trong he thong firmware DMX Node V4.0. Moi module phai tuan thu tuyet doi cac quy dinh ve input/output va kieu du lieu duoc mo ta duoi day de dam bao tinh on dinh va realtime.

## 2. Kieu du lieu he thong (System Types)

Dinh nghia tai: `common/dmx_types.h`

### 2.1 Enums

**dmx_port_t**

- `DMX_PORT_A` (0)
- `DMX_PORT_B` (1)
- `DMX_PORT_C` (2)
- `DMX_PORT_D` (3)

**status_code_t**

- `STATUS_BOOTING`: Khoi dong
- `STATUS_NET_ETH`: Ket noi Ethernet
- `STATUS_NET_WIFI`: Ket noi Wifi
- `STATUS_NO_NET`: Mat mang
- `STATUS_DMX_WARN`: Mat tin hieu DMX
- `STATUS_OTA`: Dang update Firmware
- `STATUS_ERROR`: Loi he thong

### 2.2 Structs

**sys_config_t** (Cau hinh toan cuc)

- `net`: Cau hinh IP, Wifi
- `ports[4]`: Cau hinh 4 port DMX (Universe, Protocol, Enable)
- `failsafe`: Che do xu ly khi mat tin hieu (Hold/Snapshot)

## 3. SYS_MOD Interface

Module trung tam, quan ly State va Config.

### 3.1 Config API

- **Function**: `sys_get_config()`
  - **Input**: `void`
  - **Output**: `const sys_config_t*`
  - **Mo ta**: Tra ve con tro (Read-only) den cau hinh hien tai trong RAM.
- **Function**: `sys_update_port_cfg(port_idx, config)`
  - **Input**: `int`, `dmx_port_cfg_t*`
  - **Output**: `esp_err_t`
  - **Mo ta**: Cap nhat cau hinh port Runtime (Hot-swap). Tu dong validate va bao driver.

### 3.2 Data Routing API

- **Function**: `sys_get_dmx_buffer(port_idx)`
  - **Input**: `int`
  - **Output**: `uint8_t*` (Pointer to 512 bytes buffer)
  - **Mo ta**: Tra ve vung nho dem data cua port. Dung chung cho MOD_PROTO (Ghi) va MOD_DMX (Doc).
- **Function**: `sys_notify_activity(port_idx)`
  - **Input**: `int`
  - **Output**: `void`
  - **Mo ta**: Bao hieu co du lieu moi de reset Watchdog timer cua Port.

## 4. MOD_DMX Interface (Driver Layer)

Chiu trach nhiem xuat tin hieu vat ly.

### 4.1 Control API

- **Function**: `dmx_driver_init(cfg)`
  - **Input**: `const sys_config_t*`
  - **Output**: `esp_err_t`
  - **Mo ta**: Khoi tao RMT/UART, pin task vao Core 1.
- **Function**: `dmx_apply_new_timing(port, timing)`
  - **Input**: `int`, `dmx_timing_t*`
  - **Output**: `void`
  - **Mo ta**: Tinh toan lai thong so Break/MAB cho frame tiep theo.

## 5. MOD_NET Interface

Quan ly ket noi mang.

### 5.1 Control API

- **Function**: `net_init(cfg)`
  - **Input**: `const net_config_t*`
  - **Output**: `esp_err_t`
  - **Mo ta**: Khoi dong stack mang, uu tien Ethernet > Wifi.
- **Function**: `net_get_status(status)`
  - **Input**: `net_status_t*`
  - **Output**: `void`
  - **Mo ta**: Lay thong tin IP, MAC, Link status hien tai.

## 6. MOD_PROTO Interface

Xu ly goi tin ArtNet/sACN.

### 6.1 Control API

- **Function**: `proto_start()`
  - **Input**: `void`
  - **Output**: `esp_err_t`
  - **Mo ta**: Mo socket UDP (6454, 5568), bat dau task lang nghe.
- **Function**: `proto_reload_config()`
  - **Input**: `void`
  - **Output**: `void`
  - **Mo ta**: Leave/Join Multicast group lai dua theo mapping Universe moi.

## 7. MOD_STATUS Interface

Dieu khien LED bao hieu.

### 7.1 Control API

- **Function**: `status_set_code(code)`
  - **Input**: `status_code_t`
  - **Output**: `void`
  - **Mo ta**: Thay doi mau sac/hieu ung LED theo trang thai he thong. Non-blocking.

## 8. Quy tac chung

- **Validation**: Moi du lieu tu Web/User phai duoc SYS_MOD validate truoc khi luu.
- **Concurrency**:
  - Config: Read/Write phai co Mutex bao ve.
  - Buffer: Single-buffer (V4.0) chap nhan tearing nhe hoac su dung co che Atomic o V5.0.
- **Memory**: Khong free() cac pointer lay tu `sys_get_config` hoac `sys_get_dmx_buffer`.
