/**
 * @file mod_web.h
 * @brief MOD_WEB Public API - Web Server Module
 * 
 * This module provides:
 * - HTTP server for serving static files (SPA)
 * - REST API endpoints for system configuration
 * - WebSocket for realtime status updates
 * 
 * Thread-safety: All handlers run in HTTP server context (Core 0, low priority)
 * Memory: ~8KB stack, minimal heap usage
 */

#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== INITIALIZATION ========== */

/**
 * @brief Initialize MOD_WEB module
 * 
 * Initializes:
 * 1. HTTP server with REST API handlers
 * 2. WebSocket handler for realtime updates
 * 3. Static file serving (gzip assets)
 * 
 * Runs on Core 0, low priority task.
 * Stack size: 4096 bytes (WEB_STACK_SIZE).
 * Max clients: 4 (WEB_MAX_CLIENTS).
 * 
 * @return ESP_OK on success, error code otherwise
 */
esp_err_t web_init(void);

/**
 * @brief Stop MOD_WEB module
 * 
 * Stops HTTP server and cleans up resources.
 * 
 * @return ESP_OK on success
 */
esp_err_t web_stop(void);

#ifdef __cplusplus
}
#endif

