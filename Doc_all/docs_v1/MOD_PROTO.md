# MOD_PROTO (Protocol Stack)

## 1. Thong tin Module

- **Module Name**: Protocol Stack
- **Module ID**: `MOD_PROTO`
- **Version**: 1.0.0
- **Status**: Stable
- **File tai lieu lien quan**: `MOD_PROTO.md` (Goc)

## 2. Pham vi va Trach nhiem

**Module nay lam gi:**

- Lang nghe, giai ma goi tin UDP Art-Net va sACN.
- Xu ly logic gop tin hieu (Merging Logic).
- Phan hoi goi tin Discovery (ArtPoll).

**Module nay KHONG lam gi:**

- Khong quan ly IP/Mac (viec cua NET).
- Khong xuat tin hieu dien (viec cua DMX).

## 3. Yeu cau chuc nang (Functional Requirements)

- **FR-PROTO-01**: Parse dung chuan Art-Net 4 (UDP 6454).
- **FR-PROTO-02**: Parse dung chuan sACN E1.31 (UDP 5568).
- **FR-PROTO-03**: Ho tro sACN Multicast Join/Leave tu dong.
- **FR-PROTO-04**: Thuc hien Merge HTP/LTP cho moi Universe.
- **FR-PROTO-05**: Xu ly sACN Priority (chi nhan priority cao nhat).

## 4. Yeu cau phi chuc nang (Non-Functional Requirements)

- **NFR-PROTO-01**: Xu ly duoc 16 Universes o 44Hz tren 1 Core.
- **NFR-PROTO-02**: Khong block socket read (dung `select` hoac non-blocking).
- **NFR-PROTO-03**: Memory footprint on dinh (Static buffers).

## 5. Giao tiep Module (Module Interface)

**Public API:**

```c
esp_err_t proto_start(void);
void proto_stop(void);
void proto_reload_config(void); // Goi khi doi mapping
```

## 6. Cau truc du lieu (Data Structures)

**Internal Structs:**

- `artnet_packet_t`: Header ArtNet.
- `sacn_packet_t`: Header sACN.
- `merge_context_t`: Trang thai cua 2 nguon Source A/B cho tung Port.

## 7. Luong xu ly (Processing Flow)

### 7.1 Receive Loop

1. `recvfrom` UDP Socket.
2. Check Header (ArtNet? sACN?).
3. **Parse**: Lay Universe ID, Data, Priority.
4. **Filter**: Universe nay co duoc Mapping vao Port nao khong? (Neu khong -> Drop).
5. **Merge**: Dua vao Merge Engine -> Ket qua cuoi cung.
6. **Output**: `memcpy` vao Buffer cua `SYS_MOD`.
7. **Notify**: `sys_notify_activity`.

## 8. Quan ly loi va trang thai

- **ERR_PROTO_MALFORMED**: Goi tin sai header -> Drop silent.
- **ERR_PROTO_SOCKET_BIND**: Khong bind duoc port -> Log Error.

## 9. Phu thuoc (Dependencies)

- **Phu thuoc**: `lwip/sockets` (ESP-IDF).
- **Trigger**: Can co Event `NET_EVENT_GOT_IP` moi bat dau chay.

## 10. Cau hinh (Configuration)

- `PROTO_MERGE_TIMEOUT_MS`: 2500 (Thoi gian cho source thu 2 mat di).

## 11. Rang buoc ky thuat (Constraints)

- **Endianness**: Mang la Big-Endian, ESP32 la Little-Endian. Can chuyen doi.
- **Multicast**: Can Router ho tro tot IGMP.

## 12. Kich ban su dung (Use Case)

**UC-01: Art-Net Input**

1. Console gui Art-Net Universe 0.
2. `MOD_PROTO` parse thay Universe 0 map vao Port A.
3. Ghi du lieu vao Data Buffer A.
4. Port A phat sang.

**UC-02: sACN Priority Override**

1. Source A (Prio 100) dang gui. Den sang mau A.
2. Source B (Prio 150) bat dau gui.
3. `MOD_PROTO` detect Prio B > A.
4. Chuyen sang nhan du lieu B. Den sang mau B.
5. Source B tat. Sau 2.5s timeout, quay lai Source A.

## 13. Checklist trien khai (Implementation Checklist)

- [ ] Implement `proto_mgr` quan ly socket.
- [ ] Implement `artnet.c` parser.
- [ ] Implement `sacn.c` parser & multicast logic.
- [ ] Implement `merge.c` HTP/LTP logic.

## 14. Lien ket cheo tai lieu

- `MOD_NET.md`: Cung cap socket ha tang.
- `MOD_SYS.md`: Dich den cua du lieu.

## 15. Pham vi mo rong ve sau

- Ho tro RDMNet (E1.33).
- Ho tro phan hoi ArtRdm.

## 16. Tieu chi hoan thanh (Definition of Done)

- Nhan duoc goi tin tu phan mem (Resolume/Madrix).
- Merge duoc 2 nguon tin hieu cung luc.
- sACN Priority hoat dong dung.
