/**
 * @file mod_web_error.c
 * @brief Error Response Implementation
 */

#include "mod_web_error.h"
#include "mod_web_json.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "cJSON.h"

static const char *TAG = "MOD_WEB_ERROR";

esp_err_t mod_web_error_send_400(httpd_req_t *req, const char *message)
{
    ESP_LOGW(TAG, "400 Bad Request: %s", message);

    // Per MOD_WEB.md: HTTP 400 for JSON sai format
    httpd_resp_set_status(req, "400 Bad Request");
    httpd_resp_set_type(req, "application/json");

    // Per MOD_WEB.md: Simple error response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "error", message ? message : "Bad Request");

    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t mod_web_error_send_500(httpd_req_t *req, const char *message)
{
    ESP_LOGE(TAG, "500 Internal Server Error: %s", message);

    // Per MOD_WEB.md: HTTP 500 for System Internal Error (Mutex timeout)
    httpd_resp_set_status(req, "500 Internal Server Error");
    httpd_resp_set_type(req, "application/json");

    // Per MOD_WEB.md: Simple error response
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "error", message ? message : "Internal Server Error");

    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t mod_web_error_send_401(httpd_req_t *req, const char *message)
{
    ESP_LOGW(TAG, "401 Unauthorized: %s", message);
    httpd_resp_set_status(req, "401 Unauthorized");
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "error");
    cJSON_AddStringToObject(root, "error", message ? message : "Unauthorized");

    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

