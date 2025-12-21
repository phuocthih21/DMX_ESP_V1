/**
 * @file sys_setup.h
 * @brief System initialization orchestration API
 * 
 * Provides single-call initialization for main.c
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize entire system (one-call setup)
 * 
 * This function orchestrates the complete initialization sequence:
 * 1. Initialize NVS flash
 * 2. Initialize SYS_MOD (config, buffers, timers)
 * 3. Verify all subsystems ready
 * 
 * Error handling: Logs errors and returns first failure code
 * 
 * Usage in main.c:
 * @code
 * void app_main(void) {
 *     esp_err_t ret = sys_setup_all();
 *     if (ret != ESP_OK) {
 *         ESP_LOGE("MAIN", "System setup failed: %d", ret);
 *         return;
 *     }
 *     // Continue with application logic
 * }
 * @endcode
 * 
 * @return ESP_OK on success, error code on failure
 */
esp_err_t sys_setup_all(void);

#ifdef __cplusplus
}
#endif
