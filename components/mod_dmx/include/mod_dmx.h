#pragma once

#include "esp_err.h"
#include "dmx_types.h"
#include "sys_mod.h"  // For sys_config_t

#ifdef __cplusplus
extern "C" {
#endif

// Public API
esp_err_t dmx_init(void);
esp_err_t dmx_start(void);
esp_err_t dmx_stop(void);

/**
 * @brief Apply new timing configuration for a port (hot-swap)
 * 
 * According to API Contract: MOD_DMX Interface
 * Timing is automatically read from sys_config each frame, but this
 * function can be called to force immediate update.
 * 
 * @param port Port index (0-3)
 * @param timing New timing configuration
 */
void dmx_apply_new_timing(int port, const dmx_timing_t* timing);

/**
 * @brief Initialize DMX driver with config (API Contract alias)
 * 
 * This is an alias for dmx_init() which reads config from sys_config internally.
 * Provided for API Contract compliance.
 * 
 * @param cfg System configuration (currently unused, reads from sys_config)
 * @return ESP_OK on success
 */
esp_err_t dmx_driver_init(const sys_config_t* cfg);

// Pin mapping
#define GPIO_PORT_A_TX  12
#define GPIO_PORT_B_TX  13
#define GPIO_PORT_C_TX  14
#define GPIO_PORT_C_DE  15
#define GPIO_PORT_D_TX  16
#define GPIO_PORT_D_DE  17

// Ports
#define DMX_PORT_A 0
#define DMX_PORT_B 1
#define DMX_PORT_C 2
#define DMX_PORT_D 3
#define DMX_PORT_COUNT 4

// DMX constants
#define DMX_START_CODE 0x00
#define DMX_FRAME_SIZE 513 // Start code + 512 channels
#define DMX_UNIVERSE_SIZE 512

#ifdef __cplusplus
}
#endif
