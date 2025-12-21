# MOD_WEB - Chi tiet Thiet ke Trien khai

## 1. Tong quan

`MOD_WEB` cung cap giao dien Web (SPA) va REST API de cau hinh he thong. Chay priority thap tren Core 0, khong anh huong DMX realtime.

**File nguon:** `components/mod_web/`

**Dependencies:** `esp_http_server`, `cJSON`

---

## 2. API Endpoints

```c
// System APIs
GET  /api/sys/info         // Get device info
POST /api/sys/reboot       // Reboot device
POST /api/sys/factory      // Factory reset

// DMX APIs
GET  /api/dmx/status       // Get DMX port status
POST /api/dmx/config       // Update DMX config
POST /api/dmx/snapshot     // Record snapshot

// Network APIs
GET  /api/net/status       // Get network status
POST /api/net/config       // Update network config

// Files
GET  /                     // Serve index.html (gzipped)
GET  /app.js               // Serve JS bundle (gzipped)
GET  /style.css            // Serve CSS (gzipped)
```

---

## 3. Static File Serving (Gzip)

```c
/**
 * @brief Embedded gzipped files
 * Build: gzip -9 dist/index.html > index.html.gz
 * Then: xxd -i index.html.gz > index_html_gz.c
 */
extern const uint8_t index_html_gz_start[] asm("_binary_index_html_gz_start");
extern const uint8_t index_html_gz_end[]   asm("_binary_index_html_gz_end");

static esp_err_t web_handler_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");

    const size_t len = index_html_gz_end - index_html_gz_start;
    httpd_resp_send(req, (const char*)index_html_gz_start, len);
    return ESP_OK;
}
```

---

## 4. REST API Handlers

### 4.1 System Info

```c
static esp_err_t api_sys_info(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();

    const sys_config_t* cfg = sys_get_config();
    cJSON_AddStringToObject(root, "device", cfg->device_label);
    cJSON_AddStringToObject(root, "version", "4.0.0");
    cJSON_AddNumberToObject(root, "uptime", esp_timer_get_time() / 1000000);
    cJSON_AddNumberToObject(root, "free_heap", esp_get_free_heap_size());

    char* json_str = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json_str);

    free(json_str);
    cJSON_Delete(root);
    return ESP_OK;
}
```

### 4.2 DMX Config Update

```c
static esp_err_t api_dmx_config(httpd_req_t *req) {
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) return ESP_FAIL;

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    int port = cJSON_GetObjectItem(root, "port")->valueint;
    int break_us = cJSON_GetObjectItem(root, "break_us")->valueint;
    int mab_us = cJSON_GetObjectItem(root, "mab_us")->valueint;

    // Validate
    if (port < 0 || port >= 4 || break_us < 88 || break_us > 500) {
        cJSON_Delete(root);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid params");
        return ESP_FAIL;
    }

    // Update via SYS_MOD
    dmx_port_cfg_t new_cfg = {0};
    new_cfg.timing.break_us = break_us;
    new_cfg.timing.mab_us = mab_us;

    esp_err_t err = sys_update_port_cfg(port, &new_cfg);

    cJSON_Delete(root);

    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Update failed");
    }

    return ESP_OK;
}
```

---

## 5. Server Initialization

```c
esp_err_t web_init(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 4096;
    config.max_open_sockets = 4;
    config.lru_purge_enable = true;

    httpd_handle_t server = NULL;
    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server");
        return ESP_FAIL;
    }

    // Register handlers
    httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = web_handler_index
    };
    httpd_register_uri_handler(server, &uri_index);

    httpd_uri_t uri_sys_info = {
        .uri = "/api/sys/info",
        .method = HTTP_GET,
        .handler = api_sys_info
    };
    httpd_register_uri_handler(server, &uri_sys_info);

    httpd_uri_t uri_dmx_config = {
        .uri = "/api/dmx/config",
        .method = HTTP_POST,
        .handler = api_dmx_config
    };
    httpd_register_uri_handler(server, &uri_dmx_config);

    ESP_LOGI(TAG, "Web server started on port %d", config.server_port);
    return ESP_OK;
}
```

---

## 6. Memory & Performance

- **RAM**: ~4KB (HTTP server + JSON buffers)
- **CPU**: < 2% (sporadic, only when user access)
- **Response time**: < 200ms

---

## 7. Testing

1. **Static files**: `curl http://192.168.1.100/` -> HTML served
2. **API GET**: `curl http://192.168.1.100/api/sys/info` -> JSON response
3. **API POST**: `curl -X POST -d '{"port":0,"break_us":200}' http://192.168.1.100/api/dmx/config`

---

**Ket luan Backend:** MOD_WEB backend ready với HTTP server, REST API, JSON handling, và gzip static files.

---

# FRONTEND SPECIFICATION

## 8. Frontend Architecture

### 8.1 Technology Stack

**Framework đề xuất:** React 18 hoặc Vue 3

**Rationale:**

- React: Ecosystem lớn, TypeScript support tốt, nhiều UI libraries
- Vue 3: Đơn giản hơn, Composition API mạnh, bundle nhẹ hơn

**Dependencies:**

```json
{
  "dependencies": {
    "react": "^18.2.0", // hoặc "vue": "^3.3.0"
    "axios": "^1.6.0", // HTTP client
    "zustand": "^4.4.0", // State management (React)
    // hoặc "pinia": "^2.1.0"     // State management (Vue)
    "tailwindcss": "^3.3.0", // Utility-first CSS
    "lucide-react": "^0.300.0" // Icons
  },
  "devDependencies": {
    "vite": "^5.0.0", // Build tool
    "typescript": "^5.3.0",
    "@types/react": "^18.2.0"
  }
}
```

### 8.2 Project Structure

```
frontend/
├── src/
│   ├── api/
│   │   ├── client.ts              # Axios instance config
│   │   ├── endpoints.ts           # API endpoint definitions
│   │   └── types.ts               # API response types
│   │
│   ├── stores/
│   │   ├── systemStore.ts         # System state (uptime, heap, cpu)
│   │   ├── dmxStore.ts            # DMX ports state
│   │   ├── networkStore.ts        # Network status
│   │   └── usePolling.ts          # Custom hook for polling
│   │
│   ├── components/
│   │   ├── layout/
│   │   │   ├── Header.tsx         # Top bar (device name, status)
│   │   │   ├── Sidebar.tsx        # Navigation tabs
│   │   │   └── Layout.tsx         # Main layout wrapper
│   │   │
│   │   ├── dashboard/
│   │   │   ├── TelemetryCard.tsx  # CPU/RAM/Uptime cards
│   │   │   ├── PortStatusGrid.tsx # 4 DMX ports status
│   │   │   └── NetworkStatus.tsx  # IP/Link indicator
│   │   │
│   │   ├── dmx/
│   │   │   ├── PortConfigForm.tsx # Per-port config
│   │   │   ├── TimingSliders.tsx  # Break/MAB/Rate
│   │   │   └── SnapshotButton.tsx # Record snapshot
│   │   │
│   │   ├── network/
│   │   │   ├── EthernetConfig.tsx
│   │   │   ├── WifiConfig.tsx
│   │   │   └── WifiScanner.tsx
│   │   │
│   │   ├── system/
│   │   │   ├── ConfigManager.tsx  # Import/Export
│   │   │   ├── OTAUploader.tsx    # Firmware upload
│   │   │   └── DangerZone.tsx     # Reboot/Reset
│   │   │
│   │   └── shared/
│   │       ├── Card.tsx
│   │       ├── Button.tsx
│   │       ├── Input.tsx
│   │       ├── Select.tsx
│   │       ├── Slider.tsx
│   │       └── StatusBadge.tsx
│   │
│   ├── pages/
│   │   ├── Dashboard.tsx
│   │   ├── DMXConfig.tsx
│   │   ├── NetworkConfig.tsx
│   │   └── SystemSettings.tsx
│   │
│   ├── utils/
│   │   ├── validators.ts          # Input validation
│   │   └── formatters.ts          # Data formatting
│   │
│   ├── App.tsx
│   └── main.tsx
│
├── public/
│   └── favicon.ico
│
└── dist/                          # Build output
    ├── index.html
    └── assets/
        ├── index-[hash].js
        └── index-[hash].css
```

---

## 9. State Management Design

### 9.1 System Store (Telemetry)

```typescript
// stores/systemStore.ts (Zustand example)
interface SystemState {
  // Data
  connected: boolean;
  uptime: number; // seconds
  freeHeap: number; // bytes
  cpuLoad: number; // percentage
  temperature: number; // celsius
  deviceLabel: string;
  firmwareVersion: string;

  // Actions
  fetchInfo: () => Promise<void>;
  setConnected: (status: boolean) => void;
}

export const useSystemStore = create<SystemState>((set, get) => ({
  connected: false,
  uptime: 0,
  freeHeap: 0,
  cpuLoad: 0,
  temperature: 0,
  deviceLabel: "DMX Node",
  firmwareVersion: "4.0.0",

  fetchInfo: async () => {
    try {
      const data = await api.getSysInfo();
      set({
        connected: true,
        uptime: data.uptime,
        freeHeap: data.free_heap,
        cpuLoad: data.cpu_load,
        temperature: data.temperature,
        deviceLabel: data.device_label,
        firmwareVersion: data.version,
      });
    } catch (error) {
      set({ connected: false });
      console.error("Failed to fetch system info:", error);
    }
  },

  setConnected: (status) => set({ connected: status }),
}));
```

### 9.2 DMX Store

```typescript
// stores/dmxStore.ts
interface PortStatus {
  id: number;
  state: "IDLE" | "RUNNING" | "HOLD" | "ERROR";
  fps: number;
  sourceIP: string;
  protocol: "ARTNET" | "SACN" | "OFF";
}

interface DMXState {
  ports: PortStatus[];

  fetchStatus: () => Promise<void>;
  updatePortConfig: (port: number, config: any) => Promise<void>;
  recordSnapshot: (port: number) => Promise<void>;
}

export const useDMXStore = create<DMXState>((set) => ({
  ports: [
    { id: 0, state: "IDLE", fps: 0, sourceIP: "-", protocol: "ARTNET" },
    { id: 1, state: "IDLE", fps: 0, sourceIP: "-", protocol: "ARTNET" },
    { id: 2, state: "IDLE", fps: 0, sourceIP: "-", protocol: "OFF" },
    { id: 3, state: "IDLE", fps: 0, sourceIP: "-", protocol: "OFF" },
  ],

  fetchStatus: async () => {
    const data = await api.getDMXStatus();
    set({ ports: data.ports });
  },

  updatePortConfig: async (port, config) => {
    await api.updateDMXConfig(port, config);
    // Refetch status
    useDMXStore.getState().fetchStatus();
  },

  recordSnapshot: async (port) => {
    await api.recordSnapshot(port);
  },
}));
```

### 9.3 Network Store

```typescript
// stores/networkStore.ts
interface NetworkState {
  currentIP: string;
  netmask: string;
  gateway: string;
  mode: "ETHERNET" | "WIFI_STA" | "WIFI_AP";
  wifiSSID: string;
  wifiRSSI: number;
  mdnsHostname: string;

  fetchStatus: () => Promise<void>;
  updateConfig: (config: any) => Promise<void>;
  scanWifi: () => Promise<string[]>;
}
```

---

## 10. API Layer Implementation

### 10.1 API Client

```typescript
// api/client.ts
import axios, { AxiosInstance } from "axios";

const BASE_URL = import.meta.env.PROD
  ? "" // Same origin in production
  : "http://192.168.1.100"; // Dev proxy

export const apiClient: AxiosInstance = axios.create({
  baseURL: BASE_URL,
  timeout: 5000,
  headers: {
    "Content-Type": "application/json",
  },
});

// Request interceptor (for auth if needed)
apiClient.interceptors.request.use((config) => {
  // Add auth token if exists
  const token = localStorage.getItem("auth_token");
  if (token) {
    config.headers.Authorization = `Bearer ${token}`;
  }
  return config;
});

// Response interceptor (error handling)
apiClient.interceptors.response.use(
  (response) => response.data,
  (error) => {
    if (error.code === "ECONNABORTED") {
      console.error("Request timeout");
    } else if (error.response?.status === 401) {
      // Unauthorized
      localStorage.removeItem("auth_token");
    }
    return Promise.reject(error);
  }
);
```

### 10.2 API Endpoints

```typescript
// api/endpoints.ts
import { apiClient } from "./client";

export const api = {
  // System APIs
  async getSysInfo() {
    return apiClient.get("/api/sys/info");
  },

  async reboot() {
    return apiClient.post("/api/sys/reboot");
  },

  async factoryReset() {
    return apiClient.post("/api/sys/factory");
  },

  // DMX APIs
  async getDMXStatus() {
    return apiClient.get("/api/dmx/status");
  },

  async updateDMXConfig(port: number, config: any) {
    return apiClient.post("/api/dmx/config", { port, ...config });
  },

  async updateAdvancedTiming(
    port: number,
    timing: {
      break_us: number;
      mab_us: number;
      rate: number;
    }
  ) {
    return apiClient.post("/api/dmx/advanced", { port, ...timing });
  },

  async recordSnapshot(port: number) {
    return apiClient.post("/api/dmx/snapshot", { port, action: "record" });
  },

  // Network APIs
  async getNetStatus() {
    return apiClient.get("/api/net/status");
  },

  async scanWifi() {
    return apiClient.get("/api/net/scan");
  },

  async updateNetConfig(config: any) {
    return apiClient.post("/api/net/config", config);
  },

  // File APIs
  async exportConfig() {
    return apiClient.get("/api/file/export", {
      responseType: "blob",
    });
  },

  async importConfig(file: File) {
    const formData = new FormData();
    formData.append("config", file);
    return apiClient.post("/api/file/import", formData, {
      headers: { "Content-Type": "multipart/form-data" },
    });
  },

  // OTA API
  async uploadFirmware(file: File, onProgress: (percent: number) => void) {
    const formData = new FormData();
    formData.append("firmware", file);
    return apiClient.post("/api/sys/ota", formData, {
      headers: { "Content-Type": "multipart/form-data" },
      onUploadProgress: (progressEvent) => {
        const percent = Math.round(
          (progressEvent.loaded * 100) / progressEvent.total!
        );
        onProgress(percent);
      },
    });
  },
};
```

---

## 11. Component Design

### 11.1 Dashboard - Telemetry Card

```typescript
// components/dashboard/TelemetryCard.tsx
import { Card } from "../shared/Card";

interface TelemetryCardProps {
  title: string;
  value: string | number;
  unit?: string;
  icon: React.ReactNode;
  status?: "good" | "warning" | "error";
}

export const TelemetryCard: React.FC<TelemetryCardProps> = ({
  title,
  value,
  unit,
  icon,
  status = "good",
}) => {
  const statusColors = {
    good: "text-green-500",
    warning: "text-yellow-500",
    error: "text-red-500",
  };

  return (
    <Card className="p-4">
      <div className="flex items-center justify-between">
        <div>
          <p className="text-sm text-gray-500">{title}</p>
          <p className={`text-2xl font-bold ${statusColors[status]}`}>
            {value} <span className="text-sm">{unit}</span>
          </p>
        </div>
        <div className="text-3xl text-gray-300">{icon}</div>
      </div>
    </Card>
  );
};

// Usage:
<TelemetryCard
  title="CPU Load"
  value={cpuLoad}
  unit="%"
  icon={<CpuIcon />}
  status={cpuLoad > 80 ? "warning" : "good"}
/>;
```

### 11.2 DMX - Port Status Grid

```typescript
// components/dmx/PortStatusGrid.tsx
import { useDMXStore } from "../../stores/dmxStore";

export const PortStatusGrid: React.FC = () => {
  const ports = useDMXStore((state) => state.ports);

  const stateColors = {
    IDLE: "bg-gray-200 text-gray-700",
    RUNNING: "bg-green-500 text-white",
    HOLD: "bg-yellow-500 text-white",
    ERROR: "bg-red-500 text-white",
  };

  return (
    <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4">
      {ports.map((port) => (
        <Card key={port.id} className="p-4">
          <div className="flex justify-between items-start mb-2">
            <h3 className="font-bold">
              Port {String.fromCharCode(65 + port.id)}
            </h3>
            <span
              className={`px-2 py-1 rounded text-xs ${stateColors[port.state]}`}
            >
              {port.state}
            </span>
          </div>

          <div className="space-y-1 text-sm">
            <p className="text-gray-600">
              FPS: <span className="font-mono">{port.fps}</span>
            </p>
            <p className="text-gray-600">
              Protocol: <span className="font-medium">{port.protocol}</span>
            </p>
            <p className="text-gray-600">
              Source: <span className="font-mono text-xs">{port.sourceIP}</span>
            </p>
          </div>
        </Card>
      ))}
    </div>
  );
};
```

### 11.3 DMX - Timing Sliders

```typescript
// components/dmx/TimingSliders.tsx
import { useState } from "react";
import { Slider } from "../shared/Slider";
import { Button } from "../shared/Button";
import { useDMXStore } from "../../stores/dmxStore";

interface TimingSlidersProps {
  port: number;
  initialValues: {
    break_us: number;
    mab_us: number;
    rate: number;
  };
}

export const TimingSliders: React.FC<TimingSlidersProps> = ({
  port,
  initialValues,
}) => {
  const [values, setValues] = useState(initialValues);
  const [saving, setSaving] = useState(false);
  const updateConfig = useDMXStore((state) => state.updatePortConfig);

  const handleApply = async () => {
    setSaving(true);
    try {
      await updateConfig(port, { timing: values });
      // Success notification
    } catch (error) {
      // Error notification
    } finally {
      setSaving(false);
    }
  };

  return (
    <div className="space-y-4">
      <div>
        <label className="block text-sm font-medium mb-2">
          Break Time: {values.break_us}µs
        </label>
        <Slider
          min={88}
          max={500}
          value={values.break_us}
          onChange={(v) => setValues({ ...values, break_us: v })}
        />
        <p className="text-xs text-gray-500 mt-1">DMX512 Standard: 88-500µs</p>
      </div>

      <div>
        <label className="block text-sm font-medium mb-2">
          MAB Time: {values.mab_us}µs
        </label>
        <Slider
          min={8}
          max={100}
          value={values.mab_us}
          onChange={(v) => setValues({ ...values, mab_us: v })}
        />
      </div>

      <div>
        <label className="block text-sm font-medium mb-2">
          Refresh Rate: {values.rate}Hz
        </label>
        <Slider
          min={20}
          max={44}
          value={values.rate}
          onChange={(v) => setValues({ ...values, rate: v })}
        />
      </div>

      <Button onClick={handleApply} loading={saving} className="w-full">
        Apply Changes
      </Button>
    </div>
  );
};
```

---

## 12. Real-time Updates

### 12.1 Polling Hook

```typescript
// stores/usePolling.ts
import { useEffect, useRef } from "react";

export const usePolling = (
  callback: () => void | Promise<void>,
  interval: number,
  enabled: boolean = true
) => {
  const savedCallback = useRef(callback);

  useEffect(() => {
    savedCallback.current = callback;
  }, [callback]);

  useEffect(() => {
    if (!enabled) return;

    // Run immediately
    savedCallback.current();

    // Then poll
    const id = setInterval(() => {
      savedCallback.current();
    }, interval);

    return () => clearInterval(id);
  }, [interval, enabled]);
};

// Usage:
const Dashboard = () => {
  const fetchInfo = useSystemStore((state) => state.fetchInfo);
  const connected = useSystemStore((state) => state.connected);

  usePolling(fetchInfo, 1000, true); // Poll every 1s

  return <div>...</div>;
};
```

### 12.2 Connection Status

```typescript
// components/layout/Header.tsx
import { useSystemStore } from "../../stores/systemStore";

export const Header: React.FC = () => {
  const { connected, deviceLabel } = useSystemStore();

  return (
    <header className="bg-white border-b px-6 py-4">
      <div className="flex justify-between items-center">
        <h1 className="text-xl font-bold">{deviceLabel}</h1>

        <div className="flex items-center gap-2">
          <div
            className={`w-2 h-2 rounded-full ${
              connected ? "bg-green-500" : "bg-red-500"
            }`}
          />
          <span className="text-sm text-gray-600">
            {connected ? "Connected" : "Offline"}
          </span>
        </div>
      </div>
    </header>
  );
};
```

---

## 13. Build & Deployment

### 13.1 Build Configuration

```javascript
// vite.config.ts
import { defineConfig } from "vite";
import react from "@vitejs/plugin-react";

export default defineConfig({
  plugins: [react()],
  build: {
    outDir: "dist",
    assetsDir: "assets",
    minify: "terser",
    terserOptions: {
      compress: {
        drop_console: true,
      },
    },
  },
  server: {
    proxy: {
      "/api": {
        target: "http://192.168.1.100",
        changeOrigin: true,
      },
    },
  },
});
```

### 13.2 Gzip & Embed Script

```bash
#!/bin/bash
# scripts/build_and_embed.sh

echo "Building frontend..."
cd frontend && npm run build

echo "Compressing assets..."
cd dist
gzip -9k index.html
gzip -9k assets/*.js
gzip -9k assets/*.css

echo "Converting to C arrays..."
cd ..
mkdir -p ../components/mod_web/web_assets

xxd -i dist/index.html.gz > ../components/mod_web/web_assets/index_html.c
xxd -i dist/assets/index-*.js.gz > ../components/mod_web/web_assets/app_js.c
xxd -i dist/assets/index-*.css.gz > ../components/mod_web/web_assets/app_css.c

echo "Done! Assets embedded in firmware."
```

---

## 14. Testing Strategy

### 14.1 Unit Tests

```typescript
// __tests__/stores/systemStore.test.ts
import { renderHook, act } from "@testing-library/react";
import { useSystemStore } from "../../stores/systemStore";

describe("SystemStore", () => {
  it("should fetch system info", async () => {
    const { result } = renderHook(() => useSystemStore());

    await act(async () => {
      await result.current.fetchInfo();
    });

    expect(result.current.connected).toBe(true);
    expect(result.current.uptime).toBeGreaterThan(0);
  });

  it("should handle connection failure", async () => {
    // Mock API failure
    const { result } = renderHook(() => useSystemStore());

    await act(async () => {
      await result.current.fetchInfo();
    });

    expect(result.current.connected).toBe(false);
  });
});
```

### 14.2 Integration Tests

```typescript
// __tests__/integration/dmxConfig.test.tsx
import { render, screen, fireEvent, waitFor } from "@testing-library/react";
import DMXConfig from "../../pages/DMXConfig";

describe("DMX Configuration", () => {
  it("should update port config", async () => {
    render(<DMXConfig />);

    const universeInput = screen.getByLabelText("Universe");
    fireEvent.change(universeInput, { target: { value: "100" } });

    const applyButton = screen.getByText("Apply");
    fireEvent.click(applyButton);

    await waitFor(() => {
      expect(screen.getByText("Success")).toBeInTheDocument();
    });
  });
});
```

---

## 15. Performance Optimization

### 15.1 Code Splitting

```typescript
// App.tsx
import { lazy, Suspense } from "react";

const Dashboard = lazy(() => import("./pages/Dashboard"));
const DMXConfig = lazy(() => import("./pages/DMXConfig"));
const NetworkConfig = lazy(() => import("./pages/NetworkConfig"));
const SystemSettings = lazy(() => import("./pages/SystemSettings"));

export const App = () => {
  return (
    <Layout>
      <Suspense fallback={<LoadingSpinner />}>
        <Routes>
          <Route path="/" element={<Dashboard />} />
          <Route path="/dmx" element={<DMXConfig />} />
          <Route path="/network" element={<NetworkConfig />} />
          <Route path="/system" element={<SystemSettings />} />
        </Routes>
      </Suspense>
    </Layout>
  );
};
```

### 15.2 Debouncing

```typescript
// utils/debounce.ts
export const useDebouncedValue = <T>(value: T, delay: number): T => {
  const [debouncedValue, setDebouncedValue] = useState(value);

  useEffect(() => {
    const handler = setTimeout(() => {
      setDebouncedValue(value);
    }, delay);

    return () => clearTimeout(handler);
  }, [value, delay]);

  return debouncedValue;
};

// Usage in form:
const FormComponent = () => {
  const [universe, setUniverse] = useState(0);
  const debouncedUniverse = useDebouncedValue(universe, 500);

  useEffect(() => {
    // Only call API after 500ms of no changes
    api.updateDMXConfig(port, { universe: debouncedUniverse });
  }, [debouncedUniverse]);
};
```

---

**KET LUAN FRONTEND:** Web frontend ready với React/Vue SPA, state management, real-time polling, responsive UI, và complete API integration. Total bundle size expected: ~200KB (gzipped).
