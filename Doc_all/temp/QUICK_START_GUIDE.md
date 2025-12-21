# 🚀 Quick Start Guide - Backend & Frontend Integration

## 📚 Tài liệu đã tạo

1. **PLAN_BACKEND_FRONTEND_INTEGRATION.md** - Kế hoạch chi tiết triển khai
2. **IMPLEMENTATION_CHECKLIST.md** - Checklist từng bước
3. **API_MAPPING_FRONTEND_BACKEND.md** - Mapping API giữa Frontend ↔ Backend ↔ SYS_MOD

---

## 🎯 Tổng quan

### Mục tiêu
Xây dựng **MOD_WEB Backend** (ESP-IDF) để:
- Phục vụ Frontend React SPA (static files)
- Cung cấp REST API cho Frontend
- Cung cấp WebSocket cho realtime updates
- Kết nối với SYS_MOD để quản lý hệ thống

### Kiến trúc
```
Frontend (React) 
    ↓ HTTP/WS
MOD_WEB (Backend)
    ↓ API Calls
SYS_MOD (Core)
    ↓
MOD_DMX, MOD_NET, MOD_PROTO
```

---

## 📋 Các bước triển khai

### Bước 1: Tạo cấu trúc thư mục

```bash
mkdir -p components/mod_web/{include,src/{api,ws,static,utils},assets}
```

### Bước 2: Implement Core Infrastructure

1. **mod_web.h** - Public API
2. **mod_web.c** - Module initialization
3. **mod_web_server.c** - HTTP server setup
4. **CMakeLists.txt** - Build configuration

### Bước 3: Implement REST API Handlers

1. **api_system.c** - System endpoints
2. **api_dmx.c** - DMX endpoints
3. **api_network.c** - Network endpoints
4. **json_utils.c** - JSON helpers
5. **validation.c** - Input validation

### Bước 4: Implement WebSocket

1. **ws_handler.c** - Connection management
2. **ws_broadcast.c** - Message broadcasting
3. **ws_events.c** - Event subscription

### Bước 5: Static File Serving

1. Build script để gzip frontend assets
2. Convert to C arrays
3. **static_handler.c** - File serving

### Bước 6: Integration & Testing

1. Test với Frontend
2. Verify all endpoints
3. Performance testing

---

## 🔌 API Endpoints Summary

### System
- `GET /api/sys/info` - System information
- `POST /api/sys/reboot` - Reboot device
- `POST /api/sys/factory` - Factory reset

### DMX
- `GET /api/dmx/status` - DMX port status
- `POST /api/dmx/config` - Update DMX config

### Network
- `GET /api/network/status` - Network status
- `POST /api/network/config` - Update network config

### WebSocket
- `ws://<ip>/ws/status` - Realtime status stream

---

## 📦 Dependencies

### Backend (ESP-IDF)
- `esp_http_server` - HTTP server
- `cJSON` - JSON parsing
- `esp_websocket_server` - WebSocket support

### Frontend (React)
- `axios` - HTTP client
- `zustand` - State management
- WebSocket client (native or library)

---

## 🛠️ Build Process

### 1. Build Frontend
```bash
cd frontend
npm install
npm run build
```

### 2. Generate Assets
```bash
# Gzip files
cd dist
gzip -9k index.html
gzip -9k assets/*.js
gzip -9k assets/*.css

# Convert to C arrays
xxd -i index.html.gz > ../../components/mod_web/assets/index_html.c
xxd -i assets/index-*.js.gz > ../../components/mod_web/assets/app_js.c
xxd -i assets/index-*.css.gz > ../../components/mod_web/assets/app_css.c
```

### 3. Build Firmware
```bash
idf.py build
idf.py flash monitor
```

---

## ✅ Verification Checklist

- [ ] HTTP server starts successfully
- [ ] Static files load in browser (`http://<ip>/`)
- [ ] All REST API endpoints respond correctly
- [ ] WebSocket connects and receives messages
- [ ] Frontend can update DMX config
- [ ] Frontend can update network config
- [ ] System reboot works
- [ ] Factory reset works

---

## 🐛 Troubleshooting

### Static files không load
- Check Gzip compression
- Verify C array declarations
- Check Content-Encoding header

### API returns 404
- Verify URI registration
- Check route paths match frontend

### WebSocket không connect
- Check WebSocket upgrade handler
- Verify event subscription
- Check message format

### Memory issues
- Verify all `cJSON_Delete()` calls
- Check stack size (min 8KB)
- Monitor heap usage

---

## 📖 Tài liệu tham khảo

- **PLAN_BACKEND_FRONTEND_INTEGRATION.md** - Chi tiết implementation
- **API_MAPPING_FRONTEND_BACKEND.md** - API mapping
- **IMPLEMENTATION_CHECKLIST.md** - Step-by-step checklist
- **docs_v1/MOD_WEB.md** - Backend specification
- **DESIGN_MOD/web_socket_spec_v_1.md** - WebSocket spec

---

## 🎓 Next Steps

1. Đọc **PLAN_BACKEND_FRONTEND_INTEGRATION.md** để hiểu chi tiết
2. Follow **IMPLEMENTATION_CHECKLIST.md** để implement từng bước
3. Tham khảo **API_MAPPING_FRONTEND_BACKEND.md** khi implement API handlers
4. Test từng phase trước khi chuyển sang phase tiếp theo

---

**Good luck! 🚀**

