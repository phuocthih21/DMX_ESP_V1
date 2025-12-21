#pragma once

#include "esp_err.h"
#include "net_types.h"
#include "sys_mod.h"  // For net_config_t

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize network manager
 * 
 * According to API Contract: net_init(cfg) -> esp_err_t
 * If cfg is NULL, reads config from SYS_MOD internally.
 * 
 * @param cfg Network configuration (can be NULL to use sys_config)
 * @return ESP_OK on success
 */
esp_err_t net_init(const net_config_t* cfg);

/**
 * @brief Get network status
 * 
 * According to API Contract: net_get_status(status) -> void
 * This version takes output parameter.
 * 
 * @param status Output parameter for network status (must not be NULL)
 */
void net_get_status(net_status_t* status);

/**
 * @brief Reload network configuration from sys config (e.g. after web change)
 */
void net_reload_config(void);

#ifdef __cplusplus
}
#endif
