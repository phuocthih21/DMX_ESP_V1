/**
 * @file mod_web_static.h
 * @brief Static File Serving
 * 
 * Handlers for serving gzipped static files (SPA assets).
 */

#pragma once

#include "esp_err.h"
#include "esp_http_server.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief GET / -> index.html.gz
 */
esp_err_t mod_web_static_handler_index(httpd_req_t *req);

/**
 * @brief GET /app.js -> app.js.gz
 */
esp_err_t mod_web_static_handler_js(httpd_req_t *req);

/**
 * @brief GET /style.css -> style.css.gz
 */
esp_err_t mod_web_static_handler_css(httpd_req_t *req);

#ifdef __cplusplus
}
#endif

