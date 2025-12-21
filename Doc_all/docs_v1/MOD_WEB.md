# MOD_WEB (User Interface)

## 1. Thong tin Module

- **Module Name**: Web User Interface
- **Module ID**: `MOD_WEB`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `MOD_WEB.md` (Goc), `Web.md`

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Cung cap Dashboard cau hinh qua trinh duyet.
- Xu ly REST API (JSON) de doc/ghi trang thai he thong.
- Phuc vu file tinh (HTML/JS/CSS).

**Module nay KHONG lam gi:**

- Khong truc tiep thay doi phan cung (Phai goi qua API cua SYS, NET).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-WEB-01**: Phuc vu trang chu (`index.html`) nen Gzip.
- **FR-WEB-02**: API doc thong tin status (IP, DMX Ports).
- **FR-WEB-03**: API ghi cau hinh DMX (Mapping, Timing).
- **FR-WEB-04**: API dieu khien he thong (Reboot, Factory Reset).
- **FR-WEB-05**: API nhan file config Import/Export.

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-WEB-01**: Web server chay Low Priority (Core 0).
- **NFR-WEB-02**: Response time API < 200ms.
- **NFR-WEB-03**: Ho tro truy cap dong thoi (Concurrent) nhung xu ly tuan tu (Serialize Config Write).

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
esp_err_t web_init(void);
```

**Endpoints:**

- `GET /` -> Serve index.html
- `GET /api/sys/info`
- `POST /api/dmx/config`
- `POST /api/net/config`

## 6. Cau truc du lieu (Data Structures)

- JSON Models (Mapping voi `sys_config_t`).

## 7. Luong xu ly (Processing Flow)

### 7.1 Request Handling

1. HTTP Request den.
2. Router kiem tra URL.
3. Neu la API -> Parse JSON Body (cJSON).
4. Goi `sys_update_...` tuong ung.
5. Tao JSON Response (`{"status": "ok"}`).
6. Send Response.

## 8. Quan ly loi va trang thai

- **HTTP 400**: JSON sai format.
- **HTTP 405**: Method not allowed.
- **HTTP 500**: System Internal Error (Mutex timeout).

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `esp_http_server`, `cJSON`.
- **System**: `sys_mod` (Backend logic).

## 10. Cau hinh (Configuration)

- `WEB_MAX_CLIENTS`: 4.
- `WEB_STACK_SIZE`: 4096 bytes.

## 11. Rang buoc ky thuat (Constraints)

- **Storage**: Giao dien Web phai nho (< 200KB Gzip) de nhung vao Flash.
- **Javascript**: Client-side rendering (SPA). Firmware khong render HTML.

## 12. Kich ban su dung (Use Case)

**UC-01: User thay doi IP**

1. User vao tab Network, nhap static IP.
2. Browser gui POST `/api/net/config`.
3. `MOD_WEB` goi `sys_update_net_cfg`.
4. Tra ve OK.
5. User mat ket noi (do IP doi).

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Implement `mod_web.c` khoi tao Server.
- [ ] Implement `api_handler.c` xu ly JSON.
- [ ] Convert Frontend build (Dist) thanh `html_gz.c` (Array C).

## 14. Lien ket cheo tai lieu

- `MOD_NET.md`: Ha tang mang.
- `sys_api.h`: API backend.

## 15. Pham vi mo rong ve sau

- Login/Auth.
- WebSocket cho realtime status update (Vuot muc den V4.0).

## 16. Tieu chi hoan thanh (Definition of Done)

- Truyen cap duoc Dashboard tu trinh duyet.
- API tra ve JSON dung format.
- Import/Export file config hoat dong tot.
