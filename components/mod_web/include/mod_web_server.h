/**
 * @file mod_web_server.h
 * @brief HTTP Server Management
 * 
 * Internal header for HTTP server setup and lifecycle.
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Start HTTP server
 * 
 * Configures and starts esp_http_server with all routes.
 * 
 * @return ESP_OK on success
 */
esp_err_t mod_web_server_start(void);

/**
 * @brief Stop HTTP server
 * 
 * @return ESP_OK on success
 */
esp_err_t mod_web_server_stop(void);

#ifdef __cplusplus
}
#endif

