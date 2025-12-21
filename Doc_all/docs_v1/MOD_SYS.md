# MOD_SYS (System Core)

## 1. Thong tin Module

- **Module Name**: System Core
- **Module ID**: `SYS_MOD`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `Mod_SYS.md` (Goc), `API-Contract.md`

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Quan ly toan bo cau hinh he thong (Config Life-cycle).
- Cung cap bo nho dem (Buffer) cho DMX Data.
- Dinh tuyen du lieu tu Protocol sang Driver.
- Quan ly OTA Update va Factory Reset.

**Module nay KHONG lam gi:**

- Khong tu truc tiep xuat tin hieu DMX (trach nhiem cua MOD_DMX).
- Khong xu ly goi tin mang UDP (trach nhiem cua MOD_PROTO).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-SYS-01**: Khoi tao bien cau hinh default khi NVS trong.
- **FR-SYS-02**: Validate moi du lieu cau hinh truoc khi ghi vao RAM.
- **FR-SYS-03**: Cung cap API lay pointer read-only cua Config ma khong block.
- **FR-SYS-04**: Tu dong kich hoat Lazy Save (ghi NVS sau 5s) khi co thay doi Config.
- **FR-SYS-05**: Cung cap vung nho dem 512 bytes cho moi Port DMX.
- **FR-SYS-06**: Quan ly trang thai Fail-safe (Timeout) cua tung Port.
- **FR-SYS-07**: Luu/Khoi phuc du lieu Snapshot tu NVS Blob.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-SYS-01**: Thread-safety cho moi thao tac ghi Config (Mutex Protected).
- **NFR-SYS-02**: Buffer Read/Write phai la Atomic hoac it nhat la Safe Pointer (khong duoc free).
- **NFR-SYS-03**: Tai nguyen RAM tinh: ~4KB (Config + Buffer).

## 5. Giao tiep Module (Module Interface)

Tham khao chi tiet tai: `docs_v1/API-Contract.md`

**Public API:**

```c
esp_err_t sys_mod_init(void);
const sys_config_t* sys_get_config(void);
esp_err_t sys_update_port_cfg(int port_idx, const dmx_port_cfg_t *new_cfg);
uint8_t* sys_get_dmx_buffer(int port_idx);
void sys_notify_activity(int port_idx);
esp_err_t sys_ota_begin(size_t image_len);
```

## 6. Cau truc du lieu (Data Structures)

**Public Structs:**

- `sys_config_t`: Cau hinh toan cuc (Common types).
- `dmx_port_cfg_t`: Cau hinh tung port.
- `dmx_failsafe_t`: Cau hinh an toan.

**Internal Structs:**

- `sys_state_t`: Trang thai Runtime (Dirty flag, Timer handle).

## 7. Luong xu ly (Processing Flow)

### 7.1 Khoi tao (`sys_mod_init`) 

1. Khoi tao NVS Flash.
2. Doc `sys_config_t` tu NVS. Neu loi CRC hoac khong co -> Load Default.
3. Cap phat 4 bufer DMX trong PSRAM/DRAM.
4. Tao Mutex bao ve config.

### 7.2 Hot-swap Config Update

1. Web goi API `sys_update_port_cfg`.
2. Validate du lieu (Range check).
3. Lock Mutex.
4. `memcpy` config moi vao RAM.
5. Unlock Mutex.
6. Gui Signal cho `MOD_DMX`.
7. Reset Timer Lazy Save (5s).

## 8. Quan ly loi va trang thai

- **ERR_SYS_NVS_FAIL**: Loi phan cung Flash -> Thu Format NVS -> Neu van loi: Bao den RED.
- **ERR_SYS_INVALID_ARG**: Tham so cau hinh sai -> Tra ve ESP_ERR_INVALID_ARG, khong luu.
- **ERR_SYS_OTA_FAIL**: Checksum sSai -> Rollback ve phan vung cu.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `nvs_flash`, `esp_partition` (ESP-IDF).
- **Thu tu khoi tao**: Phai khoi tao DAU TIEN truo khi `MOD_NET` hay `MOD_DMX` chay.

## 10. Cau hinh (Configuration)

Dinh nghia trong `menuconfig` hoac header:

- `SYS_DEFAULT_NAME`: "DMX-Node-V4"
- `SYS_LAZY_SAVE_MS`: 5000

## 11. Rang buoc ky thuat (Constraints)

- **Flash**: Su dung phan vung NVS co dinh.
- **OS**: FreeRTOS.

## 12. Kich ban su dung (Use Case)

**UC-01: Luu config tu Web**

1. User doi Break Time = 300us tren Web.
2. `MOD_WEB` goi `sys_update_port_cfg(0, {break:300})`.
3. `SYS_MOD` validate (300 nam trong 88-500).
4. `SYS_MOD` update RAM.
5. Sau 5s, `SYS_MOD` thuc hien ghi xuong Flash.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Tao file `sys_manager.c`, `sys_config.c`.
- [ ] Implement `sys_mod_init` load NVS.
- [ ] Implement Mutex cho `g_sys_config`.
- [ ] Implement Buffer allocation.
- [ ] Tao task Auto-save.

## 14. Lien ket cheo tai lieu

- `ARCHITECTURE.md`: Xem phan Data Flow.
- `MOD_DMX.md`: Module tieu thu du lieu tu Buffer.

## 15. Pham vi mo rong ve sau

- Ho tro luu nhieu Profile cau hinh kha nhau.
- Ho tro ma hoa Config.

## 16. Tieu chi hoan thanh (Definition of Done)

- Moi thay doi config deu duoc luu lai sau Reboot.
- Khong co Race condition khi Web va DMX cung truy cap Config.
- Buffer DMX luon hop le (khong NULL).
