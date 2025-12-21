/**
 * @file dmx_types.h
 * @brief Shared data type definitions for DMX Node V4.0
 * 
 * This file contains all core data structures used across modules.
 * Total config size: 512 bytes (aligned for NVS storage)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/* ========== CONSTANTS ========== */

#define SYS_CONFIG_MAGIC    0xDEADBEEF  // Magic number for NVS validation
#define SYS_CONFIG_VERSION  1           // Config version for migration
#define SYS_MAX_PORTS       4           // Number of DMX output ports
#define DMX_UNIVERSE_SIZE   512         // DMX512 standard channel count

/* ========== TIMING CONFIGURATION ========== */

/**
 * @brief DMX timing configuration for each port
 * Size: 6 bytes
 */
typedef struct {
    uint16_t break_us;      // Break time: 88-500us (Default: 176us)
    uint16_t mab_us;        // Mark After Break: 8-100us (Default: 12us)
    uint16_t refresh_rate;  // Refresh rate: 20-44Hz (Default: 40Hz)
} dmx_timing_t;

/* ========== FAIL-SAFE CONFIGURATION ========== */

/**
 * @brief Fail-safe mode enumeration
 */
typedef enum {
    FAILSAFE_HOLD = 0,      // Hold last received values
    FAILSAFE_BLACKOUT = 1,  // Output all zeros
    FAILSAFE_SNAPSHOT = 2   // Output saved snapshot
} failsafe_mode_t;

/**
 * @brief Fail-safe configuration
 * Size: 8 bytes (aligned)
 */
typedef struct {
    uint8_t mode;           // failsafe_mode_t
    uint8_t reserved[3];    // Padding for alignment
    uint16_t timeout_ms;    // Timeout: Default 2000ms
    bool has_snapshot;      // Snapshot available flag
    uint8_t reserved2;      // Padding
} dmx_failsafe_t;

/* ========== PORT CONFIGURATION ========== */

/**
 * @brief Protocol type enumeration
 */
typedef enum {
    PROTOCOL_ARTNET = 0,
    PROTOCOL_SACN = 1
} protocol_type_t;

/**
 * @brief Per-port DMX configuration
 * Size: 16 bytes (aligned to 4-byte boundary)
 */
typedef struct {
    bool enabled;           // Port enable/disable
    uint8_t protocol;       // protocol_type_t
    uint16_t universe;      // Universe ID: 0-32767
    bool rdm_enabled;       // RDM enable (future feature)
    uint8_t reserved[3];    // Padding
    dmx_timing_t timing;    // 6 bytes
    uint8_t reserved2[2];   // Padding to 16 bytes
} dmx_port_cfg_t;

/* ========== NETWORK CONFIGURATION ========== */

/**
 * @brief Network configuration
 * Size: 256 bytes (aligned)
 */
typedef struct {
    bool dhcp_enabled;      // DHCP on/off
    char ip[16];            // Static IP: "192.168.1.100"
    char netmask[16];       // Netmask: "255.255.255.0"
    char gateway[16];       // Gateway: "192.168.1.1"
    char wifi_ssid[32];     // WiFi SSID
    char wifi_pass[64];     // WiFi password
    uint8_t wifi_channel;   // WiFi channel: 1-13
    int8_t wifi_tx_power;   // TX power: 0-78 (dBm * 4)
    char hostname[32];      // mDNS hostname
    uint8_t reserved[77];   // Reserved (was 96, adjusted to make total 256)
} net_config_t;

/* ========== SYSTEM CONFIGURATION ========== */

/**
 * @brief Global system configuration
 * Size: 512 bytes (aligned for NVS blob storage)
 * 
 * Memory layout:
 * Offset   0: Magic number (4 bytes)
 * Offset   4: Version (4 bytes)
 * Offset   8: Device label (32 bytes)
 * Offset  40: LED brightness (1 byte) + padding (23 bytes)
 * Offset  64: Network config (~256 bytes)
 * Offset 320: Port configs (16 * 4 = 64 bytes)
 * Offset 384: Failsafe config (8 bytes)
 * Offset 392: Reserved (120 bytes)
 * Offset 508: CRC32 checksum (4 bytes)
 */
typedef struct __attribute__((packed)) {
    uint32_t magic_number;              // Offset 0: Magic validation
    uint32_t version;                   // Offset 4: Config version
    
    char device_label[32];              // Offset 8: Device name
    uint8_t led_brightness;             // Offset 40: LED brightness 0-100
    uint8_t reserved1[23];              // Padding to offset 64
    
    net_config_t net;                   // Offset 64: Network config
    
    dmx_port_cfg_t ports[SYS_MAX_PORTS]; // Offset 320: Port configs
    dmx_failsafe_t failsafe;            // Offset 384: Failsafe config
    
    uint8_t reserved2[116];             // Offset 392: Future expansion
    
    uint32_t crc32;                     // Offset 508: CRC32 checksum
} sys_config_t;

/* Compile-time size validation */
_Static_assert(sizeof(sys_config_t) == 512, "sys_config_t must be exactly 512 bytes");

/* ========== RUNTIME STATE ========== */

/**
 * @brief Runtime system state (RAM only, not persisted)
 * Size: ~64 bytes
 */
typedef struct {
    void* config_mutex;                 // SemaphoreHandle_t (cast to avoid FreeRTOS header)
    
    bool config_dirty;                  // Unsaved changes flag
    int64_t last_change_time;           // Timestamp of last change (us)
    void* save_timer;                   // esp_timer_handle_t
    
    bool ota_in_progress;               // OTA update flag
    const void* ota_partition;          // esp_partition_t*
    uint32_t ota_handle;                // esp_ota_handle_t
    
    uint8_t* dmx_buffers[SYS_MAX_PORTS]; // Pointers to DMX buffers
    int64_t last_activity[SYS_MAX_PORTS]; // Last packet timestamp (watchdog)
    
    uint8_t reserved[16];               // Future expansion
} sys_state_t;

/* ========== SNAPSHOT DATA ========== */

/**
 * @brief Snapshot data for one port (stored in NVS)
 * NVS key format: "snap_port0" to "snap_port3"
 * Size: 512 bytes
 */
typedef struct {
    uint8_t dmx_data[DMX_UNIVERSE_SIZE]; // DMX channel data
} sys_snapshot_t;

/* ========== EVENT DEFINITIONS ========== */

/**
 * @brief System event types
 */
typedef enum {
    SYS_EVT_CONFIG_LOADED = 0,
    SYS_EVT_CONFIG_SAVED,
    SYS_EVT_NET_CONNECTED,
    SYS_EVT_NET_DISCONNECTED,
    SYS_EVT_DMX_ACTIVE,
    SYS_EVT_ID_ERROR  // Renamed to avoid conflict with sys_event_t::SYS_EVT_ERROR
} sys_event_id_t;
