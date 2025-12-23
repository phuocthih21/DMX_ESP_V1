/**
 * @file mod_web_api.c
 * @brief REST API Handlers Implementation
 * 
 * Implements all REST API endpoints for system, DMX, and network configuration.
 */

#include "mod_web_api.h"
#include "mod_web_json.h"
#include "mod_web_validation.h"
#include "mod_web_error.h"
#include "sys_mod.h"
#include "mod_net.h"
#include "mod_web_auth.h"
#include "dmx_types.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MOD_WEB_API";

/* ========== SYSTEM API HANDLERS ========== */

esp_err_t mod_web_api_system_info(httpd_req_t *req)
{
    ESP_LOGD(TAG, "GET /api/sys/info");

    // Get system configuration
    const sys_config_t *cfg = sys_get_config();
    if (cfg == NULL) {
        return mod_web_error_send_500(req, "Failed to get system config");
    }

    // Get network status
    net_status_t net_status;
    net_get_status(&net_status);

    // Calculate uptime (seconds)
    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_sec = (uint32_t)(uptime_us / 1000000);

    // Get free heap
    uint32_t free_heap = esp_get_free_heap_size();

    // Build JSON response
    // Per MOD_WEB.md: Response format should be simple, mapping to sys_config_t
    cJSON *root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "device", cfg->device_label);
    cJSON_AddStringToObject(root, "version", "4.0.0");
    cJSON_AddNumberToObject(root, "uptime", uptime_sec);
    cJSON_AddNumberToObject(root, "free_heap", free_heap);
    const sys_state_t *state = sys_get_state();
    uint8_t cpu = state ? state->cpu_load : 0;
    cJSON_AddNumberToObject(root, "cpu", cpu);
    cJSON_AddBoolToObject(root, "eth_up", net_status.eth_connected);
    cJSON_AddBoolToObject(root, "wifi_up", net_status.wifi_connected);
    
    // IP addresses
    if (net_status.has_ip && strlen(net_status.current_ip) > 0) {
        cJSON_AddStringToObject(root, "ip", net_status.current_ip);
    } else {
        cJSON_AddItemToObject(root, "ip", cJSON_CreateNull());
    }

    // Send response
    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t mod_web_api_system_reboot(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/sys/reboot");

    // Require auth for admin actions
    if (mod_web_auth_is_enabled()) {
        if (!mod_web_auth_check_request(req)) {
            return mod_web_error_send_401(req, "Authentication required");
        }
    }

    // Send response before rebooting
    // Per MOD_WEB.md: Response format {"status": "ok"}
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    // Reboot after short delay
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();

    return ESP_OK;
}

esp_err_t mod_web_api_system_factory(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/sys/factory");

    // Require auth for admin actions
    if (mod_web_auth_is_enabled()) {
        if (!mod_web_auth_check_request(req)) {
            return mod_web_error_send_401(req, "Authentication required");
        }
    }

    // Parse request body to check for confirmation
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret > 0) {
        buf[ret] = '\0';
        cJSON *json = cJSON_Parse(buf);
        if (json != NULL) {
            cJSON *confirm = cJSON_GetObjectItem(json, "confirm");
            if (confirm == NULL || !cJSON_IsTrue(confirm)) {
                cJSON_Delete(json);
                return mod_web_error_send_400(req, "Confirmation required");
            }
            cJSON_Delete(json);
        }
    }

    // Perform factory reset
    esp_err_t err = sys_factory_reset();
    if (err != ESP_OK) {
        return mod_web_error_send_500(req, "Factory reset failed");
    }

    // Send response
    // Per MOD_WEB.md: Response format {"status": "ok"}
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    // Reboot after short delay
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_restart();

    return ESP_OK;
}

esp_err_t mod_web_api_auth_login(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/auth/login");

    // Read body
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return mod_web_error_send_400(req, "Empty request body");
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        return mod_web_error_send_400(req, "Invalid JSON");
    }

    cJSON *pw = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(pw)) {
        cJSON_Delete(json);
        return mod_web_error_send_400(req, "Missing password field");
    }

    const char *password = pw->valuestring;

    if (!mod_web_auth_verify_password(password)) {
        cJSON_Delete(json);
        return mod_web_error_send_401(req, "Invalid credentials");
    }

    // Generate token (8 hours)
    char *token = mod_web_auth_generate_token(8 * 60 * 60);
    if (!token) {
        cJSON_Delete(json);
        return mod_web_error_send_500(req, "Failed to generate token");
    }

    // Build response: { token: "...", expires_seconds: 28800 }
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "token", token);
    cJSON_AddNumberToObject(root, "expires_seconds", (double)(8 * 60 * 60));

    esp_err_t r = mod_web_json_send_response(req, root);

    cJSON_Delete(root);
    free(token);
    cJSON_Delete(json);

    return r;
}

esp_err_t mod_web_api_auth_set_password(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/auth/set_password");

    // Read body
    char buf[256];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return mod_web_error_send_400(req, "Empty request body");
    }
    buf[ret] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        return mod_web_error_send_400(req, "Invalid JSON");
    }

    cJSON *pw = cJSON_GetObjectItem(json, "password");
    if (!cJSON_IsString(pw)) {
        cJSON_Delete(json);
        return mod_web_error_send_400(req, "Missing password field");
    }

    const char *password = pw->valuestring;

    // If auth already enabled, require auth for password change
    if (mod_web_auth_is_enabled() && !mod_web_auth_check_request(req)) {
        cJSON_Delete(json);
        return mod_web_error_send_401(req, "Authentication required to change password");
    }

    esp_err_t err = mod_web_auth_set_password(password);
    if (err != ESP_OK) {
        cJSON_Delete(json);
        return mod_web_error_send_500(req, "Failed to set password");
    }

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    esp_err_t r = mod_web_json_send_response(req, root);
    cJSON_Delete(root);
    cJSON_Delete(json);

    return r;
}

/* ========== DMX API HANDLERS ========== */

esp_err_t mod_web_api_dmx_status(httpd_req_t *req)
{
    ESP_LOGD(TAG, "GET /api/dmx/status");

    // Get system configuration
    const sys_config_t *cfg = sys_get_config();
    if (cfg == NULL) {
        return mod_web_error_send_500(req, "Failed to get system config");
    }

    // Build JSON response
    // Per MOD_WEB.md: JSON Models mapping to sys_config_t
    cJSON *root = cJSON_CreateObject();
    cJSON *ports = cJSON_CreateArray();

    // Get activity timestamps for FPS calculation
    for (int i = 0; i < 4; i++) {
        cJSON *port = cJSON_CreateObject();
        // Copy to local variable to avoid packed member address warning
        dmx_port_cfg_t port_cfg = cfg->ports[i];

        cJSON_AddNumberToObject(port, "port", i);
        cJSON_AddNumberToObject(port, "universe", port_cfg.universe);
        cJSON_AddBoolToObject(port, "enabled", port_cfg.enabled);

        // Calculate FPS from activity (simplified - TODO: implement proper FPS calculation)
        int64_t last_activity = sys_get_last_activity(i);
        uint16_t fps = 0;
        if (last_activity > 0) {
            // Simple FPS estimate (would need proper tracking)
            fps = port_cfg.enabled ? 40 : 0;
        }
        cJSON_AddNumberToObject(port, "fps", fps);

        // Backend type (RMT or UART - simplified)
        cJSON_AddStringToObject(port, "backend", "RMT");

        // Activity counter (simplified)
        cJSON_AddNumberToObject(port, "activity_counter", last_activity > 0 ? 1 : 0);

        cJSON_AddItemToArray(ports, port);
    }

    cJSON_AddItemToObject(root, "ports", ports);

    // Send response
    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

esp_err_t mod_web_api_dmx_config(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/dmx/config");

    // Require auth for admin actions
    if (mod_web_auth_is_enabled()) {
        if (!mod_web_auth_check_request(req)) {
            return mod_web_error_send_401(req, "Authentication required");
        }
    }

    // Read request body
    char buf[512];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return mod_web_error_send_400(req, "Empty request body");
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        return mod_web_error_send_400(req, "Invalid JSON");
    }

    // Extract and validate fields
    cJSON *port_item = cJSON_GetObjectItem(json, "port");
    cJSON *universe_item = cJSON_GetObjectItem(json, "universe");
    cJSON *enabled_item = cJSON_GetObjectItem(json, "enabled");
    cJSON *break_us_item = cJSON_GetObjectItem(json, "break_us");
    cJSON *mab_us_item = cJSON_GetObjectItem(json, "mab_us");

    if (!cJSON_IsNumber(port_item) || !cJSON_IsNumber(universe_item) || !cJSON_IsBool(enabled_item)) {
        cJSON_Delete(json);
        return mod_web_error_send_400(req, "Missing required fields");
    }

    int port = port_item->valueint;
    int universe = universe_item->valueint;
    bool enabled = cJSON_IsTrue(enabled_item);

    // Validate port
    if (!mod_web_validation_port(port)) {
        cJSON_Delete(json);
        return mod_web_error_send_400(req, "Invalid port (0-3)");
    }

    // Validate universe
    if (!mod_web_validation_universe(universe)) {
        cJSON_Delete(json);
        return mod_web_error_send_400(req, "Invalid universe (0-32767)");
    }

    // Build port configuration
    dmx_port_cfg_t new_cfg = {0};
    new_cfg.enabled = enabled;
    new_cfg.universe = (uint16_t)universe;
    new_cfg.protocol = PROTOCOL_ARTNET; // Default

    // Timing configuration (if provided)
    if (cJSON_IsNumber(break_us_item)) {
        int break_us = break_us_item->valueint;
        if (!mod_web_validation_break_us(break_us)) {
            cJSON_Delete(json);
            return mod_web_error_send_400(req, "Invalid break_us (88-500)");
        }
        new_cfg.timing.break_us = (uint16_t)break_us;
    } else {
        new_cfg.timing.break_us = 176; // Default
    }

    if (cJSON_IsNumber(mab_us_item)) {
        int mab_us = mab_us_item->valueint;
        if (!mod_web_validation_mab_us(mab_us)) {
            cJSON_Delete(json);
            return mod_web_error_send_400(req, "Invalid mab_us (8-100)");
        }
        new_cfg.timing.mab_us = (uint16_t)mab_us;
    } else {
        new_cfg.timing.mab_us = 12; // Default
    }

    new_cfg.timing.refresh_rate = 40; // Default

    cJSON_Delete(json);

    // Apply configuration via SYS_MOD
    esp_err_t err = sys_update_port_cfg(port, &new_cfg);
    if (err != ESP_OK) {
        return mod_web_error_send_500(req, "Failed to update port config");
    }

    // Send success response
    // Per MOD_WEB.md: Response format {"status": "ok"}
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    esp_err_t resp_ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return resp_ret;
}

/* ========== NETWORK API HANDLERS ========== */

esp_err_t mod_web_api_network_status(httpd_req_t *req)
{
    ESP_LOGD(TAG, "GET /api/net/status");

    // Get network status
    net_status_t net_status;
    net_get_status(&net_status);

    // Get system configuration for SSID
    const sys_config_t *cfg = sys_get_config();
    if (cfg == NULL) {
        return mod_web_error_send_500(req, "Failed to get system config");
    }

    // Build the existing response first (ip, wifi_ssid, flags)

    // Build JSON response
    // Per MOD_WEB.md: JSON Models mapping to sys_config_t
    cJSON *root = cJSON_CreateObject();

    cJSON_AddBoolToObject(root, "eth_up", net_status.eth_connected);
    cJSON_AddBoolToObject(root, "wifi_up", net_status.wifi_connected);

    // IP address (from net_status)
    if (net_status.has_ip && strlen(net_status.current_ip) > 0) {
        cJSON_AddStringToObject(root, "ip", net_status.current_ip);
    } else {
        cJSON_AddItemToObject(root, "ip", cJSON_CreateNull());
    }

    // WiFi SSID (from config)
    if (strlen(cfg->net.wifi_ssid) > 0) {
        cJSON_AddStringToObject(root, "wifi_ssid", cfg->net.wifi_ssid);
    } else {
        cJSON_AddItemToObject(root, "wifi_ssid", cJSON_CreateNull());
    }

    // Send response
    esp_err_t ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return ret;
}

/* Wi-Fi scan handler: performs a synchronous Wi-Fi scan and returns an array of networks */
esp_err_t mod_web_api_network_scan(httpd_req_t *req)
{
    ESP_LOGI(TAG, "GET /api/network/status/scan");

    // Ensure WiFi driver is up
    wifi_mode_t mode;
    esp_err_t e = esp_wifi_get_mode(&mode);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_get_mode failed: %s", esp_err_to_name(e));
        return mod_web_error_send_500(req, "wifi_not_ready");
    }

    // Start blocking scan
    wifi_scan_config_t scan_conf = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true
    };

    e = esp_wifi_scan_start(&scan_conf, true); // block until done
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_start failed: %s", esp_err_to_name(e));
        return mod_web_error_send_500(req, "scan_start_failed");
    }

    uint16_t ap_num = 0;
    e = esp_wifi_scan_get_ap_num(&ap_num);
    if (e != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_scan_get_ap_num failed: %s", esp_err_to_name(e));
        return mod_web_error_send_500(req, "scan_get_num_failed");
    }

    // Limit number of returned entries to reasonable value to avoid large allocations
    const uint16_t MAX_RESULTS = 64;
    if (ap_num > MAX_RESULTS) ap_num = MAX_RESULTS;

    wifi_ap_record_t *ap_records = NULL;
    if (ap_num > 0) {
        ap_records = (wifi_ap_record_t *)calloc(ap_num, sizeof(wifi_ap_record_t));
        if (!ap_records) return mod_web_error_send_500(req, "no_memory");

        e = esp_wifi_scan_get_ap_records(&ap_num, ap_records);
        if (e != ESP_OK) {
            free(ap_records);
            ESP_LOGW(TAG, "esp_wifi_scan_get_ap_records failed: %s", esp_err_to_name(e));
            return mod_web_error_send_500(req, "scan_get_records_failed");
        }
    }

    cJSON *arr = cJSON_CreateArray();

    for (uint16_t i = 0; i < ap_num; i++) {
        wifi_ap_record_t *r = &ap_records[i];
        cJSON *obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "ssid", (const char *)r->ssid);
        cJSON_AddNumberToObject(obj, "rssi", r->rssi);
        cJSON_AddNumberToObject(obj, "auth_mode", (int)r->authmode);
        cJSON_AddNumberToObject(obj, "channel", r->primary);
        // bssid as hex string
        char bssid_hex[18];
        sprintf(bssid_hex, "%02x:%02x:%02x:%02x:%02x:%02x",
                r->bssid[0], r->bssid[1], r->bssid[2], r->bssid[3], r->bssid[4], r->bssid[5]);
        cJSON_AddStringToObject(obj, "bssid", bssid_hex);
        cJSON_AddBoolToObject(obj, "hidden", strlen((const char *)r->ssid) == 0);
        cJSON_AddItemToArray(arr, obj);
    }

    if (ap_records) free(ap_records);

    esp_err_t ret = mod_web_json_send_response(req, arr);
    cJSON_Delete(arr);
    return ret;
}

/* Generic OPTIONS handler for CORS preflight */
esp_err_t mod_web_api_options(httpd_req_t *req)
{
    ESP_LOGD(TAG, "OPTIONS %s", req->uri);

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type, Authorization");

    return httpd_resp_send(req, NULL, 0);
}

esp_err_t mod_web_api_network_failure(httpd_req_t *req)
{
    ESP_LOGD(TAG, "GET /api/net/failure");

    char buf[256];
    esp_err_t r = net_get_last_failure(buf, sizeof(buf));
    if (r == ESP_OK) {
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "last_failure", buf);
        esp_err_t rr = mod_web_json_send_response(req, root);
        cJSON_Delete(root);
        return rr;
    } else if (r == ESP_ERR_NOT_FOUND) {
        return mod_web_error_send_404(req, "no_failure_recorded");
    } else {
        ESP_LOGW(TAG, "Failed to read last network failure: %s", esp_err_to_name(r));
        return mod_web_error_send_500(req, "failed_to_read_failure");
    }
}

esp_err_t mod_web_api_network_config(httpd_req_t *req)
{
    ESP_LOGI(TAG, "POST /api/net/config");

    // Require auth for admin actions
    if (mod_web_auth_is_enabled()) {
        if (!mod_web_auth_check_request(req)) {
            return mod_web_error_send_401(req, "Authentication required");
        }
    }

    // Read request body
    char buf[1024];
    int ret = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (ret <= 0) {
        return mod_web_error_send_400(req, "Empty request body");
    }
    buf[ret] = '\0';

    // Parse JSON
    cJSON *json = cJSON_Parse(buf);
    if (json == NULL) {
        return mod_web_error_send_400(req, "Invalid JSON");
    }

    // Build network configuration
    net_config_t new_net = {0};

    // Ethernet configuration
    cJSON *eth = cJSON_GetObjectItem(json, "ethernet");
    if (eth != NULL) {
        cJSON *dhcp = cJSON_GetObjectItem(eth, "dhcp");
        cJSON *ip = cJSON_GetObjectItem(eth, "ip");
        cJSON *netmask = cJSON_GetObjectItem(eth, "netmask");
        cJSON *gateway = cJSON_GetObjectItem(eth, "gateway");

        new_net.dhcp_enabled = (dhcp != NULL && cJSON_IsTrue(dhcp));

        if (ip != NULL && cJSON_IsString(ip)) {
            strncpy(new_net.ip, ip->valuestring, sizeof(new_net.ip) - 1);
        }
        if (netmask != NULL && cJSON_IsString(netmask)) {
            strncpy(new_net.netmask, netmask->valuestring, sizeof(new_net.netmask) - 1);
        }
        if (gateway != NULL && cJSON_IsString(gateway)) {
            strncpy(new_net.gateway, gateway->valuestring, sizeof(new_net.gateway) - 1);
        }
    }

    // WiFi Station configuration
    cJSON *wifi_sta = cJSON_GetObjectItem(json, "wifi_sta");
    if (wifi_sta != NULL) {
        cJSON *ssid = cJSON_GetObjectItem(wifi_sta, "ssid");
        cJSON *password = cJSON_GetObjectItem(wifi_sta, "password");

        if (ssid != NULL && cJSON_IsString(ssid)) {
            strncpy(new_net.wifi_ssid, ssid->valuestring, sizeof(new_net.wifi_ssid) - 1);
        }
        if (password != NULL && cJSON_IsString(password)) {
            strncpy(new_net.wifi_pass, password->valuestring, sizeof(new_net.wifi_pass) - 1);
        }
    }

    // WiFi AP (rescue) configuration
    cJSON *wifi_ap = cJSON_GetObjectItem(json, "wifi_ap");
    if (wifi_ap != NULL) {
        cJSON *ssid = cJSON_GetObjectItem(wifi_ap, "ssid");
        cJSON *password = cJSON_GetObjectItem(wifi_ap, "password");
        cJSON *dhcp = cJSON_GetObjectItem(wifi_ap, "dhcp");
        cJSON *ip = cJSON_GetObjectItem(wifi_ap, "ip");
        cJSON *netmask = cJSON_GetObjectItem(wifi_ap, "netmask");
        cJSON *gateway = cJSON_GetObjectItem(wifi_ap, "gateway");
        cJSON *channel = cJSON_GetObjectItem(wifi_ap, "channel");

        if (ssid != NULL && cJSON_IsString(ssid)) {
            strncpy(new_net.ap_ssid, ssid->valuestring, sizeof(new_net.ap_ssid) - 1);
        }
        if (password != NULL && cJSON_IsString(password)) {
            strncpy(new_net.ap_pass, password->valuestring, sizeof(new_net.ap_pass) - 1);
        }
        new_net.ap_dhcp_enabled = (dhcp != NULL && cJSON_IsTrue(dhcp));
        if (ip != NULL && cJSON_IsString(ip)) {
            strncpy(new_net.ap_ip, ip->valuestring, sizeof(new_net.ap_ip) - 1);
        }
        if (netmask != NULL && cJSON_IsString(netmask)) {
            strncpy(new_net.ap_netmask, netmask->valuestring, sizeof(new_net.ap_netmask) - 1);
        }
        if (gateway != NULL && cJSON_IsString(gateway)) {
            strncpy(new_net.ap_gateway, gateway->valuestring, sizeof(new_net.ap_gateway) - 1);
        }
        if (channel != NULL && cJSON_IsNumber(channel)) {
            new_net.ap_channel = (uint8_t)channel->valueint;
        }
    }

    cJSON_Delete(json);

    // Apply configuration via SYS_MOD
    esp_err_t err = sys_update_net_cfg(&new_net);
    if (err != ESP_OK) {
        return mod_web_error_send_500(req, "Failed to update network config");
    }

    // Send success response
    // Per MOD_WEB.md: Response format {"status": "ok"}
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "ok");

    esp_err_t resp_ret = mod_web_json_send_response(req, root);
    cJSON_Delete(root);

    return resp_ret;
}

