#include "mod_net.h"
#include "net_types.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include <string.h>

static const char* TAG = "MOD_NET";

// Trạng thái mạng toàn cục
static net_status_t g_net_status = {
    .current_mode = NET_MODE_NONE,
    .eth_connected = false,
    .wifi_connected = false,
    .has_ip = false,
    .current_ip = "0.0.0.0"
};

// Event Group để đợi kết nối WiFi
static EventGroupHandle_t s_net_event_group;
#define WIFI_CONNECTED_BIT BIT0

extern esp_err_t net_eth_start(void);
extern esp_err_t net_eth_stop(void);
extern esp_err_t net_wifi_start_sta(const char* ssid, const char* pass);
extern esp_err_t net_wifi_start_ap(const char* ssid, const char* pass);
extern esp_err_t net_wifi_stop(void);
extern esp_err_t net_mdns_start(const char* hostname);

// ======================================================
// PHẦN 1: CÁC HÀM HELPER (BỔ SUNG ĐỂ FIX LỖI LINKER)
// ======================================================

/**
 * @brief Cập nhật chế độ mạng hiện tại (Được gọi từ net_wifi.c / net_eth.c)
 */
void net_set_current_mode(net_mode_t mode) {
    g_net_status.current_mode = mode;
}

/**
 * @brief Lấy trạng thái mạng (Được gọi từ mod_web)
 */
void net_get_status(net_status_t* status) {
    if (status) {
        memcpy(status, &g_net_status, sizeof(net_status_t));
    }
}

/**
 * @brief Lấy thông tin lỗi lần trước từ NVS (Được gọi từ mod_web)
 */
esp_err_t net_get_last_failure(char* buf, size_t buf_len) {
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;
    
    nvs_handle_t nvs;
    esp_err_t err = nvs_open("err_log", NVS_READONLY, &nvs);
    if (err != ESP_OK) {
        // Nếu không có log lỗi, trả về chuỗi rỗng thay vì lỗi
        buf[0] = '\0'; 
        return ESP_OK; 
    }

    size_t required = buf_len;
    err = nvs_get_str(nvs, "net_fail", buf, &required);
    if (err != ESP_OK) {
        buf[0] = '\0';
    }
    
    nvs_close(nvs);
    return ESP_OK;
}

// Hàm ghi lỗi (Optionally used by internal modules)
void net_record_failure_internal(const char* json_log) {
    nvs_handle_t nvs;
    if (nvs_open("err_log", NVS_READWRITE, &nvs) == ESP_OK) {
        nvs_set_str(nvs, "net_fail", json_log);
        nvs_commit(nvs);
        nvs_close(nvs);
    }
}

// ======================================================
// PHẦN 2: LOGIC KHỞI TẠO (SEQUENTIAL BOOT)
// ======================================================

// Handler cập nhật trạng thái IP/Event
static void net_event_handler(void* arg, esp_event_base_t event_base,
                              int32_t event_id, void* event_data)
{
    if (event_base == IP_EVENT && (event_id == IP_EVENT_STA_GOT_IP || event_id == IP_EVENT_ETH_GOT_IP)) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        
        // Cập nhật IP vào struct global
        esp_ip4addr_ntoa(&event->ip_info.ip, g_net_status.current_ip, sizeof(g_net_status.current_ip));
        ESP_LOGI(TAG, "Got IP: %s", g_net_status.current_ip);
        
        g_net_status.has_ip = true;
        
        if (event_id == IP_EVENT_STA_GOT_IP) {
            g_net_status.wifi_connected = true;
            if(s_net_event_group) xEventGroupSetBits(s_net_event_group, WIFI_CONNECTED_BIT);
        } else {
            g_net_status.eth_connected = true;
        }
        
        net_mdns_start(sys_get_config()->net.hostname);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        g_net_status.wifi_connected = false;
        g_net_status.has_ip = false;
        // Tự động reconnect nếu đang ở mode STA (nhưng net_init sẽ quyết định timeout)
        if (g_net_status.current_mode == NET_MODE_WIFI_STA) {
             esp_wifi_connect(); 
        }
    }
}

esp_err_t net_init(const net_config_t* cfg) {
    ESP_LOGI(TAG, "Initializing Network Manager (Sequential Mode)");

    // Reset status
    memset(&g_net_status, 0, sizeof(net_status_t));
    g_net_status.current_mode = NET_MODE_NONE;

    esp_netif_init();
    // Lưu ý: esp_event_loop_create_default() thường được gọi ở main.c. 
    // Nếu main.c đã gọi, gọi lại sẽ báo lỗi ESP_ERR_INVALID_STATE, ta bỏ qua lỗi đó.
    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Event loop error: %d", err);
    }

    s_net_event_group = xEventGroupCreate();

    // Register Event Handlers
    esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &net_event_handler, NULL, NULL);
    esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_event_handler, NULL, NULL);

    const sys_config_t* sys_cfg = sys_get_config();

    // --- STEP 1: ETHERNET ---
    if (net_eth_start() == ESP_OK) {
        g_net_status.current_mode = NET_MODE_ETHERNET;
        // Đợi IP của Ethernet một chút nếu cần, nhưng thường Eth nhanh nên ta return luôn
        // Logic đợi IP có thể thêm vào sau nếu cần.
        return ESP_OK; 
    }

    // Nếu Eth thất bại: Tắt hoàn toàn để tránh nhiễu
    ESP_LOGW(TAG, "Ethernet failed. Stopping Ethernet completely...");
    net_eth_stop(); 

    // --- STEP 2: WIFI STA ---
    ESP_LOGI(TAG, "Switching to WiFi STA...");
    if (net_wifi_start_sta(sys_cfg->net.wifi_ssid, sys_cfg->net.wifi_pass) == ESP_OK) {
        g_net_status.current_mode = NET_MODE_WIFI_STA;

        // Đợi tối đa 10s để có IP
        EventBits_t bits = xEventGroupWaitBits(s_net_event_group,
                                               WIFI_CONNECTED_BIT,
                                               pdFALSE,
                                               pdFALSE,
                                               pdMS_TO_TICKS(10000));

        if (bits & WIFI_CONNECTED_BIT) {
            ESP_LOGI(TAG, "WiFi STA Connected & Got IP!");
            return ESP_OK; // Xong!
        }
        
        ESP_LOGW(TAG, "WiFi STA Timeout (10s). Stopping STA...");
    }

    // Nếu WiFi STA thất bại hoặc Timeout: Tắt STA hoàn toàn
    net_wifi_stop();

    // --- STEP 3: WIFI AP (Fallback cuối cùng) ---
    ESP_LOGW(TAG, "Starting WiFi AP Mode...");
    net_wifi_start_ap(sys_cfg->net.ap_ssid, sys_cfg->net.ap_pass);
    g_net_status.current_mode = NET_MODE_WIFI_AP;

    return ESP_OK;
}