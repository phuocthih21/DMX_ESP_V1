/**
 * @file mod_web_error.h
 * @brief Error Response Helpers
 * 
 * Functions for sending standardized error responses.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send 400 Bad Request error
 * 
 * @param req HTTP request handle
 * @param message Error message
 * @return ESP_OK
 */
esp_err_t mod_web_error_send_400(httpd_req_t *req, const char *message);

/**
 * @brief Send 500 Internal Server Error
 * 
 * @param req HTTP request handle
 * @param message Error message
 * @return ESP_OK
 */
esp_err_t mod_web_error_send_500(httpd_req_t *req, const char *message);

/**
 * @brief Send 401 Unauthorized error
 *
 * @param req HTTP request handle
 * @param message Error message
 * @return ESP_OK
 */
esp_err_t mod_web_error_send_401(httpd_req_t *req, const char *message);

/**
 * @brief Send 404 Not Found
 */
esp_err_t mod_web_error_send_404(httpd_req_t *req, const char *message);

#ifdef __cplusplus
}
#endif

