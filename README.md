# DMX_ESP_V1 🔧💡

**Mô tả ngắn:**
Firmware cho ESP32‑S3 dùng để điều khiển thiết bị DMX, kèm giao diện mạng và web frontend. Dự án tổ chức theo mô-đun (components) với tài liệu thiết kế và API trong thư mục `Doc_all/`.

---

## 🚀 Tính năng chính
- Điều khiển DMX (module: `mod_dmx`)
- Giao tiếp mạng và cấu hình (module: `mod_net`)
- Giao thức điều khiển nội bộ (module: `mod_proto`)
- API web và backend (module: `mod_web`) cùng frontend (thư mục `frontend/`)
- Module trạng thái/giám sát (module: `mod_status`)

## 📁 Cấu trúc dự án (tóm tắt)
- `main/` — mã nguồn firmware chính
- `components/` — các module firmware (mod_dmx, mod_net, mod_proto, ...)
- `frontend/` — giao diện web (vite + npm)
- `Doc_all/` — tài liệu thiết kế, API, hướng dẫn
- `build/`, `sdkconfig*`, `CMakeLists.txt` — cấu hình build ESP‑IDF

## 🧰 Yêu cầu môi trường
- ESP32‑S3 (target)
- ESP‑IDF (đã thử nghiệm với v5.2.6)
- Node.js & npm (để chạy/build frontend)

## 🛠 Hướng dẫn nhanh — Firmware (ESP‑IDF)
1. Thiết lập ESP‑IDF (PowerShell ví dụ):
```powershell
# set IDF_PATH nếu cần
$env:IDF_PATH = 'E:/Espressif/frameworks/esp-idf-v5.2.6/'
. $env:IDF_PATH/export.ps1
```
2. Cấu hình và build:
```bash
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p <COM_PORT> flash monitor
```

## 🖥 Frontend (phát triển / chạy)
```bash
cd frontend
npm install
npm run dev     # chạy dev server
npm run build   # build production
```
> Giao diện web truy cập tại: `http://<device_ip>/` (xem `Doc_all/` để biết API chi tiết)

## ⚙️ Cấu hình & Tài liệu
- Cấu hình firmware: `sdkconfig`, `partitions.csv`
- Tài liệu thiết kế/API: xem `Doc_all/` (API‑Contract, ARCHITECTURE, MOD_*)

## 🧪 Kiểm thử & Debug
- Sử dụng `idf.py monitor` để xem log thiết bị.
- Frontend: dùng console dev (vite) để debug giao diện.

## 🤝 Contributing
- Mở issue hoặc PR; xem `ISSUES_PROGRESS.md` và `README_ISSUES.md` để biết tiến trình.
- Viết mô tả thay đổi rõ ràng khi tạo PR.

## 📄 License
Hiện tại repo chưa có file `LICENSE` — vui lòng xác nhận license mong muốn (ví dụ MIT) để mình thêm vào.

---

Nếu bạn muốn mình chỉnh sửa (thêm badges, ảnh, English translation) hoặc ghi file này vào repo, trả lời kèm tùy chọn bạn muốn: (1) Ghi thay thế ngay, (2) Ghi sang `README.md` giữ nguyên bản cũ thành `README.old.md` trước khi thay, (3) Chỉnh nội dung.
