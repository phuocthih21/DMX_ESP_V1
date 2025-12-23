/**
 * @file startup_types.h
 * @brief Boot mode types and constants for MOD_STARTUP
 * 
 * Defines boot modes, protection data structures, and configuration constants
 * for the boot manager module.
 */

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ========== BOOT MODES ========== */

/**
 * @brief Boot mode enumeration
 * 
 * Determines how the system initializes after power-on or reset.
 */
typedef enum {
    BOOT_MODE_NORMAL = 0,      /**< Normal operation - full system init */
    BOOT_MODE_RESCUE,          /**< Rescue mode - minimal init with AP */
    BOOT_MODE_FACTORY_RESET    /**< Factory reset - erase NVS and reboot */
} boot_mode_t;

/* ========== BOOT PROTECTION ========== */

/**
 * @brief Boot protection data stored in NVS
 * 
 * Persisted in NVS namespace "boot_cfg", key "crash_cnt"
 * Used to detect and recover from boot loops.
 */
typedef struct {
    uint8_t crash_counter;     /**< Number of consecutive crashes */
    uint32_t last_boot_time;   /**< Last boot timestamp (epoch seconds) */
    uint8_t image_sha[32];     /**< SHA256 of the running app image */
    uint32_t magic;            /**< Validation magic number */
} boot_protect_data_t;

/* ========== CONSTANTS ========== */

/** Magic number for validating NVS boot protection data */
#define BOOT_PROTECT_MAGIC 0xB007CAFE

/** Crash counter threshold - force rescue mode after this many crashes */
#define BOOT_CRASH_THRESHOLD 3

/** Time to wait before marking system as stable (milliseconds) */
#define BOOT_STABLE_TIME_MS 60000

/** GPIO pin for boot button detection */
#define BOOT_BUTTON_PIN 0

/** Button hold time to enter rescue mode (milliseconds) */
#define RESCUE_HOLD_MS 3000

/** Button hold time to trigger factory reset (milliseconds) */
#define FACTORY_HOLD_MS 10000

#ifdef __cplusplus
}
#endif
