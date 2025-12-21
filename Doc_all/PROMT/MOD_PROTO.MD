# PROMPT SINH CODE MOD_PROTO (PROTOCOL STACK) - ESP-IDF

## 1. Vai trò của AI

Bạn là **Network Protocol Engineer** chuyên về các giao thức Realtime Ethernet (Art-Net, sACN) trên hệ thống nhúng.
Nhiệm vụ: Xây dựng **MOD_PROTO** - Module xử lý gói tin UDP, phân tích cú pháp (Parsing) và trộn tín hiệu (Merging) trước khi gửi xuống tầng Driver.

## 2. Ngữ cảnh hệ thống (Context)

- **Input:** Gói tin UDP từ Art-Net (Port 6454) và sACN (Port 5568).
- **Output:** Dữ liệu DMX sạch (512 bytes) ghi vào buffer của `SYS_MOD`.
- **Merge Engine:** Hệ thống hỗ trợ trộn 2 nguồn (Source A & Source B) theo cơ chế HTP (Cao nhất thắng) hoặc LTP (Mới nhất thắng).
- **Dependencies:** `lwip/sockets`, `sys_mod` (để lấy config và ghi output).

## 3. Data Contracts (INPUT BẮT BUỘC)

Sử dụng các cấu trúc dữ liệu chuẩn cho giao thức (Lưu ý: Network Byte Order thường là Big-Endian):

```c
// components/mod_proto/include/proto_types.h

// --- ART-NET v4 ---
#define ARTNET_PORT 6454
#define ARTNET_HEADER "Art-Net" // 8 bytes incl null
#define OP_OUTPUT 0x5000        // Little Endian in Packet? Check Spec! (ArtNet is usually Little Endian for OpCode, but Big Endian for Port?)
                                // UPDATE: ArtNet OpCode is Little Endian (Low byte first).
                                // PROT_VER is High byte first. Handle cẩn thận!

typedef struct __attribute__((packed)) {
    char id[8];
    uint16_t op_code;           // OpOutput = 0x5000
    uint16_t prot_ver;          // >= 14
    uint8_t sequence;
    uint8_t physical;
    uint8_t sub_uni;
    uint8_t net;
    uint16_t length;            // Big Endian
    uint8_t data[512];
} artnet_packet_t;

// --- sACN (E1.31) ---
#define SACN_PORT 5568
#define SACN_ROOT_ID_LEN 12 // "ASC-E1.17\0\0\0"

typedef struct __attribute__((packed)) {
    // Root Layer
    uint16_t preamble_size;
    uint16_t postamble_size;
    uint8_t packet_id[SACN_ROOT_ID_LEN];
    uint16_t flags_length;
    uint32_t vector_root;
    uint8_t cid[16];

    // Framing Layer
    uint16_t fl_flags_length;
    uint32_t vector_frame;
    char source_name[64];
    uint8_t priority;
    uint16_t sync_addr;
    uint8_t sequence;
    uint8_t options;
    uint16_t universe;      // Big Endian

    // DMP Layer
    uint16_t dmp_flags_length;
    uint8_t vector_dmp;
    uint8_t type_byte;
    uint16_t first_prop_addr;
    uint16_t addr_inc;
    uint16_t prop_val_count;
    uint8_t data[513];      // Start Code + 512 Data
} sacn_packet_t;

// --- MERGE CONTEXT ---
typedef enum {
    MERGE_MODE_HTP, // Highest Takes Precedence
    MERGE_MODE_LTP  // Latest Takes Precedence
} merge_mode_t;

typedef struct {
    bool active;
    uint32_t last_pkt_ts;
    uint8_t priority;
    uint8_t data[512];
    uint32_t src_ip;
} proto_source_t;

typedef struct {
    uint16_t universe;
    merge_mode_t mode;
    proto_source_t source_a;
    proto_source_t source_b;
    uint8_t final_data[512];
} merge_context_t;

```

## 4. Ràng buộc kỹ thuật (CRITICAL RULES)

❗ **TUYỆT ĐỐI TUÂN THỦ:**

1. **Endianness (Thứ tự Byte):**

- Mạng gửi Big-Endian (Network Order). ESP32 là Little-Endian.
- Bắt buộc dùng `ntohs()`, `ntohl()` khi đọc field `universe`, `opcode`, `length`.
- _Lưu ý đặc biệt:_ Art-Net OpCode là Little-Endian theo chuẩn, nhưng các field khác có thể là Big-Endian. Hãy check kỹ logic parse.

2. **Socket & Multicast:**

- Socket phải Non-blocking (dùng `select` hoặc `recvfrom` với timeout cực ngắn).
- Với sACN: Phải join Multicast Group tương ứng với Universe (IP: `239.255.u.u`).

3. **Merge Logic (State Machine):**

- Cài đặt logic theo `merge_engine_state.md`.
- **Timeout:** Nếu một source không gửi gói tin trong 2500ms -> Mark as inactive -> Recalculate Output.
- **HTP:** `Out[i] = max(SrcA[i], SrcB[i])`.
- **LTP:** Source nào có gói tin mới nhất -> Lấy dữ liệu source đó.

4. **Performance:**

- Hạn chế `memcpy` thừa. Parse trực tiếp trên buffer nhận được từ `recvfrom`.
- Chỉ gọi `sys_write_dmx_buffer` khi dữ liệu thực sự thay đổi hoặc đủ frame rate (để tránh spam Mutex của SYS_MOD).

## 5. Phạm vi code cần sinh

Sinh code component `components/mod_proto` theo cấu trúc:

```text
components/mod_proto/
├── include/
│   ├── proto_types.h     (Structs)
│   └── mod_proto.h       (Public API)
├── proto_mgr.c           (Task chính, Socket Select Loop)
├── artnet.c              (Parser Art-Net)
├── sacn.c                (Parser sACN + IGMP Join)
├── merge.c               (Logic trộn HTP/LTP + Timeout)
└── CMakeLists.txt

```

## 6. Yêu cầu chi tiết từng file

### 6.1 `proto_mgr.c`

- Tạo Task `proto_task` chạy trên Core 0.
- Init 2 UDP Sockets (6454, 5568).
- Vòng lặp `while(1)`: Dùng `select()` để đợi dữ liệu từ cả 2 socket.
- Nếu có dữ liệu -> Gọi parser tương ứng -> Gọi `merge_process()` -> Gửi sang `SYS_MOD`.

### 6.2 `artnet.c` & `sacn.c`

- Chỉ làm nhiệm vụ Parse & Validate Header.
- Trả về struct chứa pointer tới dữ liệu DMX và độ dài.
- `sacn_join_universe(uint16_t uni)`: Tính toán IP multicast và setsockopt `IP_ADD_MEMBERSHIP`.

### 6.3 `merge.c`

- Quản lý mảng `merge_context_t contexts[4]` (cho 4 cổng output).
- Hàm `merge_input(port_idx, source_data, priority, ip)`:
- Update timestamp cho source.
- So sánh Priority (sACN).
- Thực hiện HTP/LTP.
- Trả về kết quả cuối cùng.

- Hàm `merge_check_timeout()`: Gọi định kỳ để clear các source bị mất kết nối.

## 7. Output mong muốn

1. Code C tối ưu, xử lý pointer cẩn thận.
2. Xử lý đúng byte order (đây là lỗi hay gặp nhất).
3. Comment giải thích logic Merge (ví dụ: "If Source A priority > Source B -> Output A").
