/**
 * @file mod_web_json.c
 * @brief JSON Utility Implementation
 */

#include "mod_web_json.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "MOD_WEB_JSON";

esp_err_t mod_web_json_send_response(httpd_req_t *req, cJSON *json)
{
    if (json == NULL) {
        ESP_LOGE(TAG, "Cannot send NULL JSON");
        return ESP_ERR_INVALID_ARG;
    }

    // Set content type and CORS headers
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");

    // Serialize JSON
    char *json_str = cJSON_PrintUnformatted(json);
    if (json_str == NULL) {
        ESP_LOGE(TAG, "Failed to serialize JSON");
        return ESP_ERR_NO_MEM;
    }

    // Send response
    esp_err_t ret = httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);

    // Free JSON string
    free(json_str);

    return ret;
}

cJSON *mod_web_json_parse_body(httpd_req_t *req, char *buf, size_t buf_size)
{
    if (buf == NULL || buf_size == 0) {
        return NULL;
    }

    // Read request body
    int ret = httpd_req_recv(req, buf, buf_size - 1);
    if (ret <= 0) {
        ESP_LOGE(TAG, "Failed to read request body");
        return NULL;
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr != NULL) {
            ESP_LOGE(TAG, "JSON parse error: %s", error_ptr);
        }
        return NULL;
    }

    return json;
}

