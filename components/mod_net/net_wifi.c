#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "sys_mod.h"
#include "lwip/inet.h"
#include <string.h>

static const char* TAG = "NET_WIFI";

esp_err_t net_wifi_start_sta(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Starting WiFi STA (SSID=%s)", ssid ? ssid : "<null>");

    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
        return ret;
    }

    // Create default STA netif and capture pointer
    esp_netif_t* sta_netif = esp_netif_create_default_wifi_sta();

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
        strncpy((char*)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
        wifi_config.sta.ssid[sizeof(wifi_config.sta.ssid) - 1] = '\0';
    }
    if (pass) {
        strncpy((char*)wifi_config.sta.password, pass, sizeof(wifi_config.sta.password) - 1);
        wifi_config.sta.password[sizeof(wifi_config.sta.password) - 1] = '\0';
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

    ESP_LOGI(TAG, "WiFi STA started");
    return ESP_OK;
}

esp_err_t net_wifi_start_ap(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Starting WiFi AP (SSID=%s)", ssid ? ssid : "DMX-AP");

    esp_err_t ret = esp_wifi_init(&(wifi_init_config_t)WIFI_INIT_CONFIG_DEFAULT());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %d", ret);
        return ret;
    }

    // Create default AP netif
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {0};
    if (ssid) {
        strncpy((char*)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid) - 1);
        wifi_config.ap.ssid[sizeof(wifi_config.ap.ssid) - 1] = '\0';
        wifi_config.ap.ssid_len = strlen((char*)wifi_config.ap.ssid);
    }
    if (pass) {
        strncpy((char*)wifi_config.ap.password, pass, sizeof(wifi_config.ap.password) - 1);
        wifi_config.ap.password[sizeof(wifi_config.ap.password) - 1] = '\0';
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    wifi_config.ap.channel = 6;
    wifi_config.ap.max_connection = 4;

    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_mode failed: %d", ret);
        return ret;
    }

    ret = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_set_config AP failed: %d", ret);
        return ret;
    }

    ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start AP failed: %d", ret);
        return ret;
    }

    ESP_LOGI(TAG, "WiFi AP started: %s", ssid ? ssid : "DMX-AP");
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
    return ESP_OK;
}
