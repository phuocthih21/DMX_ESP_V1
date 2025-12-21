# FRONTEND.md – REACT SPA SPECIFICATION (v1.0)

---

## 1. MUC DICH & VAI TRO

Tai lieu nay mo ta **dac ta ky thuat day du cho Frontend (Web UI)** cua du an DMX Node.

Frontend la:
- Cong cu **commissioning**
- Cong cu **giam sat he thong**
- Cong cu **cau hinh an toan**

Frontend **khong phai**:
- Lighting console
- Realtime DMX controller
- Merge / priority engine

Tat ca rang buoc trong tai lieu nay **bat buoc tuan theo Web.md v1.0**.

---

## 2. PHAM VI & RANH GIOI

### 2.1 Ranh gioi ky thuat

Frontend:
- Chi giao tiep qua REST API (/api)
- Khong truy cap truc tiep firmware core
- Khong thuc hien tinh toan realtime

Khong duoc:
- Gia lap DMX timing
- Dieu khien DMX frame
- Giu state lam anh huong den DMX engine

---

## 3. CONG NGHE & STACK BAT BUOC

- React 18
- TypeScript
- Vite
- Zustand (state management)
- Axios (HTTP client)
- TailwindCSS (UI)

Khong su dung:
- Redux
- SSR
- Heavy animation

---

## 4. KET CAU UNG DUNG

```
frontend/
├── src/
│   ├── api/            # API client (typed)
│   ├── stores/         # Zustand stores
│   ├── pages/          # Page components
│   ├── components/     # Shared UI components
│   ├── types/          # TypeScript interfaces
│   ├── utils/          # Helper functions
│   └── main.tsx
└── index.html
```

---

## 5. MO HINH STATE FRONTEND

### 5.1 Tong quan

Frontend state duoc chia theo **domain firmware**:

```ts
FrontendState = {
  system: SystemState,
  dmx: DMXState,
  network: NetworkState
}
```

---

### 5.2 SystemState

```ts
SystemState = {
  info: SystemInfo | null,
  loading: boolean,
  error: string | null
}
```

Polling: 1s

---

### 5.3 DMXState

```ts
DMXState = {
  ports: DMXPortStatus[],
  lastUpdate: number,
  loading: boolean
}
```

Polling: 500ms – 1s

---

### 5.4 NetworkState

```ts
NetworkState = {
  status: NetworkStatus | null,
  scanning: boolean,
  error: string | null
}
```

Polling: 2s

---

## 6. API CLIENT (FRONTEND ↔ BACKEND)

### 6.1 Nguyen tac

- Moi endpoint = 1 function
- Typed response
- Khong retry vo han

---

### 6.2 Vi du

```ts
getSystemInfo(): Promise<SystemInfo>
getDMXStatus(): Promise<DMXStatus>
postDMXConfig(cfg: DMXConfig): Promise<void>
```

---

## 7. PAGE SPECIFICATION

### 7.1 Dashboard Page

API:
- GET /api/sys/info
- GET /api/dmx/status

Hien thi:
- Device ID
- Firmware version
- CPU / RAM
- DMX activity

---

### 7.2 DMX Mapping Page

API:
- GET /api/dmx/status
- POST /api/dmx/config

Chuc nang:
- Gan universe cho tung port
- Enable / disable port
- Trang thai pending / applied

---

### 7.3 Network Page

API:
- GET /api/network/status
- POST /api/network/config

Chuc nang:
- Cau hinh IP
- Hien thi ket noi

---

### 7.4 System Page

API:
- POST /api/sys/reboot
- POST /api/sys/factory

Chuc nang:
- Reboot
- Factory reset (can xac nhan)

---

## 8. UX & SAFETY RULES

- Range check input
- Disable nut khi dang gui request
- Xac nhan thao tac nguy hiem
- Khong spam API

---

## 9. RUNTIME INTERACTION RULES

- Frontend chi gui config
- Khong hien thi "apply ngay"
- Chap nhan do tre (eventual apply)

---

## 10. ERROR & DIAGNOSTICS

- Hien thi loi o muc UI
- Khong show stacktrace
- Error code mapping

---

## 11. BUILD & EMBED

```
vite build
  ↓
dist/
  ↓
gzip
  ↓
xxd -i
  ↓
MOD_WEB
```

---

**Trang thai tai lieu**: FINAL – v1.0
**Phu thuoc**: Web.md v1.0
**Vai tro**: SPEC TRIEN KHAI FRONTEND

