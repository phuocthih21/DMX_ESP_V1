/**
 * @file mod_web_routes.h
 * @brief URI Route Registration
 * 
 * Registers all HTTP endpoints and WebSocket handlers.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Register all URI handlers
 * 
 * Registers:
 * - Static file handlers (/, /app.js, /style.css)
 * - REST API handlers (/api/...)
 * - WebSocket handler (/ws/status)
 * 
 * @param server HTTP server handle
 * @return ESP_OK on success
 */
esp_err_t mod_web_register_routes(httpd_handle_t server);

#ifdef __cplusplus
}
#endif

