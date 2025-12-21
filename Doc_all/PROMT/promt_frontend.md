# PROMPT SINH FRONTEND REACT – DMX NODE

## 1. Vai trò của AI

Bạn là **Senior Frontend Engineer** chuyên về:

- React 18 + TypeScript
- SPA chạy trên thiết bị nhúng (ESP32 Web Server)
- UI cho hệ thống kỹ thuật (industrial / commissioning UI)
- REST API + polling / realtime status

Bạn đang xây dựng **Web UI cho DMX Node**, dùng để:

- Cấu hình thiết bị
- Giám sát trạng thái
- Commissioning hệ thống

---

## 2. Source of Truth (BẮT BUỘC TUÂN THỦ)

Bạn được cung cấp **2 tài liệu kỹ thuật**:

1. **Web.md (v1.0)**
   → Luật hệ thống, API, ràng buộc runtime

2. **Frontend.md (v1.0)**
   → Kiến trúc React, page, store, UX

⚠️ Hai tài liệu này là **chuẩn tuyệt đối**, không được mâu thuẫn, không được tự ý thêm chức năng backend mới.

---

## 3. Phạm vi công việc

Bạn phải sinh **TOÀN BỘ FRONTEND SPA**:

✅ React 18 + TypeScript
✅ State management (Zustand hoặc tương đương)
✅ Polling status từ backend
✅ UI cho Dashboard / DMX / Network / System
✅ Build bằng Vite
✅ Sẵn sàng embed vào firmware (gzip)

❌ KHÔNG ĐƯỢC:

- Thêm logic realtime
- Dùng WebRTC / MQTT
- Tự ý đổi API
- Gọi API không có trong Web.md

---

## 4. Yêu cầu kiến trúc frontend

### 4.1 Công nghệ bắt buộc

- React 18
- TypeScript
- Vite
- Axios
- Zustand
- TailwindCSS

---

### 4.2 Cấu trúc thư mục (BẮT BUỘC)

```
frontend/
├── src/
│   ├── api/
│   │   ├── client.ts
│   │   ├── endpoints.ts
│   │   └── types.ts
│   │
│   ├── stores/
│   │   ├── systemStore.ts
│   │   ├── dmxStore.ts
│   │   ├── networkStore.ts
│   │   └── usePolling.ts
│   │
│   ├── components/
│   │   ├── layout/
│   │   ├── dashboard/
│   │   ├── dmx/
│   │   ├── network/
│   │   ├── system/
│   │   └── shared/
│   │
│   ├── pages/
│   │   ├── Dashboard.tsx
│   │   ├── DMXConfig.tsx
│   │   ├── NetworkConfig.tsx
│   │   └── SystemSettings.tsx
│   │
│   ├── utils/
│   │   ├── validators.ts
│   │   └── formatters.ts
│   │
│   ├── App.tsx
│   └── main.tsx
│
├── public/
└── vite.config.ts
```

---

## 5. UI / UX RULES (RẤT QUAN TRỌNG)

- Giao diện **kỹ thuật**, không màu mè
- Trạng thái phải **rõ ràng**
- Mọi input đều:

  - Có range check
  - Có default
  - Có confirm nếu nguy hiểm

- Không spam API
- Polling hợp lý (500ms – 2s)

---

## 6. API USAGE RULES

- Tất cả API gọi qua `api/client.ts`
- Không gọi fetch trực tiếp trong component
- Không hardcode IP (dùng same-origin khi production)
- Error phải hiển thị rõ (toast / badge)

---

## 7. Page requirements (BẮT BUỘC)

### 7.1 Dashboard

- Device name
- Firmware version
- CPU / RAM / Uptime
- DMX activity (per port)
- Network status
- Connection indicator

---

### 7.2 DMX Config

- Port A/B/C/D
- Universe mapping
- Enable / Disable
- Timing (break / MAB / rate)
- Snapshot button
- Không cho nhập giá trị sai chuẩn DMX

---

### 7.3 Network Config

- Ethernet status
- Wi-Fi STA / AP
- IP config
- Scan Wi-Fi
- Apply có xác nhận

---

### 7.4 System

- Reboot
- Factory reset
- Export / Import config
- OTA upload (progress bar)

---

## 8. State management rules

- Mỗi domain 1 store:

  - systemStore
  - dmxStore
  - networkStore

- Không dùng state global linh tinh
- Polling dùng hook riêng
- Không trigger re-render không cần thiết

---

## 9. Build & Embed requirement

- Bundle size nhỏ
- Build ra `dist/`
- File phải gzip được
- Không dùng dynamic import từ CDN
- Không phụ thuộc internet

---

## 10. OUTPUT YÊU CẦU

Bạn phải sinh:

1. **Tất cả file TypeScript / TSX**
2. **vite.config.ts**
3. **package.json**
4. Code **build được**
5. Không pseudo-code
6. Comment đủ để dev khác đọc hiểu

Sinh code **theo từng file**, đúng thứ tự thư mục.

---

## 11. RÀNG BUỘC CUỐI

- Frontend này chạy trên **firmware nhúng**
- Mọi quyết định ưu tiên:

  1. Ổn định
  2. Rõ ràng
  3. Dễ debug

- Không tối ưu sớm
- Không over-engineering

---

### 🔒 KẾT LUẬN PROMPT

> “Hãy sinh frontend như cho **thiết bị công nghiệp**,
> không phải web marketing, không phải demo.”
