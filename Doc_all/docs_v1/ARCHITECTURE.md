# Architecture Document

## 1. Tong quan kien truc

He thong DMX Node V4.0 duoc thiet ke theo kien truc **Phan lop (Layered Architecture)** va **Huong su kien (Event-driven)**. Kien truc nay dam bao viec xu ly tin hieu thoi gian thuc (Real-time) khong bi anh huong boi cac tac vu mang hay giao dien nguoi dung.

## 2. So do kien truc Logic (Logical View)

```
[ USER INTERFACE ]       [ NETWORK INPUT ]
        |                       |
   (Web Browser)          (Art-Net / sACN)
        |                       |
        v                       v
+---------------+       +---------------+
|    MOD_WEB    |       |   MOD_PROTO   |
| (Core 0, Low) |       | (Core 0, High)|
+-------+-------+       +-------+-------+
        |                       |
        |  (API Calls)          | (Write Buffer)
        v                       v
+---------------------------------------+
|               SYS_MOD                 |
|            (SYSTEM CORE)              |
| ------------------------------------- |
|  - Config Manager (NVS)               |
|  - Data Routing Hub (DMX Buffers)     |
|  - Failsafe Logic                     |
+-------------------+-------------------+
                    | (Read Buffer)
                    v
+---------------------------------------+
|               MOD_DMX                 |
|           (DRIVER ENGINE)             |
| ------------------------------------- |
| Core 1, Highest Priority, 40Hz Locked |
+-------------------+-------------------+
                    |
              (RMT / UART)
                    |
                    v
            [ DMX512 OUTPUT ]
```

## 3. Phan bo phan cung (Deployment View)

De dam bao hieu nang (Jitter < 1us), he thong phan chia tai nguyen CPU nhu sau:

### 3.1 Core 0 (System & Protocol Core)

- **Start-up**: Khoi dong he thong.
- **MOD_NET**: Xu ly TCP/IP Stack, Wifi Driver, W5500 Driver.
- **MOD_PROTO**: Lang nghe UDP, giai ma Art-Net/sACN, Merge HTP/LTP.
- **MOD_WEB**: Xu ly HTTP Request, API JSON.
- **MOD_STATUS**: Dieu khien LED bao hieu.

### 3.2 Core 1 (Real-time Core)

- **MOD_DMX**:
  - Chay vong lap vo tan (Infinite Loop) dinh thoi chinh xac 40Hz.
  - Tinh toan RMT Items cho Break/MAB.
  - Day du lieu ra chan GPIO.
- **Luu y**: Core 1 khong duoc chay cac tac vu dính den Flash (NVS Write) hoac Wifi de tranh bi ngat (Cache miss).

## 4. Luong du lieu (Data Flow)

### 4.1 Luong tin hieu DMX (Art-Net -> Output)

1. **Input**: Goi tin UDP Art-Net den thu vie MOD_NET.
2. **Parsing**: MOD_PROTO nhan dang Header, tach lay Universe va Data.
3. **Merging**: Neu co nhieu nguon, MOD_PROTO thuc hien so sanh HTP/LTP.
4. **Routing**: MOD_PROTO copy ket qua vao Buffer cua Port tuong ung trong SYS_MOD (`sys_dmx_buffers`).
5. **Fetching**: Driver cua MOD_DMX (Core 1) doc Buffer nay o chu ky tiep theo.
6. **Output**: MOD_DMX bien doi Buffer thanh tin hieu dien RMT/UART.

### 4.2 Luong cau hinh (Web -> System)

1. **Request**: Nguoi dung gui POST JSON thay doi Timing.
2. **Handling**: MOD_WEB parse JSON.
3. **Validation**: MOD_WEB goi `sys_update_port_cfg()`. SYS_MOD kiem tra gia tri hop le.
4. **Update**:
   - SYS_MOD cap nhat bien cau hinh tren RAM (co Mutex).
   - SYS_MOD gui tin hieu (Signal) cho MOD_DMX.
5. **Persist**: SYS_MOD kich hoat Lazy Save de ghi xuong NVS sau 5s.

## 5. Co che Fail-safe (An toan he thong)

- **Network Timeout**: Neu khong nhan duoc goi tin Protocol sau 2 giay (cau hinh duoc).
  - SYS_MOD chuyen sang che do Fail-safe.
  - Tuy chon: HOLD (Giu nguyen), BLACKOUT (Tat den), SNAPSHOT (Phat canh luu truoc).
- **Boot Loop Protection**: STARTUP module dem so lan crash.
  - > 3 lan: Tu dong vao che do Rescue (Wifi AP, Default Config) de cuu ho.

## 6. Gioi han ky thuat (Technical Constraints)

- **Refresh Rate**: 40Hz (25ms/frame).
- **Latency**: < 3ms (Wifi Performance Mode), < 1ms (Ethernet).
- **Timing**: Break 88us-500us, MAB 8us-100us.
- **Snapshot**: Luu tru toi da 512 bytes/port trong NVS Blob.

## 7. Hieu nang va Kha nang mo rong (Performance & Scalability)

### 7.1 Tai nguyen he thong

- **CPU Load (Uoc tinh):**
  - Core 0 (Protocol): handling UDP 44 pkts/sec/univ -> Load trung binh.
  - Core 1 (DMX): RMT Hardware offload phan lon cong viec. Load Logic < 20%.
- **RAM Footprint:**
  - IRAM: ISR RMT/UART (~10KB).
  - DRAM: Stack & Control (~30KB).
  - PSRAM/Heap: DMX Buffers (512B \* 4 Ports = 2KB).
  - RMT Symbols: ~6000 symbols/frame/port (Static Alloc).

### 7.2 Scalability (Kha nang mo rong)

- **Universe Count:**
  - Hien tai: 4 Ports.
  - Gioi han: ESP32 co 8 Kenh RMT -> Max 8 Ports DMX (Ly thuyet).
  - Neu dung UART: Co the mo rong them (Can test CPU load).
- **Bottleneck:**
  - **Network Stack:** Xu ly qua nhieu Universe Broadcast co the gay tran nghen Interrupt Core 0. Giai phap: Dung Unicast hoac Multicast IGMP.
  - **System Bus:** SPI W5500 share bandwidth.

## 8. Tuong lai

- **RDM Proxy:** Ho tro day du RDM controller qua ArtNet.
- **Multi-core**: Nang cap len ESP32-P4 neu can xu ly > 16 Universes.
