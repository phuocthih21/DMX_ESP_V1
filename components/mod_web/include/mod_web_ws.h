/**
 * @file mod_web_ws.h
 * @brief WebSocket Handler
 * 
 * WebSocket connection management and realtime status broadcasting.
 * Per web_socket_spec_v_1.md: One-way push, stateless, rate-limited.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GET /ws/status -> WebSocket upgrade handler
 */
esp_err_t mod_web_ws_handler(httpd_req_t *req);

/**
 * @brief Initialize WebSocket module
 * 
 * Creates periodic task and registers SYS_MOD event callback.
 * 
 * @return ESP_OK on success
 */
esp_err_t mod_web_ws_init(void);

/**
 * @brief Deinitialize WebSocket module
 */
void mod_web_ws_deinit(void);

#ifdef __cplusplus
}
#endif

