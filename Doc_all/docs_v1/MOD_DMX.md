# MOD_DMX (Driver Engine)

## 1. Thong tin Module

- **Module Name**: DMX Driver Engine
- **Module ID**: `MOD_DMX`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `MOD_DMX.md` (Goc), `Timing.md`

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Dieu khien ngoai vi RMT va UART de tao tin hieu DMX512.
- Duy tri vong lap gui tin hieu (Refresh Rate) on dinh 40Hz.
- Tu dong giu (Hold) hoac tat (Blackout) khi mat du lieu nguon.

**Module nay KHONG lam gi:**

- Khong quyet dinh noi dung du lieu (Lay tu SYS_MOD).
- Khong xu ly logic protocol nhu ArtNet/sACN.

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-DMX-01**: Xuat tin hieu DMX512 chuan.
  - Baud rate: 250,000 bps.
  - Break: >= 88us (Default: 176us, Configurable 88-500us).
  - MAB: >= 8us (Default: 12us, Configurable 8-100us).
- **FR-DMX-02**: Cho phep thay doi Timing (Break/MAB) Runtime.
  - Tinh toan RMT Ticks: `Ticks = us * 10` (Resolution 0.1us).
- **FR-DMX-03**: Refresh Rate on dinh o muc ~40Hz (+/- 1Hz).
  - Frame Time: ~25ms.
  - Slot time: 44us/byte.
- **FR-DMX-04**: Ho tro RMT tren Port A/B va UART tren Port C/D.
- **FR-DMX-05**: Tu dong chuyen che do Fail-safe neu khong co `sys_notify_activity` trong thoi gian timeout.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-DMX-01**: Jitter tin hieu ra < 1us (voi RMT).
  - RMT HW Jitter: < 0.05us.
  - ISR Latency: < 0.1us.
- **NFR-DMX-02**: Task phai chay tren Core 1, Priority cao nhat.
- **NFR-DMX-03**: Khong su dung `malloc` trong vong lap realtime.
- **NFR-DMX-04**: Tat ca ISR code phai dat trong IRAM (`IRAM_ATTR`).

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
esp_err_t dmx_driver_init(const sys_config_t *cfg);
void dmx_start(void);
void dmx_stop(void);
void dmx_apply_new_timing(int port, const dmx_timing_t *timing);
```

## 6. Cau truc du lieu (Data Structures)

**Internal Structs:**

- `dmx_driver_ctx_t`: Context quan ly tung Port (RMT channel handle, UART number, buffer pointer).

## 7. Luong xu ly (Processing Flow)

### 7.1 Vong lap chinh (40Hz)

1. **Wait**: `vTaskDelay` de dam bao du chu ky 25ms.
2. **Watchdog Check**:
   - Doc `last_activity_ts` cua tung port.
   - Neu `now - last > timeout`: Active Failsafe (Hold/Snap/Blackout).
   - Neu `now - last < timeout`: Active Normal (Read SYS Buffer).
3. **Data Fetch**:
   - `uint8_t *data = sys_get_dmx_buffer(i)`.
4. **Encoding (RMT Only)**:
   - Chuyen doi Byte -> RMT Items.
   - Start Bit (Low 4us) + 8 Data Bits + 2 Stop Bits (High 8us).
   - Su dung `rmt_write_items`.
5. **RDM Check**:
   - Neu la RDM request, chuyen chan DE (Data Enable) xuong Low trong 176us.
   - Su dung `esp_rom_delay_us()` (Khong dung vTaskDelay).

### 7.2 Shutdown

1. `dmx_stop()` -> Disable Timer, Disable GPIO.
2. Dam bao chan tin hieu ve muc High (Idle).

## 8. Quan ly loi va trang thai

- **ERR_DMX_RMT_INIT**: Het kenh RMT hoac loi GPIO -> Log Error, thu chuyen sang UART (neu duoc).
- **ERR_DMX_TIMEOUT**: Mat tin hieu Input -> Chuyen Fail-safe, tat LED xanh.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `sys_mod` (Buffer & Config).
- **Phan cung**: `driver/rmt`, `driver/uart`.

## 10. Cau hinh (Configuration)

- `DMX_RMT_CLK_DIV`: 8 (80MHz / 8 = 10MHz Resolution).
- `DMX_UART_BAUD`: 250000.

## 11. Rang buoc ky thuat (Constraints)

- **Phan cung**: ESP32 / ESP32-S3.
- **Xung dot**: Canh bao xung dot voi `ws2812` neu dung chung RMT group.

## 12. Kich ban su dung (Use Case)

**UC-01: Hoat dong binh thuong**

1. `MOD_PROTO` nhan ArtNet, ghi vao Buffer, goi `sys_notify_activity`.
2. `MOD_DMX` thuc day sau 25ms.
3. Thay moi Active -> Doc Buffer -> Gui ra chan RS485.

**UC-02: Mat tin hieu**

1. Rut day mang.
2. `MOD_DMX` sau 2s (Timeout) khong thay Active.
3. Kiem tra Config `failsafe.mode`.
4. Neu la HOLD: Tiep tuc gui Buffer cu.
5. Neu la BLACKOUT: Gui mang toan 0x00.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Implement `dmx_rmt.c` voi RMT Encoder.
- [ ] Implement `dmx_uart.c` voi UART RS485 mode.
- [ ] Tao DMX Task loop chinh xac thoi gian.
- [ ] Test logic Fail-safe (rut day).

## 14. Lien ket cheo tai lieu

- `MOD_SYS.md`: Nguon du lieu.
- `MOD_STATUS.md`: Bao trang thai loi.

## 15. Pham vi mo rong ve sau

- Ho tro RDM Discovery/Responder (Chieu nguoc).
- Ho tro protocol khac truc tiep (DALI - Optional).

## 16. Tieu chi hoan thanh (Definition of Done)

- Tin hieu ra do bang Logic Analyzer thay vuong vuc, Timing dung.
- Khong bi Crash khi chay stress test 24h.
- Hot-swap Timing hoat dong tuc thi.
