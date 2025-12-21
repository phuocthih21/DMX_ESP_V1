# WEB.md – WEB SERVER & WEB UI SPECIFICATION (v1.0)

---

## A. MUC DICH & PHAM VI

Tai lieu nay la **dac ta ky thuat CHINH THUC (Source of Truth)** cho toan bo lop Web trong du an DMX Node, bao gom:

- Web Server (MOD_WEB)
- REST API
- Web UI (Frontend SPA)

Web module **khong chi la giao dien**, ma la:
- Cong cu cau hinh thiet bi (configuration)
- Cong cu giam sat (monitoring)
- Cong cu van hanh / commissioning

**Rang buoc bat buoc**:
- Web la lop **non-realtime**
- Web **khong duoc** anh huong toi DMX timing
- Moi tuong tac voi he thong deu thong qua SYS_MOD

---

## B. NGUYEN TAC THIET KE HE THONG

### B.1 Ranh gioi kien truc

- Khong truy cap truc tiep MOD_DMX
- Khong goi ham realtime
- Khong lock mutex cua DMX task
- Khong cap phat bo nho lon trong callback HTTP

Web chi:
- Doc trang thai da cache
- Gui yeu cau cau hinh

---

### B.2 Phan bo tai nguyen

- Chay tren Core 0
- Priority thap hon DMX Engine
- Tat ca xu ly HTTP phai non-blocking

---

## C. WEB SERVER REQUIREMENTS (MOD_WEB)

### C.1 Giao thuc

- HTTP/1.1
- RESTful API
- WebSocket (optional – chi dung de stream status)

---

### C.2 Static File Serving

- Serve SPA (Single Page Application)
- File build san (gzip)
- Khong render dong
- Khong can biet logic frontend

---

### C.3 Bao mat

- Password hoac token (optional)
- Chi cho phep LAN access
- Co the disable Web trong production

---

## D. API DESIGN REQUIREMENTS

### D.1 Nguyen tac chung

- Stateless
- JSON only
- Versioned API
- Prefix: /api/

Tat ca API tra ve dang:

```json
{
  "ok": true,
  "data": { },
  "error": null
}
```

---

### D.2 System API

#### GET /api/sys/info

Thong tin he thong:
- Device ID
- Firmware version
- Uptime
- CPU / RAM usage

---

#### POST /api/sys/reboot

- Khoi dong lai thiet bi
- Can xac nhan

---

#### POST /api/sys/factory

- Reset NVS
- Khoi dong lai

---

### D.3 DMX API

#### GET /api/dmx/status

- Trang thai moi port
- Universe mapping
- Backend (RMT / UART)
- Activity counter

---

#### POST /api/dmx/config

- Mapping port ↔ universe
- Enable / disable port
- Apply non-blocking

---

### D.4 Network API

#### GET /api/network/status

- Ethernet / Wi-Fi status
- IP config hien tai

---

#### POST /api/network/config

- IP
- Netmask
- Gateway
- Apply sau (background)

---

## E. WEB UI (FRONTEND) REQUIREMENTS

### E.1 Tong quan

- SPA (Single Page Application)
- Load nhanh
- Chay tot tren mobile va desktop

---

### E.2 Man hinh bat buoc

#### Dashboard

- Device status
- CPU / RAM usage
- DMX activity

---

#### DMX Mapping

- Port A/B/C/D
- Universe assignment
- Enable / disable

---

#### Network

- Ethernet / Wi-Fi
- IP config

---

#### System

- Reboot
- Factory reset

---

## F. UX / SAFETY RULES

- Range check tat ca input
- Xac nhan thao tac nguy hiem
- Trang thai ro rang (pending / applied)
- Khong cho phep spam request

---

## G. RUNTIME BEHAVIOR

- Thay doi config **khong duoc** lam giat DMX
- Luu NVS o background
- Apply config thong qua state machine

---

## H. LOGGING & DIAGNOSTICS

- Hien thi log muc INFO / WARN
- Error code de debug
- Khong expose log raw realtime

---

## I. MO RONG TUONG LAI

- User role (admin / operator)
- OTA update
- Cloud monitoring (optional)

---

**Trang thai tai lieu**: FINAL – v1.0
**Vai tro**: SPEC CHINH THUC
