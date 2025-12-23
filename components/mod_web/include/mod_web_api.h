/**
 * @file mod_web_api.h
 * @brief REST API Handlers
 * 
 * Declarations for all REST API endpoint handlers.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========== System API Handlers ========== */

/**
 * @brief GET /api/sys/info
 * 
 * Returns system information (uptime, CPU, RAM, network status).
 */
esp_err_t mod_web_api_system_info(httpd_req_t *req);

/**
 * @brief POST /api/sys/reboot
 * 
 * Reboots the device.
 */
esp_err_t mod_web_api_system_reboot(httpd_req_t *req);

/**
 * @brief POST /api/sys/factory
 * 
 * Performs factory reset.
 */
esp_err_t mod_web_api_system_factory(httpd_req_t *req);

/**
 * @brief POST /api/auth/login
 *
 * Returns a short-lived Bearer token on success.
 */
esp_err_t mod_web_api_auth_login(httpd_req_t *req);

/**
 * @brief POST /api/auth/set_password
 *
 * Set or change admin password. Allowed only when no password exists or client is authenticated.
 */
esp_err_t mod_web_api_auth_set_password(httpd_req_t *req);

/* ========== DMX API Handlers ========== */

/**
 * @brief GET /api/dmx/status
 * 
 * Returns status of all DMX ports.
 */
esp_err_t mod_web_api_dmx_status(httpd_req_t *req);

/**
 * @brief POST /api/dmx/config
 * 
 * Updates DMX port configuration.
 */
esp_err_t mod_web_api_dmx_config(httpd_req_t *req);

/* ========== Network API Handlers ========== */

/**
 * @brief GET /api/network/status
 * 
 * Returns network status (IP, MAC, link status).
 */
esp_err_t mod_web_api_network_status(httpd_req_t *req);

/**
 * @brief GET /api/network/failure
 *
 * Returns a small JSON string describing the last recorded network failure
 */
esp_err_t mod_web_api_network_failure(httpd_req_t *req);

/**
 * @brief POST /api/network/config
 * 
 * Updates network configuration.
 */
esp_err_t mod_web_api_network_config(httpd_req_t *req);

/**
 * @brief GET /api/network/status/scan
 *
 * Returns a list of visible Wi-Fi networks (not implemented yet -> returns empty array)
 */
esp_err_t mod_web_api_network_scan(httpd_req_t *req);

/**
 * @brief OPTIONS handler for CORS preflight
 */
esp_err_t mod_web_api_options(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

