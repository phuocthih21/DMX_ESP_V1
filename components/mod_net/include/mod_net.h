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

/**
 * @brief Read last recorded network failure (JSON string) from NVS
 *
 * @param buf Buffer to receive the string (null terminated)
 * @param buf_len Length of buffer
 * @return ESP_OK on success, ESP_ERR_NOT_FOUND if no record, or other NVS error
 */
esp_err_t net_get_last_failure(char* buf, size_t buf_len);

/**
 * @brief Record a short "last action" string to NVS for post-mortem debugging
 *
 * Key is short (<=15 chars). Examples: "eth_start", "wifi_start", "eth_drv_try1"
 */
esp_err_t net_write_last_action(const char *action);

/**
 * @brief Read last action string from NVS
 */
esp_err_t net_get_last_action(char *buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
