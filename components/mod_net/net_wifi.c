#include "esp_log.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "sys_mod.h"
#include "mod_net.h"
#include "lwip/inet.h"
#include <string.h>

static const char* TAG = "NET_WIFI";

// Biến quản lý trạng thái nội bộ
static esp_netif_t* s_sta_netif = NULL;
static esp_netif_t* s_ap_netif = NULL;
static bool s_wifi_inited = false;

// Forward declaration
esp_err_t net_wifi_start_ap(const char* ssid, const char* pass);

// --- HÀM DỌN DẸP ---
static void net_wifi_destroy_netifs(void) {
    if (s_sta_netif) {
        esp_netif_destroy(s_sta_netif);
        s_sta_netif = NULL;
    }
    if (s_ap_netif) {
        esp_netif_destroy(s_ap_netif);
        s_ap_netif = NULL;
    }
}

// Khởi tạo Wifi Core (chỉ làm 1 lần)
static esp_err_t net_wifi_ensure_init(void) {
    if (s_wifi_inited) return ESP_OK;

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret == ESP_OK) {
        s_wifi_inited = true;
        esp_wifi_set_storage(WIFI_STORAGE_RAM); 
    }
    return ret;
}

// --- KHỞI ĐỘNG CHẾ ĐỘ STATION (STA) ---
esp_err_t net_wifi_start_sta(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Starting WiFi STA...");

    // 1. Dọn dẹp tàn dư cũ
    net_wifi_stop();

    // 2. Init Core
    ESP_ERROR_CHECK(net_wifi_ensure_init());

    // 3. Tạo Netif STA
    s_sta_netif = esp_netif_create_default_wifi_sta();
    if (!s_sta_netif) {
        ESP_LOGE(TAG, "Failed to create STA netif");
        return ESP_FAIL;
    }

    // --- NEW: LOAD IP CONFIG FROM SYS_MOD ---
    const sys_config_t* cfg = sys_get_config();
    if (cfg) {
        if (!cfg->net.dhcp_enabled) {
            ESP_LOGI(TAG, "Configuring Static IP for STA...");
            // Dừng DHCP Client mặc định
            esp_netif_dhcpc_stop(s_sta_netif);
            
            esp_netif_ip_info_t ip_info;
            // Giả định struct sys_config có các trường ip, netmask, gateway cho STA
            if (esp_netif_str_to_ip4(cfg->net.ip, &ip_info.ip) == ESP_OK &&
                esp_netif_str_to_ip4(cfg->net.netmask, &ip_info.netmask) == ESP_OK &&
                esp_netif_str_to_ip4(cfg->net.gateway, &ip_info.gw) == ESP_OK) {
                
                esp_netif_set_ip_info(s_sta_netif, &ip_info);
                ESP_LOGI(TAG, "Static IP applied: %s", cfg->net.ip);
            } else {
                ESP_LOGW(TAG, "Invalid Static IP config, fallback to DHCP");
                esp_netif_dhcpc_start(s_sta_netif);
            }
        } else {
             ESP_LOGI(TAG, "Using DHCP for STA");
        }
    }
    // ----------------------------------------

    // 4. Config Wifi Mode
    wifi_config_t wifi_config = {0};
    strncpy((char*)wifi_config.sta.ssid, ssid ? ssid : "", 32);
    if (pass && strlen(pass) > 0) {
        strncpy((char*)wifi_config.sta.password, pass, 64);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    } else {
        wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    }
    
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    
    // 5. Bắt đầu
    esp_err_t ret = esp_wifi_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_wifi_start failed: %d", ret);
        net_wifi_stop();
        return ret;
    }

    ESP_LOGI(TAG, "WiFi STA Started. Connecting to %s...", wifi_config.sta.ssid);
    esp_wifi_connect();
    
    net_set_current_mode(NET_MODE_WIFI_STA);
    return ESP_OK;
}

// --- KHỞI ĐỘNG CHẾ ĐỘ ACCESS POINT (AP) ---
esp_err_t net_wifi_start_ap(const char* ssid, const char* pass) {
    ESP_LOGI(TAG, "Starting WiFi AP...");

    ESP_ERROR_CHECK(net_wifi_ensure_init());

    s_ap_netif = esp_netif_create_default_wifi_ap();
    if (!s_ap_netif) return ESP_FAIL;

    // Load AP IP Config từ sys_mod
    const sys_config_t* cfg = sys_get_config();
    if (cfg) {
        esp_netif_ip_info_t ip_info;
        if (esp_netif_str_to_ip4(cfg->net.ap_ip, &ip_info.ip) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.ap_netmask, &ip_info.netmask) == ESP_OK &&
            esp_netif_str_to_ip4(cfg->net.ap_gateway, &ip_info.gw) == ESP_OK) {
            
            esp_netif_dhcps_stop(s_ap_netif);
            esp_netif_set_ip_info(s_ap_netif, &ip_info);
            esp_netif_dhcps_start(s_ap_netif);
            ESP_LOGI(TAG, "AP IP set to: %s", cfg->net.ap_ip);
        }
    }

    wifi_config_t wifi_config = {0};
    const char* final_ssid = ssid ? ssid : "DMX_Node_Rescue";
    
    strncpy((char*)wifi_config.ap.ssid, final_ssid, 32);
    wifi_config.ap.ssid_len = strlen(final_ssid);
    wifi_config.ap.max_connection = 4;
    wifi_config.ap.channel = 1;
    
    if (pass && strlen(pass) >= 8) {
        strncpy((char*)wifi_config.ap.password, pass, 64);
        wifi_config.ap.authmode = WIFI_AUTH_WPA_WPA2_PSK;
    } else {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));

    if (esp_wifi_start() != ESP_OK) {
        net_wifi_stop();
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "WiFi AP Started. SSID: %s", wifi_config.ap.ssid);
    net_set_current_mode(NET_MODE_WIFI_AP);
    return ESP_OK;
}

// --- HÀM STOP & CLEANUP ---
esp_err_t net_wifi_stop(void) {
    ESP_LOGI(TAG, "Stopping WiFi...");
    esp_wifi_disconnect();
    esp_wifi_stop();
    net_wifi_destroy_netifs();
    esp_wifi_set_mode(WIFI_MODE_NULL);
    net_set_current_mode(NET_MODE_NONE);
    return ESP_OK;
}