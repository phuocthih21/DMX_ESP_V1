/**
 * @file mod_status.h
 * @brief Status LED module public API
 * 
 * Controls a WS2812B LED for visual system status feedback
 */

#pragma once

#include "status_types.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize status LED module
 * 
 * - Configures RMT Channel 7 for WS2812B control
 * - Creates FreeRTOS task for LED animation
 * - Sets initial status to STATUS_BOOTING
 * 
 * @param gpio_pin GPIO pin number (default: 28)
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t status_init(int gpio_pin);

/**
 * @brief Set status code with priority checking
 * 
 * Status codes have priority levels:
 * - STATUS_OTA (0) - Cannot be overridden
 * - STATUS_ERROR (1)
 * - STATUS_IDENTIFY (2)
 * - STATUS_DMX_WARN (3)
 * - STATUS_NO_NET (4)
 * - STATUS_BOOTING (5)
 * - STATUS_NET_ETH (6)
 * - STATUS_NET_WIFI (7)
 * - STATUS_NET_AP (8)
 * 
 * Lower number = higher priority. Higher priority status cannot
 * be overridden by lower priority status.
 * 
 * @param code New status code
 */
void status_set_code(status_code_t code);

/**
 * @brief Set global LED brightness
 * 
 * Affects all status colors. Default is 30% to avoid eye strain.
 * 
 * @param brightness Brightness percentage (0-100)
 */
void status_set_brightness(uint8_t brightness);

/**
 * @brief Get current status code
 * 
 * @return Current active status code
 */
status_code_t status_get_code(void);

/**
 * @brief Trigger identify mode temporarily
 * 
 * Flashes white strobe for specified duration, then returns
 * to previous status code.
 * 
 * @param duration_ms Duration in milliseconds
 */
void status_trigger_identify(uint32_t duration_ms);

#ifdef __cplusplus
}
#endif
