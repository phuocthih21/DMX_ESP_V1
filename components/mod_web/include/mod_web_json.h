/**
 * @file mod_web_json.h
 * @brief JSON Utility Functions
 * 
 * Helper functions for JSON parsing and response formatting.
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Send JSON response
 * 
 * Serializes cJSON object and sends as HTTP response.
 * Sets Content-Type: application/json header.
 * 
 * @param req HTTP request handle
 * @param json cJSON object to send
 * @return ESP_OK on success
 */
esp_err_t mod_web_json_send_response(httpd_req_t *req, cJSON *json);

/**
 * @brief Parse JSON request body
 * 
 * Reads HTTP request body and parses as JSON.
 * 
 * @param req HTTP request handle
 * @param buf Buffer to read into
 * @param buf_size Buffer size
 * @return Parsed cJSON object, or NULL on error
 */
cJSON *mod_web_json_parse_body(httpd_req_t *req, char *buf, size_t buf_size);

#ifdef __cplusplus
}
#endif

