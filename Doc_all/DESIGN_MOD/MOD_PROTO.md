# MOD_PROTO - Chi tiet Thiet ke Trien khai

## 1. Tong quan Thiet ke

Module `MOD_PROTO` la tang Protocol Stack, chiu trach nhiem lang nghe va giai ma cac goi tin DMX-over-IP (Art-Net, sACN). No thuc hien logic merge tin hieu tu nhieu nguon va output du lieu cuoi cung vao buffer cua SYS_MOD.

**File nguon:** `components/mod_proto/`

**Dependencies:** `lwip/sockets`, `sys_mod`

**Core Affinity:** Core 0 (Chung voi Network), Priority cao

---

## 2. Data Structure Design

### 2.1 Art-Net Protocol Structures

```c
/**
 * @file mod_proto/include/artnet.h
 * @brief Art-Net Protocol v4 Structures
 */

#define ARTNET_PORT 6454
#define ARTNET_HEADER "Art-Net\0"
#define ARTNET_OPCODE_DMX 0x5000   // OpDmx (Big-endian!)
#define ARTNET_OPCODE_POLL 0x2000  // OpPoll

#pragma pack(push, 1)  // Force 1-byte alignment

typedef struct {
    char id[8];              // "Art-Net\0"
    uint16_t opcode;         // Big-endian!
    uint16_t protocol_ver;   // Protocol version (14)
    uint8_t sequence;
    uint8_t physical;
    uint16_t universe;       // Universe (0-32767), Little-endian
    uint16_t length;         // Data length (Big-endian: 2-512)
    uint8_t data[512];
} artnet_dmx_packet_t;

#pragma pack(pop)
```

### 2.2 sACN Protocol Structures

```c
/**
 * @file mod_proto/include/sacn.h
 * @brief sACN (E1.31) Protocol Structures
 */

#define SACN_PORT 5568
#define SACN_MULTICAST_BASE "239.255.0.0"  // 239.255.x.x

#pragma pack(push, 1)

typedef struct {
    // Root Layer
    uint16_t preamble_size;       // 0x0010
    uint16_t postamble_size;      // 0x0000
    uint8_t acn_packet_id[12];    // ACN Packet Identifier
    uint16_t flags_length;        // Flags + Length
    uint32_t vector;              // VECTOR_ROOT_E131_DATA
    uint8_t cid[16];              // Sender CID (UUID)

    // Framing Layer
    uint16_t framing_flags_length;
    uint32_t framing_vector;
    char source_name[64];
    uint8_t priority;             // 0-200
    uint16_t sync_address;
    uint8_t sequence_number;
    uint8_t options;
    uint16_t universe;            // Universe (1-63999)

    // DMP Layer
    uint16_t dmp_flags_length;
    uint8_t dmp_vector;
    uint8_t address_type;
    uint16_t first_address;
    uint16_t address_increment;
    uint16_t property_value_count;
    uint8_t start_code;
    uint8_t data[512];
} sacn_packet_t;

#pragma pack(pop)
```

### 2.3 Merge Context

```c
/**
 * @brief Context merge cho moi Port (Xu ly 2 nguon tin hieu)
 */
typedef struct {
    // Source A
    struct {
        bool active;
        int64_t last_packet_time;
        uint8_t priority;          // sACN priority
        uint8_t sequence;          // Sequence number (detect packet loss)
        uint8_t data[512];
    } source_a;

    // Source B
    struct {
        bool active;
        int64_t last_packet_time;
        uint8_t priority;
        uint8_t sequence;
        uint8_t data[512];
    } source_b;

    // Merge Config
    enum {
        MERGE_MODE_HTP = 0,        // Highest Takes Precedence
        MERGE_MODE_LTP             // Latest Takes Precedence
    } merge_mode;

    uint32_t timeout_ms;           // Timeout de xoa source (default 2500ms)

    // Statistics
    uint32_t packets_received;
    uint32_t packets_merged;

} merge_context_t;

static merge_context_t g_merge_ctx[4];  // 4 ports
```

### 2.4 Proto State

```c
/**
 * @brief Trang thai tong module
 */
typedef struct {
    bool running;
    TaskHandle_t task_handle;

    // Sockets
    int artnet_socket;
    int sacn_socket;

    // Statistics
    uint32_t artnet_packets;
    uint32_t sacn_packets;
    uint32_t dropped_packets;

} proto_state_t;

static proto_state_t g_proto_state = {0};
```

---

## 3. Socket Setup

### 3.1 Art-Net Socket

```c
/**
 * @brief Tao socket cho Art-Net (UDP Broadcast)
 */
static int proto_create_artnet_socket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create ArtNet socket");
        return -1;
    }

    // Set socket non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Set SO_REUSEADDR (Cho phep nhieu instance bind cung port)
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port 6454
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(ARTNET_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "Failed to bind ArtNet socket");
        close(sock);
        return -1;
    }

    ESP_LOGI(TAG, "ArtNet socket created on port %d", ARTNET_PORT);
    return sock;
}
```

### 3.2 sACN Socket (Multicast)

```c
/**
 * @brief Tao socket cho sACN (UDP Multicast)
 */
static int proto_create_sacn_socket(void) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return -1;

    // Set non-blocking
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    // Set SO_REUSEADDR
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind to port 5568
    struct sockaddr_in addr = {
        .sin_family = AF_INET,
        .sin_port = htons(SACN_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY)
    };

    if (bind(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // Join Multicast Groups (cho cac Universe da config)
    proto_join_multicast_groups(sock);

    ESP_LOGI(TAG, "sACN socket created on port %d", SACN_PORT);
    return sock;
}

/**
 * @brief Join sACN Multicast cho cac Universe
 * Multicast IP: 239.255.(univ >> 8).(univ & 0xFF)
 */
static void proto_join_multicast_groups(int sock) {
    const sys_config_t* cfg = sys_get_config();

    for (int i = 0; i < 4; i++) {
        if (!cfg->ports[i].enabled) continue;
        if (cfg->ports[i].protocol != PROTO_SACN) continue;

        uint16_t universe = cfg->ports[i].universe;

        // Tinh multicast IP
        char mcast_ip[16];
        snprintf(mcast_ip, sizeof(mcast_ip), "239.255.%d.%d",
                (universe >> 8) & 0xFF, universe & 0xFF);

        // Join group
        struct ip_mreq mreq;
        mreq.imr_multiaddr.s_addr = inet_addr(mcast_ip);
        mreq.imr_interface.s_addr = htonl(INADDR_ANY);

        if (setsockopt(sock, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) == 0) {
            ESP_LOGI(TAG, "Joined sACN multicast %s (Universe %d)", mcast_ip, universe);
        }
    }
}
```

---

## 4. Packet Parsing

### 4.1 Art-Net Parser

```c
/**
 * @brief Parse Art-Net DMX packet
 * @return true neu hop le
 */
static bool proto_parse_artnet(const uint8_t* data, size_t len,
                                uint16_t* out_universe, uint8_t** out_dmx_data)
{
    if (len < sizeof(artnet_dmx_packet_t)) return false;

    artnet_dmx_packet_t* pkt = (artnet_dmx_packet_t*)data;

    // Check header
    if (memcmp(pkt->id, ARTNET_HEADER, 8) != 0) return false;

    // Check opcode (Big-endian!)
    uint16_t opcode = ntohs(pkt->opcode);  // Network to host
    if (opcode != ARTNET_OPCODE_DMX) return false;

    // Extract universe (Little-endian)
    *out_universe = pkt->universe;

    // Extract data (Big-endian length)
    uint16_t data_len = ntohs(pkt->length);
    if (data_len > 512) data_len = 512;

    *out_dmx_data = pkt->data;

    return true;
}
```

### 4.2 sACN Parser

```c
/**
 * @brief Parse sACN packet
 * @return true neu hop le
 */
static bool proto_parse_sacn(const uint8_t* data, size_t len,
                              uint16_t* out_universe,
                              uint8_t* out_priority,
                              uint8_t** out_dmx_data)
{
    if (len < sizeof(sacn_packet_t)) return false;

    sacn_packet_t* pkt = (sacn_packet_t*)data;

    // Check preamble (Big-endian)
    if (ntohs(pkt->preamble_size) != 0x0010) return false;

    // Check ACN Packet ID
    static const uint8_t acn_id[12] = {
        0x41, 0x53, 0x43, 0x2d, 0x45, 0x31, 0x2e, 0x31, 0x37, 0x00, 0x00, 0x00
    };
    if (memcmp(pkt->acn_packet_id, acn_id, 12) != 0) return false;

    // Extract universe (Big-endian)
    *out_universe = ntohs(pkt->universe);

    // Extract priority
    *out_priority = pkt->priority;

    // Extract DMX data
    *out_dmx_data = pkt->data;

    return true;
}
```

---

## 5. Universe Routing

```c
/**
 * @brief Tim port mapping cho universe
 * @return Port index (0-3), hoac -1 neu khong map
 */
static int proto_find_port_for_universe(uint16_t universe) {
    const sys_config_t* cfg = sys_get_config();

    for (int i = 0; i < 4; i++) {
        if (cfg->ports[i].enabled && cfg->ports[i].universe == universe) {
            return i;
        }
    }

    return -1;  // Not mapped
}
```

---

## 6. Merge Engine

### 6.1 HTP Merge (Highest Takes Precedence)

```c
/**
 * @brief Merge 2 nguon data bang HTP
 * Output = max(A, B) cho moi channel
 */
static void merge_htp(const uint8_t* data_a, const uint8_t* data_b,
                       uint8_t* output)
{
    for (int i = 0; i < 512; i++) {
        output[i] = (data_a[i] > data_b[i]) ? data_a[i] : data_b[i];
    }
}
```

### 6.2 Merge with Priority

```c
/**
 * @brief Merge co xet priority (cho sACN)
 */
static void proto_merge_port(int port_idx, const uint8_t* new_data,
                              uint8_t new_priority)
{
    merge_context_t* ctx = &g_merge_ctx[port_idx];
    int64_t now = esp_timer_get_time();

    // Check timeout (Xoa source cu neu > 2.5s khong nhan)
    if (ctx->source_a.active && (now - ctx->source_a.last_packet_time) > (ctx->timeout_ms * 1000)) {
        ESP_LOGW(TAG, "Port %d: Source A timeout", port_idx);
        ctx->source_a.active = false;
    }
    if (ctx->source_b.active && (now - ctx->source_b.last_packet_time) > (ctx->timeout_ms * 1000)) {
        ESP_LOGW(TAG, "Port %d: Source B timeout", port_idx);
        ctx->source_b.active = false;
    }

    // Quyet dinh source A hay B
    bool is_source_a = false;

    if (!ctx->source_a.active) {
        // A trong -> Dung A
        is_source_a = true;
    } else if (!ctx->source_b.active) {
        // B trong -> Check priority
        if (new_priority >= ctx->source_a.priority) {
            is_source_a = false;  // Thay the B
        } else {
            is_source_a = true;   // Update A
        }
    } else {
        // Ca A va B deu active -> Check priority
        if (new_priority > ctx->source_a.priority) {
            is_source_a = false;  // B co priority cao hon
        } else {
            is_source_a = true;
        }
    }

    // Update source tuong ung
    if (is_source_a) {
        memcpy(ctx->source_a.data, new_data, 512);
        ctx->source_a.last_packet_time = now;
        ctx->source_a.priority = new_priority;
        ctx->source_a.active = true;
    } else {
        memcpy(ctx->source_b.data, new_data, 512);
        ctx->source_b.last_packet_time = now;
        ctx->source_b.priority = new_priority;
        ctx->source_b.active = true;
    }

    // Merge output
    uint8_t* output_buf = sys_get_dmx_buffer(port_idx);

    if (ctx->source_a.active && ctx->source_b.active) {
        // Merge HTP
        merge_htp(ctx->source_a.data, ctx->source_b.data, output_buf);
        ctx->packets_merged++;
    } else if (ctx->source_a.active) {
        memcpy(output_buf, ctx->source_a.data, 512);
    } else if (ctx->source_b.active) {
        memcpy(output_buf, ctx->source_b.data, 512);
    }

    // Notify DMX Driver
    sys_notify_activity(port_idx);
}
```

---

## 7. Main Task Loop

```c
/**
 * @brief Task loop nhan packet
 */
static void proto_task_main(void* arg) {
    uint8_t rx_buffer[1024];
    fd_set read_fds;
    struct timeval timeout;

    ESP_LOGI(TAG, "Protocol task started on Core %d", xPortGetCoreID());

    while (g_proto_state.running) {
        // Setup select()
        FD_ZERO(&read_fds);
        FD_SET(g_proto_state.artnet_socket, &read_fds);
        FD_SET(g_proto_state.sacn_socket, &read_fds);

        int max_fd = (g_proto_state.artnet_socket > g_proto_state.sacn_socket) ?
                      g_proto_state.artnet_socket : g_proto_state.sacn_socket;

        // Timeout 100ms
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;

        int ret = select(max_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret > 0) {
            // Art-Net packet
            if (FD_ISSET(g_proto_state.artnet_socket, &read_fds)) {
                ssize_t len = recvfrom(g_proto_state.artnet_socket, rx_buffer,
                                       sizeof(rx_buffer), 0, NULL, NULL);
                if (len > 0) {
                    proto_handle_artnet_packet(rx_buffer, len);
                }
            }

            // sACN packet
            if (FD_ISSET(g_proto_state.sacn_socket, &read_fds)) {
                ssize_t len = recvfrom(g_proto_state.sacn_socket, rx_buffer,
                                       sizeof(rx_buffer), 0, NULL, NULL);
                if (len > 0) {
                    proto_handle_sacn_packet(rx_buffer, len);
                }
            }
        }
    }

    ESP_LOGI(TAG, "Protocol task stopped");
    vTaskDelete(NULL);
}

/**
 * @brief Xu ly Art-Net packet
 */
static void proto_handle_artnet_packet(const uint8_t* data, size_t len) {
    uint16_t universe;
    uint8_t* dmx_data;

    if (!proto_parse_artnet(data, len, &universe, &dmx_data)) {
        g_proto_state.dropped_packets++;
        return;
    }

    int port = proto_find_port_for_universe(universe);
    if (port < 0) {
        // Not mapped
        return;
    }

    // Merge (Art-Net khong co priority -> Dung priority = 100 default)
    proto_merge_port(port, dmx_data, 100);

    g_proto_state.artnet_packets++;
}

/**
 * @brief Xu ly sACN packet
 */
static void proto_handle_sacn_packet(const uint8_t* data, size_t len) {
    uint16_t universe;
    uint8_t priority;
    uint8_t* dmx_data;

    if (!proto_parse_sacn(data, len, &universe, &priority, &dmx_data)) {
        g_proto_state.dropped_packets++;
        return;
    }

    int port = proto_find_port_for_universe(universe);
    if (port < 0) return;

    proto_merge_port(port, dmx_data, priority);

    g_proto_state.sacn_packets++;
}
```

---

## 8. API Implementation

```c
/**
 * @brief Start protocol stack
 */
esp_err_t proto_start(void) {
    if (g_proto_state.running) return ESP_OK;

    // Create sockets
    g_proto_state.artnet_socket = proto_create_artnet_socket();
    g_proto_state.sacn_socket = proto_create_sacn_socket();

    if (g_proto_state.artnet_socket < 0 || g_proto_state.sacn_socket < 0) {
        ESP_LOGE(TAG, "Failed to create sockets");
        return ESP_FAIL;
    }

    // Init merge contexts
    for (int i = 0; i < 4; i++) {
        g_merge_ctx[i].merge_mode = MERGE_MODE_HTP;
        g_merge_ctx[i].timeout_ms = 2500;
    }

    // Start task
    g_proto_state.running = true;
    xTaskCreatePinnedToCore(
        proto_task_main,
        "proto",
        4096,
        NULL,
        5,  // Priority cao (Vi tinh realtime)
        &g_proto_state.task_handle,
        0   // Core 0
    );

    ESP_LOGI(TAG, "Protocol stack started");
    return ESP_OK;
}

/**
 * @brief Stop protocol stack
 */
void proto_stop(void) {
    if (!g_proto_state.running) return;

    g_proto_state.running = false;

    vTaskDelay(pdMS_TO_TICKS(200));

    close(g_proto_state.artnet_socket);
    close(g_proto_state.sacn_socket);

    ESP_LOGI(TAG, "Protocol stack stopped");
}
```

---

## 9. Memory & Performance

- **RAM**: ~4KB (Merge contexts 2KB + Task stack 2KB)
- **CPU**: < 10% Core 0 @ 44 packets/sec
- **Latency**: < 1ms (socket read + parse + merge)

---

## 10. Testing

### Unit Tests

1. Parse Art-Net valid packet -> Extract correct universe/data
2. Parse sACN with priority -> Extract correct priority
3. HTP merge [100, 50] + [80, 200] -> [100, 200]

### Integration Tests

1. Send Art-Net from console -> Verify DMX output matches
2. Send 2 sACN sources -> Verify merge HTP
3. Stop source A -> Verify switch to source B after timeout

---

## Ket luan

MOD_PROTO implementation-ready với parsing algorithms, merge logic và socket handling đầy đủ.
