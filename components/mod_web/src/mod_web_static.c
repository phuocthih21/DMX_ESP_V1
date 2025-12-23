/**
 * @file mod_web_static.c
 * @brief Static File Serving Implementation
 * 
 * Serves gzipped static files embedded in firmware.
 */

#include "mod_web_static.h"
#include "esp_log.h"
#include "esp_http_server.h"

static const char *TAG = "MOD_WEB_STATIC";

/* ========== STATIC FILE HANDLERS ========== */

esp_err_t mod_web_static_handler_index(httpd_req_t *req)
{
    // Set headers for gzipped HTML
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // TODO: Replace with actual embedded binary when assets are generated
    // For now, return a simple placeholder
    const char *html = "<!DOCTYPE html><html><head><title>DMX Node</title></head><body><h1>DMX Node Web Interface</h1><p>Static files not yet embedded. Run build script to generate assets.</p></body></html>";
    
    httpd_resp_send(req, html, HTTPD_RESP_USE_STRLEN);
    
    ESP_LOGD(TAG, "Served index.html (placeholder)");
    return ESP_OK;
}

esp_err_t mod_web_static_handler_js(httpd_req_t *req)
{
    // Set headers for gzipped JavaScript
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // TODO: Replace with actual embedded binary
    const char *js = "// JavaScript bundle placeholder";
    
    httpd_resp_send(req, js, HTTPD_RESP_USE_STRLEN);
    
    ESP_LOGD(TAG, "Served app.js (placeholder)");
    return ESP_OK;
}

esp_err_t mod_web_static_handler_css(httpd_req_t *req)
{
    // Set headers for gzipped CSS
    httpd_resp_set_type(req, "text/css");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");

    // TODO: Replace with actual embedded binary
    const char *css = "/* CSS bundle placeholder */";
    
    httpd_resp_send(req, css, HTTPD_RESP_USE_STRLEN);
    
    ESP_LOGD(TAG, "Served style.css (placeholder)");
    return ESP_OK;
}

esp_err_t mod_web_static_handler_favicon(httpd_req_t *req)
{
    // Return an empty 204 to satisfy browser requests for favicon without shipping an asset
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_set_hdr(req, "Cache-Control", "max-age=86400");
    httpd_resp_set_status(req, "204 No Content");
    return httpd_resp_send(req, NULL, 0);
}

