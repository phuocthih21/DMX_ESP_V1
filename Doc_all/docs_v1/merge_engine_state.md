# STATE MACHINE – MERGE ENGINE (TEXT-BASED)

---

## 1. Mục đích

State Machine này mô tả **hành vi runtime của Merge Engine** trong hệ Art-Net / sACN Node, dùng để:
- Triển khai code rõ ràng, tránh logic rẽ nhánh rối
- Đảm bảo xử lý đúng priority, HTP/LTP, timeout
- Làm nền cho debug, test case và mở rộng

State machine hoạt động **theo từng Universe độc lập**.

---

## 2. Các thực thể tham gia

- Source[i]: Art-Net hoặc sACN sender
- Merge Engine: logic quyết định output DMX
- Timer: theo dõi timeout source

---

## 3. Danh sách trạng thái (States)

```
[S0] IDLE
[S1] SINGLE_SOURCE_ACTIVE
[S2] MULTI_SOURCE_ACTIVE
[S3] PRIORITY_FILTERING
[S4] MERGE_HTP
[S5] MERGE_LTP
[S6] OUTPUT_ACTIVE
[S7] FAILOVER
```

---

## 4. Mô tả chi tiết từng State

---

### [S0] IDLE

**Mô tả**:
- Chưa có source nào active
- Output DMX giữ giá trị mặc định (0 hoặc last safe frame)

**Sự kiện vào**:
- Khởi động hệ thống
- Tất cả source timeout

**Chuyển trạng thái**:
```
On Source_Join → S1
```

---

### [S1] SINGLE_SOURCE_ACTIVE

**Mô tả**:
- Chỉ có đúng 1 source active
- Không cần merge

**Hành vi**:
- Output = DMX của source đang active

**Chuyển trạng thái**:
```
On Source_Join (source thứ 2) → S2
On Source_Timeout → S0
```

---

### [S2] MULTI_SOURCE_ACTIVE

**Mô tả**:
- Có từ 2 source active trở lên
- Chưa quyết định merge hay loại source

**Hành vi**:
- Không output trực tiếp
- Chuẩn bị lọc priority

**Chuyển trạng thái**:
```
Always → S3
```

---

### [S3] PRIORITY_FILTERING

**Mô tả**:
- So sánh priority giữa các source
- Loại source priority thấp

**Hành vi**:
- Tính max_priority
- Disable source có priority < max

**Chuyển trạng thái**:
```
If only 1 source remains → S1
If >1 source remain → (merge_mode == HTP ? S4 : S5)
If no source remain → S7
```

---

### [S4] MERGE_HTP

**Mô tả**:
- Merge theo Highest Takes Precedence
- Thực hiện theo từng channel

**Hành vi**:
```
for ch = 0..511:
    out[ch] = max(srcA[ch], srcB[ch], ...)
```

**Chuyển trạng thái**:
```
After merge → S6
```

---

### [S5] MERGE_LTP

**Mô tả**:
- Merge theo Latest Takes Precedence
- Chọn source update gần nhất

**Hành vi**:
```
active = source có last_update_ms mới nhất
out = active.dmx
```

**Chuyển trạng thái**:
```
After select → S6
```

---

### [S6] OUTPUT_ACTIVE

**Mô tả**:
- Đang xuất DMX ra output
- Trạng thái ổn định

**Hành vi**:
- Gửi DMX frame
- Giữ output cho đến khi có sự kiện mới

**Chuyển trạng thái**:
```
On New_Frame (same sources) → stay S6
On Source_Join → S2
On Source_Timeout → S7
```

---

### [S7] FAILOVER

**Mô tả**:
- Source active bị timeout hoặc mất kết nối
- Cần tái đánh giá hệ thống

**Hành vi**:
- Disable source timeout
- Re-evaluate active sources

**Chuyển trạng thái**:
```
If no source left → S0
If 1 source left → S1
If >1 source left → S3
```

---

## 5. Sơ đồ luồng tổng hợp (ASCII)

```
          +------+
          | IDLE |
          +--+---+
             |
             v
     +----------------+
     | SINGLE_SOURCE  |
     +--+---------+--+
        |         |
        |         v
        |   +----------------+
        |   | MULTI_SOURCE   |
        |   +-------+--------+
        |           |
        |           v
        |   +----------------+
        |   | PRIORITY_CHECK |
        |   +---+--------+---+
        |       |        |
        |       |        v
        |       |   +-----------+
        |       |   | MERGE_LTP |
        |       |   +-----+-----+
        |       |         |
        |       v         v
        |   +-----------+ +-----------+
        |   | MERGE_HTP | | OUTPUT    |
        |   +-----+-----+ +-----+-----+
        |         |             |
        +---------+-------------+
                  |
                  v
              +--------+
              | FAILOVER|
              +----+---+
                   |
                   v
                (re-eval)
```

---

## 6. Mapping sang code (gợi ý)

- Enum state:
```c
typedef enum {
    MERGE_IDLE,
    MERGE_SINGLE,
    MERGE_MULTI,
    MERGE_PRIORITY,
    MERGE_HTP,
    MERGE_LTP,
    MERGE_OUTPUT,
    MERGE_FAILOVER
} merge_state_t;
```

- State handler dạng function pointer hoặc switch-case

---

## 7. Lợi ích khi dùng State Machine này

- Tránh logic if-else lồng nhau
- Dễ debug realtime
- Dễ thêm:
  - per-channel LTP
  - universe lock
  - backup console

---

**Kết thúc tài liệu**

