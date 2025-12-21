# CODING GUIDELINES & STANDARDS (DMX NODE V4.0)

## 1. Muc dich

Tai lieu nay quy dinh cac chuan muc lap trinh bat buoc cho toan bo du an DMX Node V4.0. Muc tieu la dam bao code co tinh nhat quan, de bao tri va dat chuan Industrial Grade.

## 2. Quy tac Dat ten (Naming Conventions)

Ky hieu: `snake_case` (chu thuong, gach duoi) la chu dao. Su dung Tieng Anh 100%.

### 2.1 Bien & Hang so

- **Bien cuc bo**: `count`, `buffer_ptr`.
- **Bien toan cuc**: `g_variable_name` (Prefix `g_`).
- **Bien tinh (Static)**: `s_variable_name` (Prefix `s_`).
- **Hang so / Macro**: `UPPER_SNAKE_CASE` (VD: `DMX_MAX_CHANNELS`).

### 2.2 Ham (Functions)

Format: `module_verb_noun`

- Dung: `dmx_send_frame()`, `sys_get_config()`.
- Sai: `sendDMX()`, `ConfigGet()`.

### 2.3 Kieu du lieu

Bat buoc hau to `_t`.

- `dmx_config_t`
- `status_code_t`

## 3. Quy tac Code (Coding Style)

- **Indentation**: 4 Spaces. Khong dung Tab.
- **Braces**: K&R Style (Mo ngoac cung dong).
- **Switch-Case**: Luon co `default`.

## 4. Quy tac An toan (Safety Rules)

Dac biet quan trong cho Embedded System.

- **Return Check**: Luon kiem tra gia tri tra ve cua `malloc`, `xTaskCreate`.
- **Memory**:
  - Uu tien Static Allocation.
  - **CAM**: `malloc`/`free` trong vong lap chinh (Run-time loop). Chi dung o Init phase.
- **Concurrency**: Bien chia se giua 2 Task phai duoc bao ve bang Mutex hoac Atomic.

## 5. Quy tac Ghi Log (Logging)

- Su dung `esp_log.h`. Khong dung `printf`.
- **CAM**: Log trong ISR hoac vong lap toc do cao (>10Hz).
- **Tag**: Moi file phai co `static const char *TAG = "module_name";`.

## 6. Cau truc File header

Moi file `.c` va `.h` phai co Header Doxygen:

```c
/**
 * @file mod_example.c
 * @author DMX Node Team
 * @brief Mo ta ngan gon chuc nang file
 * @version 1.0
 * @date 2025-01-01
 */
```

## 7. Quan ly Project

- **Commit Message**: `[MODULE] Action description` (VD: `[DMX] Fix RMT timing bug`).
- **Branching**: `feature/feature-name`, `fix/bug-name`.

## 8. Tieu chi Review Code

Truoc khi Merge, code phai thoa man:

- Khong co Warning khi build.
- Tuan thu dung Naming Convention.
- Khong co Magic Number (so cung).
- Code da duoc Format (clang-format).
