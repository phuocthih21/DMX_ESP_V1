# BUILD PLAN – DMX NODE V4.0

**Phien ban:** 1.0  
**Ngay:** 2025-12-18  
**Nguon tham khao:** `DESIGN_MOD/` (7 modules), `Mod_web.md`

---

## NGUYEN TAC LAP KE HOACH

1. **Xay tu loi → ngoai vi → giao dien**
2. **Module it phu thuoc lam truoc**
3. **Realtime build sau cung, khi data path da on dinh**
4. **Luon co trang thai quan sat duoc (LED / Log) o moi giai doan**
5. **Moi checkpoint phai co test case cu the**

---

## GIAI DOAN 0 – NEN TANG (1-2 ngay)

### B0.1 – Project Skeleton

**Muc tieu**

- Co the build & flash firmware trong

**Thuc hien**

```
DMX_ESP_V1/
├── CMakeLists.txt           # Root project
├── main/
│   ├── CMakeLists.txt
│   └── main.c               # app_main() empty
├── components/
│   ├── sys_mod/
│   ├── mod_dmx/
│   ├── mod_net/
│   ├── mod_proto/
│   ├── mod_web/
│   └── mod_status/
└── common/
    └── dmx_types.h
```

**Checklist**

- [ ] `idf.py build` thanh cong
- [ ] `idf.py flash` thanh cong
- [ ] Serial output: "DMX Node V4.0 - Boot"
- [ ] Khong reset loop

**Phu thuoc:** Khong

---

## GIAI DOAN 1 – SYSTEM CORE (3-5 ngay)

### B1.1 – `sys_setup.c` (0.5 ngay)

**Tai sao lam truoc**

- Tat ca module khac phu thuoc GPIO mapping & port config

**Noi dung** (DESIGN_MOD/SYS_MOD.md)

```c
// components/sys_mod/sys_setup.c
const port_hw_map_t g_port_map[4] = {
    {0, BACKEND_RMT,  GPIO_TX_12, -1},      // Port A
    {1, BACKEND_RMT,  GPIO_TX_13, -1},      // Port B
    {2, BACKEND_UART, GPIO_TX_14, GPIO_DE_15}, // Port C
    {3, BACKEND_UART, GPIO_TX_16, GPIO_DE_17}  // Port D
};
```

**Debug**

- [ ] Compile-time validate
- [ ] Print mapping luc boot: "Port A: RMT @ GPIO 12"

**File lien quan**

- `sys_setup.c`
- `sys_setup.h`

---

### B1.2 – `sys_config.c` (1 ngay)

**Tai sao**

- Can cau hinh runtime truoc khi chay DMX

**Noi dung** (DESIGN_MOD/SYS_MOD.md Section 4)

- Load/Save NVS (`sys_load_config_from_nvs`)
- Default config fallback
- Mutex bao ve (`g_sys_config`)
- CRC32 validation

**Debug**

- [ ] Test save → reboot → load → verify
- [ ] Inject corrupt NVS → fallback default
- [ ] Change config → verify lazy save (5s)

**File lien quan**

- `sys_config.c`
- `sys_nvs.c`

---

### B1.3 – `sys_buffer.c` (1 ngay)

**Tai sao**

- Day la diem giao giua protocol va DMX

**Noi dung** (DESIGN_MOD/SYS_MOD.md Section 6-7)

- DMX buffer 512 bytes / port (PSRAM)
- Timestamp update (`sys_notify_activity`)
- Snapshot record/restore
- Thread-safe pointer access

**Debug**

- [ ] Ghi du lieu gia vao buffer
- [ ] Doc buffer qua `sys_get_dmx_buffer()`
- [ ] Test snapshot: Record → Power cycle → Restore

**File lien quan**

- `sys_buffer.c`
- `sys_snapshot.c`

---

### B1.4 – `sys_route.c` (0.5 ngay)

**Tai sao**

- Can routing truoc khi day du lieu vao DMX

**Noi dung**

- Universe → Port map (Hash table)
- Cache routing table
- `sys_find_port_for_universe()`

**Debug**

- [ ] Unit test: Universe 0 → Port 0
- [ ] Test thay doi mapping runtime

**File lien quan**

- `sys_route.c`

---

**✅ CHECKPOINT 1: SYS_MOD hoan chinh**

- [ ] `sys_mod_init()` chay thanh cong
- [ ] Config load/save/persist
- [ ] Buffer allocation (4x512 bytes)
- [ ] Routing table hop le

---

## GIAI DOAN 2 – OBSERVABILITY (1 ngay)

### B2.1 – `mod_status` (1 ngay)

**Tai sao**

- Co trang thai truc quan de debug cac buoc sau

**Noi dung** (DESIGN_MOD/MOD_STATUS.md)

- WS2812B RMT driver (Channel 7)
- State machine (10 codes)
- Pattern generation (Breathing, Blink, Strobe)
- Priority system

**Debug**

- [ ] Force STATUS_BOOTING → LED trang tho
- [ ] Force STATUS_ERROR → LED do nhay
- [ ] Test priority: Set OTA → Set ERROR → LED van tim (OTA overrides)
- [ ] Brightness control: 0% → 100%

**File lien quan**

- `mod_status/status_led.c`
- `mod_status/patterns.c`

---

**✅ CHECKPOINT 2: Co den bao trang thai**

- [ ] LED phan anh dung trang thai he thong
- [ ] Khong block main task (Priority thap nhat)

---

## GIAI DOAN 3 – NETWORK LAYER (2-3 ngay)

### B3.1 – `mod_net` – Ethernet (1 ngay)

**Tai sao**

- Ethernet don gian hon Wifi, test truoc

**Noi dung** (DESIGN_MOD/MOD_NET.md Section 3)

- W5500 SPI init (20MHz)
- DHCP client
- Event handler (LINK_UP/LINK_DOWN)

**Debug**

- [ ] Log: "Ethernet Link Up"
- [ ] Log: "Got IP: 192.168.1.100"
- [ ] Ping tu PC → ESP32
- [ ] Rut day → LED vang (NO_NET)

---

### B3.2 – `mod_net` – Wifi (1 ngay)

**Noi dung** (DESIGN_MOD/MOD_NET.md Section 4)

- Wifi STA init
- WIFI_PS_NONE (Performance mode)
- Auto-connect
- Fallback AP mode (Rescue)

**Debug**

- [ ] Ket noi Wifi thanh cong → LED xanh duong (WIFI)
- [ ] Sai password → Switch AP mode
- [ ] Ping latency < 3ms

---

### B3.3 – Auto-switching & mDNS (0.5 ngay)

**Noi dung**

- Priority logic: Ethernet > Wifi STA > Wifi AP
- mDNS responder (`dmx-node.local`)

**Debug**

- [ ] Cam day Ethernet → Switch tu Wifi
- [ ] `ping dmx-node.local` → Phan hoi

---

**✅ CHECKPOINT 3: Network on dinh**

- [ ] Thiet bi co IP on dinh
- [ ] Auto-switch hoat dong
- [ ] mDNS discovery hoat dong

---

## GIAI DOAN 4 – PROTOCOL STACK (3-4 ngay)

### B4.1 – Art-Net Parser (1.5 ngay)

**Tai sao**

- Art-Net don gian hon sACN (no priority, no multicast)

**Noi dung** (DESIGN_MOD/MOD_PROTO.md Section 4.1)

- UDP socket bind port 6454
- Parse Art-Net DMX packet
- Extract universe & data
- Push vao `sys_buffer`

**Debug**

- [ ] GUI Art-Net packet tu Resolume
- [ ] Verify buffer match: `sys_get_dmx_buffer(0)[0..10]`
- [ ] Test sai header → drop silent

---

### B4.2 – sACN Parser (1.5 ngay)

**Noi dung** (DESIGN_MOD/MOD_PROTO.md Section 4.2)

- UDP socket bind port 5568
- Multicast join (239.255.x.x)
- Parse sACN packet
- Extract priority

**Debug**

- [ ] Gui sACN tu QLC+
- [ ] Verify multicast join: `netstat -g`

---

### B4.3 – Merge Engine (1 ngay)

**Noi dung** (DESIGN_MOD/MOD_PROTO.md Section 6)

- HTP merge (Source A + Source B)
- Priority-based switching
- Timeout detection (2.5s)

**Debug**

- [ ] 2 nguon sACN (Prio 100 va 150) → Chon 150
- [ ] Tat nguon cao → Fallback nguon thap sau 2.5s

---

**✅ CHECKPOINT 4: Protocol → Buffer**

- [ ] `sys_buffer` nhan du lieu that tu console
- [ ] Merge logic hoat dong dung
- [ ] Timeout handling on dinh

---

## GIAI DOAN 5 – DMX ENGINE (4-6 ngay)

### B5.1 – `dmx_rmt` – RMT Backend (2 ngay)

**Tai sao**

- One-way, de test nhat

**Noi dung** (DESIGN_MOD/MOD_DMX.md Section 3)

- RMT encoder (Break/MAB/Data)
- Timing calculation (10MHz = 0.1us/tick)
- Channel 0-1 (Port A/B)

**Debug**

- [ ] **Oscilloscope**:
  - Break width = 176us ± 1us
  - MAB width = 12us ± 1us
  - Bit time = 4us ± 0.1us
- [ ] Gui all 0xFF → DMX tester nhan dung

---

### B5.2 – `dmx_uart` – UART Backend (1.5 ngay)

**Noi dung** (DESIGN_MOD/MOD_DMX.md Section 4)

- UART 250000 bps, 8N2
- Software break generation
- RS485 DE/RE control

**Debug**

- [ ] Loopback test: TX → RX
- [ ] DMX tester nhan dung du lieu

---

### B5.3 – `dmx_core` – Main Loop (1.5 ngay)

**Tai sao**

- Phu thuoc tat ca phia tren

**Noi dung** (DESIGN_MOD/MOD_DMX.md Section 7)

- Realtime task 40Hz (Core 1, Priority max)
- Fetch buffer tu SYS_MOD
- Fail-safe logic (Hold/Blackout/Snapshot)
- Hot-swap timing

**Debug**

- [ ] FPS counter = 40 ± 1 Hz
- [ ] Rut day mang → Sau 2s active Failsafe
- [ ] Thay doi Break time runtime → Frame tiep theo apply

---

**✅ CHECKPOINT 5: DMX OUTPUT ON DINH**

- [ ] 4 ports DMX phat on dinh
- [ ] Timing dung chuan DMX512-A
- [ ] Jitter < 1us (RMT)
- [ ] Fail-safe hoat dong
- [ ] Hot-swap timing hoat dong

---

## GIAI DOAN 6 – RDM (MOI RONG - KE HOACH NAM 2)

### B6.1 – `mod_rdm` (Future)

**Luu y:** Khong uu tien cho V4.0 ban dau.

**Noi dung**

- RDM request decoder
- Response generator
- UART half-duplex

---

## GIAI DOAN 7 – WEB BACKEND (2-3 ngay)

### B7.1 – HTTP Server Init (0.5 ngay)

**Noi dung** (DESIGN_MOD/MOD_WEB.md Section 5)

- `esp_http_server` init
- Static file handler (gzip)
- CORS headers

**Debug**

- [ ] `curl http://192.168.1.100/` → Tra ve HTML

---

### B7.2 – REST API - System (1 ngay)

**Noi dung** (Mod_web.md Section 4.1)

- `GET /api/sys/info` → JSON telemetry
- `POST /api/sys/reboot` → Reboot
- `POST /api/sys/factory` → Factory reset

**Debug**

- [ ] `curl http://IP/api/sys/info` → JSON hop le
- [ ] Reboot API → Thiet bi khoi dong lai sau 1s

---

### B7.3 – REST API - DMX (1 ngay)

**Noi dung** (Mod_web.md Section 4.3)

- `POST /api/dmx/config` → Update universe
- `POST /api/dmx/advanced` → Timing hot-swap
- `POST /api/dmx/snapshot` → Record snapshot

**Debug**

- [ ] Update universe qua API → Verify routing
- [ ] Change break_us → DMX frame apply ngay

---

### B7.4 – Config Import/Export (0.5 ngay)

**Noi dung**

- `GET /api/file/export` → Download JSON
- `POST /api/file/import` → Upload JSON → Reboot

**Debug**

- [ ] Export config → Verify JSON structure
- [ ] Import config → Reboot → Config ap dung

---

**✅ CHECKPOINT 7: Web Backend hoat dong**

- [ ] Truy cap Dashboard qua browser
- [ ] API tra ve JSON hop le
- [ ] Hot-swap qua Web hoat dong

---

## GIAI DOAN 8 – WEB FRONTEND (5-7 ngay)

### B8.1 – Project Setup (0.5 ngay)

**Cong nghe**

- Framework: **React** hoac **Vue 3**
- Build: Vite
- UI Library: TailwindCSS (optional)
- State: Context API / Pinia

**Cau truc**

```
frontend/
├── src/
│   ├── api/
│   │   ├── client.js       # Axios/Fetch wrapper
│   │   └── endpoints.js    # API definitions
│   ├── stores/
│   │   ├── systemStore.js
│   │   ├── dmxStore.js
│   │   └── networkStore.js
│   ├── components/
│   │   ├── Dashboard/
│   │   ├── DMXConfig/
│   │   ├── NetworkConfig/
│   │   └── SystemSettings/
│   └── App.jsx
└── dist/                   # Build output
```

**Debug**

- [ ] `npm run dev` chay thanh cong
- [ ] Access http://localhost:5173

---

### B8.2 – API Layer (1 ngay)

**Noi dung**

```javascript
// src/api/client.js
export const api = {
  async getSysInfo() {
    const res = await fetch("/api/sys/info");
    return res.json();
  },
  async updateDMXConfig(port, config) {
    const res = await fetch("/api/dmx/config", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ port, ...config }),
    });
    return res.json();
  },
};
```

**Debug**

- [ ] Test API call tu console browser
- [ ] Error handling: Network timeout → Show error

---

### B8.3 – State Management (1 ngay)

**Noi dung**

```javascript
// stores/systemStore.js (React Context / Pinia)
const systemStore = {
  state: {
    connected: false,
    uptime: 0,
    freeHeap: 0,
    cpuLoad: 0,
  },
  actions: {
    async fetchInfo() {
      const data = await api.getSysInfo();
      this.state = { ...this.state, ...data };
    },
  },
};
```

**Debug**

- [ ] Store update khi fetch API
- [ ] UI re-render khi state change

---

### B8.4 – Dashboard Tab (1.5 ngay)

**Noi dung** (Mod_web.md TAB 1)

- **Telemetry Cards**:
  - CPU Load (%)
  - RAM Free (MB)
  - Uptime (hh:mm:ss)
  - Network Status (Icon + IP)
- **Port Status Grid** (4 ports):
  - State: IDLE / RUNNING / HOLD / ERROR
  - FPS counter
  - Source IP

**Debug**

- [ ] Real-time update (polling 1s)
- [ ] Responsive layout (mobile + desktop)

---

### B8.5 – DMX Config Tab (1.5 ngay)

**Noi dung** (Mod_web.md TAB 2)

- **Per-port config form**:
  - Protocol dropdown (ArtNet / sACN / Off)
  - Universe input (0-32767)
  - Enable/Disable toggle
- **Advanced section** (collapsible):
  - Break/MAB sliders (88-500us / 8-100us)
  - Refresh rate (20-44Hz)
- **Snapshot button**: Record current scene

**Debug**

- [ ] Form validation: Universe > 32767 → Error
- [ ] Submit → API call → Success notification
- [ ] Snapshot → Confirm dialog → Record

---

### B8.6 – Network Config Tab (1 ngay)

**Noi dung** (Mod_web.md TAB 3)

- **Ethernet config**:
  - Static/DHCP toggle
  - IP/Mask/Gateway inputs
- **Wifi config**:
  - SSID scanner (button)
  - Password input
  - Connect button
- **mDNS**:
  - Hostname input

**Debug**

- [ ] SSID scan → Hien thi list
- [ ] Change config → Warning: "Requires reboot"

---

### B8.7 – System Settings Tab (0.5 ngay)

**Noi dung** (Mod_web.md TAB 4)

- **Config Management**:
  - Export button → Download JSON
  - Import button → Upload JSON
- **Firmware OTA**:
  - Upload .bin file
  - Progress bar
- **Danger Zone**:
  - Reboot button
  - Factory Reset (hold 3s)

**Debug**

- [ ] Export → Valid JSON file
- [ ] Import → Apply config → Reboot
- [ ] Factory Reset → Confirm dialog

---

### B8.8 – Real-time Updates (0.5 ngay)

**Noi dung**

- Polling every 1s cho Dashboard
- Debounce cho form inputs (500ms)
- Connection status indicator

**Debug**

- [ ] Dashboard update smoothly
- [ ] Disconnect backend → Show "Offline" badge
- [ ] Reconnect → Auto-resume polling

---

### B8.9 – Build & Embed (0.5 ngay)

**Noi dung**

```bash
# Build frontend
cd frontend && npm run build

# Gzip files
gzip -9 dist/index.html
gzip -9 dist/assets/index.js
gzip -9 dist/assets/index.css

# Convert to C arrays
xxd -i dist/index.html.gz > components/mod_web/web_assets/index_html.c
xxd -i dist/assets/index.js.gz > components/mod_web/web_assets/app_js.c
```

**Debug**

- [ ] Embedded web served correctly
- [ ] All assets load (CSS, JS)

---

**✅ CHECKPOINT 8: Web Frontend hoan chinh**

- [ ] Dashboard hien thi real-time
- [ ] Cau hinh DMX qua Web hoat dong
- [ ] Responsive tren mobile
- [ ] Build & embed vao firmware

---

## GIAI DOAN 9 – INTEGRATION & QA (3-5 ngay)

### B9.1 – Fail-Safe Testing (1 ngay)

**Test Cases**

- [ ] Rut day mang → DMX hold/blackout/snapshot
- [ ] Power glitch → Boot protection → Rescue mode
- [ ] Crash 3 lan → Auto Rescue mode

---

### B9.2 – Performance Testing (1 ngay)

**Test Cases**

- [ ] 4 universes @ 44Hz → CPU < 50%
- [ ] Heap stable sau 24h
- [ ] DMX jitter < 1us (Oscilloscope)

---

### B9.3 – Stress Testing (1 ngay)

**Test Cases**

- [ ] Web: 100 slider changes/minute → No crash
- [ ] Network: Disconnect/reconnect 100 lan → On dinh
- [ ] DMX: Hot-swap timing 1000 lan → Khong loi

---

### B9.4 – User Acceptance Testing (1 ngay)

**Test Cases**

- [ ] End-to-end: Boot → Config qua Web → DMX output
- [ ] Rescue scenario: Reset → AP mode → Restore → Normal
- [ ] OTA update: Upload firmware → Reboot → Verify version

---

### B9.5 – Documentation (1 ngay)

**Noi dung**

- User Manual (PDF)
- Quick Start Guide
- API Documentation (Postman collection)
- Troubleshooting Guide

---

**✅ CHECKPOINT 9: PRODUCTION READY**

- [ ] Tat ca test cases pass
- [ ] Tai lieu day du
- [ ] Firmware stable 72h continuous operation

---

## TONG KET THU TU XAY DUNG

```
Phase 0: Skeleton (1-2 ngay)
   ↓
Phase 1: SYS_MOD (3-5 ngay)
   ↓
Phase 2: LED Status (1 ngay)
   ↓
Phase 3: Network (2-3 ngay)
   ↓
Phase 4: Protocol (3-4 ngay)
   ↓
Phase 5: DMX Engine (4-6 ngay) ★ CRITICAL
   ↓
Phase 6: RDM (Future - skipped V4.0)
   ↓
Phase 7: Web Backend (2-3 ngay)
   ↓
Phase 8: Web Frontend (5-7 ngay)
   ↓
Phase 9: QA & Hardening (3-5 ngay)
```

**TONG THOI GIAN UOC TINH:** 24-36 ngay (1 thang - 1.5 thang)

---

## PHU LUC: DANH SACH FILE CAN TAO

### Backend Firmware

```
components/sys_mod/
├── sys_setup.c
├── sys_config.c
├── sys_buffer.c
├── sys_route.c
├── sys_nvs.c
└── sys_snapshot.c

components/mod_dmx/
├── dmx_rmt.c
├── dmx_uart.c
├── dmx_core.c
└── dmx_encoder.c

components/mod_net/
├── net_manager.c
├── net_eth.c
├── net_wifi.c
└── net_mdns.c

components/mod_proto/
├── proto_mgr.c
├── artnet.c
├── sacn.c
└── merge.c

components/mod_web/
├── web_server.c
├── api_sys.c
├── api_dmx.c
├── api_net.c
└── api_file.c

components/mod_status/
├── status_led.c
└── patterns.c

main/
├── main.c
└── startup_mgr.c
```

### Frontend Web

```
frontend/
├── src/
│   ├── api/
│   │   ├── client.js
│   │   └── endpoints.js
│   ├── stores/
│   │   ├── systemStore.js
│   │   ├── dmxStore.js
│   │   └── networkStore.js
│   ├── components/
│   │   ├── Dashboard.jsx
│   │   ├── DMXConfig.jsx
│   │   ├── NetworkConfig.jsx
│   │   ├── SystemSettings.jsx
│   │   └── shared/
│   │       ├── Card.jsx
│   │       ├── Button.jsx
│   │       └── Input.jsx
│   ├── App.jsx
│   └── main.jsx
└── package.json
```

---

## KET LUAN

BUILD_PLAN nay cung cap:

- ✅ **9 giai doan logic** tu foundation → production
- ✅ **Dependencies ro rang** giua cac modules
- ✅ **Checkpoints cu the** de verify tien do
- ✅ **Test strategy** cho moi giai doan
- ✅ **Timeline uoc tinh** thuc te (24-36 ngay)

**Developer co the bat tay trien khai theo dung thu tu nay ma khong bi blocked.**
