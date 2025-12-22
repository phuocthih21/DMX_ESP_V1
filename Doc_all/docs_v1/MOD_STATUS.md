# MOD_STATUS (Indicator)

## 1. Thong tin Module

- **Module Name**: Status Indicator
- **Module ID**: `MOD_STATUS`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `MOD_STATUS.md` (Goc)

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Dieu khien LED WS2812B bao trang thai he thong.
- Tao cac hieu ung nhay (Blink, Breathe, Strobe).

**Module nay KHONG lam gi:**

- Khong quyet dinh logic loi (Module khac phai bao loi cho no).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-LED-01**: Hien thi dung mau theo ma loi (Code).
- **FR-LED-02**: Thuc hien hieu ung muot ma (Dimming).
- **FR-LED-03**: Uu tien hien thi trang thai quan trong (Loi > Canh bao > Thong tin).
- **FR-LED-04**: Ho tro chinh do sang Global.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-LED-01**: Khong dung CPU Bit-banging (Dung RMT).
- **NFR-LED-02**: Priority thap nhat (Idle priority).

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
esp_err_t status_init(int gpio_pin);
void status_set_code(status_code_t code);
void status_set_brightness(uint8_t percent);
```

## 6. Cau truc du lieu (Data Structures)

**Enums:**

- `status_code_t`: Cac ma loi (BOOT, ETH, WIFI, OTA, ERR...).

## 7. Luong xu ly (Processing Flow)

### 7.1 Task Loop

1. Doc `current_code`.
2. Tinh toan mau RGB va Do sang dua tren thoi gian (Hieu ung tho/nhay).
3. Gui du lieu xuong RMT.
4. Delay 20-50ms.

## 8. Quan ly loi va trang thai

- Module nay la noi HIENTHI loi, ban than no it khi gay loi.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `driver/rmt`.
- **Phan cung**: LED WS2812B @ GPIO 48.

## 10. Cau hinh (Configuration)

- `STATUS_LED_PIN`: 48.
- `STATUS_RMT_CHANNEL`: 7.

## 11. Rang buoc ky thuat (Constraints)

- Khong duoc dung CHANNEL 0-3 cua RMT (Danh cho DMX Output).

## 12. Kich ban su dung (Use Case)

**UC-01: OTA Update**

1. `SYS_MOD` goi `status_set_code(STATUS_OTA)`.
2. LED chuyen sang mau Tim, nhay nhanh (Strobe).
3. Nguoi dung biet khong duoc rut dien.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Init RMT Channel 7.
- [ ] Viet ham tao hieu ung Breathing (Sin wave).
- [ ] Viet Task loop kiem soat Timing.

## 14. Lien ket cheo tai lieu

- `API-Contract.md`: Dinh nghia cac Status Code.

## 15. Pham vi mo rong ve sau

- Ho tro LED 2 mau hoac LED thuong.
- Ho tro man hinh OLED/LCD.

## 16. Tieu chi hoan thanh (Definition of Done)

- LED sang dung mau.
- Khong bi flickr khi he thong tai nang.
- Hieu ung tho dep, muot.
