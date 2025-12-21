# MOD_STARTUP (Boot Manager)

## 1. Thong tin Module

- **Module Name**: Boot Manager
- **Module ID**: `STARTUP`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `STARTUP.md` (Goc), `SPECIFICATION.md`

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Quan ly quy trinh khoi dong he thong ngay khi cap dien.
- Quyet dinh che do chay: Normal, Rescue (AP Mode), hoac Factory Reset.
- Bao ve he thong khoi vong lap khoi dong (Boot Loop Protection).

**Module nay KHONG lam gi:**

- Khong khoi tao chi tiet tung module (Chi goi ham init cua module khac).
- Khong luu tru cau hinh he thong (Viec cua SYS_MOD).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-BOOT-01**: Doc `crash_counter` tu NVS ngay khi Power On.
- **FR-BOOT-02**: Neu `crash_counter` >= 3, tu dong chuyen sang `RESCUE_MODE`.
- **FR-BOOT-03**: Kiem tra nut nhan vat ly (GPIO 0).
  - Giu 3s -> `RESCUE_MODE`.
  - Giu 10s -> `FACTORY_RESET`.
- **FR-BOOT-04**: Tu dong Reset `crash_counter` ve 0 neu he thong chay on dinh > 60s.
- **FR-BOOT-05**: Khoi dong cac module theo thu tu: HAL -> SYS -> NET -> PROTO/DMX -> WEB.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-BOOT-01**: Thoi gian quyet dinh Boot Mode < 500ms.
- **NFR-BOOT-02**: Khong dung `malloc` dong trong qua trinh check boot (de phong loi Heap).
- **NFR-BOOT-03**: Log day du thong tin boot (Mode, Reason) qua UART.

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
boot_mode_t startup_get_mode(void);
void startup_mark_as_stable(void);
```

## 6. Cau truc du lieu (Data Structures)

**Enums:**

- `boot_mode_t`: `NORMAL`, `RESCUE`, `FACTORY_RESET`.

## 7. Luong xu ly (Processing Flow)

### 7.1 Power On Sequence

1. Init NVS, GPIO.
2. Doc & Tang `crash_counter`. Tang ngay lap tuc de catch loi neu sap xay ra.
3. Check GPIO Button.
4. Xac dinh Mode.
5. `app_main` switch(Mode):
   - Case NORMAL: Init Full Stack.
   - Case RESCUE: Init Wifi AP + Web (Skip DMX, Proto).
   - Case RESET: Erase NVS, Reboot.

## 8. Quan ly loi va trang thai

- **Boot Loop**: Duoc xu ly boi Logic `crash_counter`.
- **Brownout**: Can tat Brownout Detector tam thoi neu nguon yeu khi khoi dong Wifi.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `nvs_flash`, `driver/gpio`.
- **Khoi tao**: Chay truoc ca `sys_mod_init`. 

## 10. Cau hinh (Configuration)

- `BOOT_CRASH_THRESHOLD`: 3 lan.
- `BOOT_STABLE_TIME_MS`: 60000 (60s).
- `BOOT_BUTTON_PIN`: GPIO 0.

## 11. Rang buoc ky thuat (Constraints)

- **NVS**: Bien `crash_counter` phai nam trong Namespace `boot_cfg` rieng biet de khong bi xoa khi Factory Reset nham.

## 12. Kich ban su dung (Use Case)

**UC-01: Cuu ho (Rescue)**

1. User flash nham Firmware loi hoac Config sai lam thiet bi Crash lien tuc.
2. Thiet bi reboot 3 lan.
3. Lan thu 4, `STARTUP` thay counter=3 -> Activate Rescue Mode.
4. Phat Wifi "DMX-Rescue".
5. User ket noi Wifi, vao Web up lai Firmware/Config dung.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Tao `boot_protect.c` doc/ghi NVS counter.
- [ ] Viet logic check nut bam bang `esp_timer` hoac delay loop.
- [ ] Implement `app_main` switch-case theo Mode.

## 14. Lien ket cheo tai lieu

- `DIRECTORY_TREE.md`: Vi tri `startup_mgr.c`.
- `MOD_NET.md`: Che do AP duoc goi khi Rescue.

## 15. Pham vi mo rong ve sau

- Ho tro Safe Boot (tu dong rollback OTA partition).
- Ho tro den bao hieu so lan Crash.

## 16. Tieu chi hoan thanh (Definition of Done)

- Phat hien dung Reset lien tuc.
- Nut nhan phan hoi chinh xac 3s/10s.
- Hien thi dung Mode tren Log.
