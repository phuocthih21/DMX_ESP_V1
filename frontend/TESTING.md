# Hướng dẫn Test Frontend với Mock Server

## Bước 1: Khởi động Mock Server

```bash
cd tools/mock_sys_mod
npm install  # Chỉ cần chạy lần đầu
npm start
```

Mock server sẽ chạy tại: `http://localhost:3000`

## Bước 2: Khởi động Frontend

Mở terminal mới:

```bash
cd frontend
npm install  # Chỉ cần chạy lần đầu
npm run dev
```

Frontend sẽ chạy tại: `http://localhost:5173`

## Bước 3: Kết nối Frontend với Mock Server

1. Mở trình duyệt: `http://localhost:5173`
2. Ở thanh trên cùng, bạn sẽ thấy "Backend URL" input
3. Nhập: `localhost:3000` (hoặc để mặc định)
4. Click "Connect"
5. Bạn sẽ thấy indicator "Connected" màu xanh

## Kiểm tra

- **Dashboard**: Sẽ hiển thị system info, DMX ports, network status từ mock server
- **DMX Config**: Có thể xem và cấu hình DMX ports
- **Network Config**: Xem network status
- **System Settings**: Các chức năng system

## WebSocket

Mock server tự động gửi updates mỗi giây qua WebSocket. Bạn sẽ thấy:
### OTA

- To test OTA upload behavior, try `curl -I -X POST http://localhost:3000/api/system/ota` to check whether the endpoint exists (404/405 indicates not implemented).
- The UI will display a friendly message if the device does not implement the OTA API.- Uptime tăng dần
- CPU load thay đổi
- Network status có thể thay đổi ngẫu nhiên

## Troubleshooting

### Frontend không kết nối được
- Kiểm tra mock server đang chạy: `curl http://localhost:3000/api/system/info`
- Kiểm tra console browser (F12) xem có lỗi CORS không
- Đảm bảo nhập đúng URL: `localhost:3000` (không cần http://)

### WebSocket không kết nối
- Kiểm tra mock server log xem có "WS client connected" không
- Kiểm tra browser console có lỗi WebSocket không
- WebSocket sẽ tự động retry nếu thất bại

### Dữ liệu không hiển thị
- Kiểm tra Network tab trong DevTools xem API calls có thành công không
- Kiểm tra Response tab xem data có đúng format không

