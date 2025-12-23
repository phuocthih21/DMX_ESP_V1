#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "sys_mod.h"
#include "mod_net.h"
#include "lwip/inet.h"
#include <string.h>

static const char* TAG = "NET_WIFI";
static esp_netif_t* s_sta_netif = NULL;
static esp_netif_t* s_ap_netif = NULL;

// Forward declaration to allow STA startup to fall back to AP when SSID is empty
esp_err_t net_wifi_start_ap(const char* ssid, const char* pass);

esp_err_t net_wifi_start_sta(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Starting WiFi STA (SSID=%s)", ssid ? ssid : "<null>");

    /* If no SSID is provided, start AP mode instead (uses hostname) */
    if (!ssid || ssid[0] == '\0') {
        const sys_config_t* cfg = sys_get_config();
        ESP_LOGI(TAG, "SSID empty; starting AP mode using hostname %s", cfg->net.hostname);
        esp_err_t ap_ret = net_wifi_start_ap(cfg->net.hostname, NULL);
        if (ap_ret != ESP_OK) {
            ESP_LOGW(TAG, "AP fallback failed: %s (%d)", esp_err_to_name(ap_ret), ap_ret);
        }
        return ap_ret;
    }

    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "esp_wifi_init already initialized (IGNORING)");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
        return ret;
    }

    // Create default STA netif and capture pointer
    if (!s_sta_netif) {
        s_sta_netif = esp_netif_create_default_wifi_sta();
        if (!s_sta_netif) {
            ESP_LOGE(TAG, "Failed to create WiFi STA netif");
            return ESP_FAIL;
        }
    } else {
        ESP_LOGD(TAG, "WiFi STA netif already exists");
    }
    esp_netif_t* sta_netif = s_sta_netif;

    // Apply static IP if configured
    const sys_config_t* cfg = sys_get_config();
    if (cfg && !cfg->net.dhcp_enabled && sta_netif) {
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_str_to_ip4(cfg->net.ip, &ip_info.ip) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.netmask, &ip_info.netmask) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.gateway, &ip_info.gw) == ESP_OK) {
            ESP_LOGI(TAG, "Applying static IP %s to WiFi STA", cfg->net.ip);
            esp_netif_set_ip_info(sta_netif, &ip_info);
        } else {
            ESP_LOGW(TAG, "Invalid static IP config in sys_config, skipping");
        }
    }

    wifi_config_t wifi_config = {0};
    if (ssid) {
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    }
    if (pass) {
        strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password));
    }

    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", ret);
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config failed: %d", ret);
        return ret;
    }

    // Prefer performance mode for reliability
    esp_wifi_set_ps(WIFI_PS_NONE);

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %d", ret);
        return ret;
    }

    ret = esp_wifi_connect();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_connect returned: %d", ret);
    }

    // Mark that STA mode is active (connection event will set wifi_connected)
    net_set_current_mode(NET_MODE_WIFI_STA);

    ESP_LOGI(TAG, "WiFi STA started");
    return ESP_OK;
}

esp_err_t net_wifi_start_ap(const char* ssid, const char* pass) {
    /* Prefer provided SSID/pass, otherwise fall back to system AP config */
    const sys_config_t* cfg = sys_get_config();
    const char* use_ssid = (ssid && ssid[0]) ? ssid : (cfg ? cfg->net.ap_ssid : NULL);
    const char* use_pass = (pass && pass[0]) ? pass : (cfg ? cfg->net.ap_pass : NULL);
    const char* ssid_name = use_ssid ? use_ssid : "DMX-AP";

    ESP_LOGI(TAG, "Starting WiFi AP (SSID=%s)", ssid_name);

    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGD(TAG, "esp_wifi_init already initialized (IGNORING)");
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s (%d)", esp_err_to_name(ret), ret);
        return ret;
    }

    // Create default AP netif
    if (!s_ap_netif) {
        s_ap_netif = esp_netif_create_default_wifi_ap();
        if (!s_ap_netif) {
            ESP_LOGE(TAG, "Failed to create WiFi AP netif");
            return ESP_FAIL;
        }
    }

    wifi_config_t wifi_config = {0};
    if (use_ssid) {
        strncpy((char*)wifi_config.ap.ssid, use_ssid, sizeof(wifi_config.ap.ssid)-1);
        wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid)-1] = '\0';
        wifi_config.ap.ssid_len = strnlen((const char*)wifi_config.ap.ssid, sizeof(wifi_config.ap.ssid));
    }
    if (use_pass && use_pass[0]) {
        size_t pass_len = strnlen(use_pass, sizeof(wifi_config.ap.password));
        if (pass_len >= 8) {
            strncpy((char*)wifi_config.ap.password, use_pass, sizeof(wifi_config.ap.password)-1);
            wifi_config.ap.password[sizeof(wifi_config.ap.password)-1] = '\0';
            wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
            ESP_LOGD(TAG, "AP using WPA PSK (password length=%d)", (int)pass_len);
        } else {
            ESP_LOGW(TAG, "AP password too short (%d chars); starting OPEN AP instead", (int)pass_len);
            /* Ensure password buffer is empty for open AP */
            wifi_config.ap.password[0] = '\0';
            wifi_config.ap.authmode = WIFI_AUTH_OPEN;
        }
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    /* Apply AP channel from config if present */
    if (cfg) {
        if (cfg->net.ap_channel >= 1 && cfg->net.ap_channel <= 13) {
            wifi_config.ap.channel = cfg->net.ap_channel;
        } else {
            wifi_config.ap.channel = 6;
        }
    } else {
        wifi_config.ap.channel = 6;
    }
    wifi_config.ap.max_connection = 4;

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %s (%d)", esp_err_to_name(ret), ret);
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config AP failed: %s (%d)", esp_err_to_name(ret), ret);
        return ret;
    }

    /* Apply AP IP/netmask/gateway if provided in config */
    if (cfg && cfg->net.ap_ip[0] != '\0') {
        esp_netif_ip_info_t ip_info = {0};
        if (esp_netif_str_to_ip4(cfg->net.ap_ip, &ip_info.ip) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.ap_netmask, &ip_info.netmask) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.ap_gateway, &ip_info.gw) == ESP_OK) {
            ESP_LOGI(TAG, "Applying AP IP %s", cfg->net.ap_ip);
            esp_netif_set_ip_info(s_ap_netif, &ip_info);
        } else {
            ESP_LOGW(TAG, "Invalid AP IP/netmask/gateway in config");
        }
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start AP failed: %s (%d)", esp_err_to_name(ret), ret);
        return ret;
    }

    // Mark that AP mode is active
    net_set_current_mode(NET_MODE_WIFI_AP);

    ESP_LOGI(TAG, "WiFi AP started: %s", ssid_name);
    return ESP_OK;
}

esp_err_t net_wifi_stop(void) {
    ESP_LOGI(TAG, "Stopping WiFi");
    esp_err_t ret = esp_wifi_stop();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_stop failed: %d", ret);
    }
    ret = esp_wifi_set_mode(WIFI_MODE_NULL);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "esp_wifi_set_mode NULL failed: %d", ret);
    }

    if (s_sta_netif) {
        ESP_LOGI(TAG, "Destroying WiFi STA netif");
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }
    if (s_ap_netif) {
        ESP_LOGI(TAG, "Destroying WiFi AP netif");
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }

    return ESP_OK;
}
