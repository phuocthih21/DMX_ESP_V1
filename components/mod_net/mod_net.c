#include "mod_net.h"
#include "net_types.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "sys_mod.h"
#include "esp_wifi.h"
#include "lwip/ip4_addr.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

static const char* TAG = "MOD_NET";
static net_status_t g_net_status = {
    .current_mode = NET_MODE_NONE,
    .eth_connected = false,
    .wifi_connected = false,
    .has_ip = false,
    .current_ip = "0.0.0.0"
};

/* External module functions */
extern esp_err_t net_eth_start(void);
extern esp_err_t net_eth_stop(void);
extern esp_err_t net_wifi_start_sta(const char* ssid, const char* pass);
extern esp_err_t net_wifi_start_ap(const char* ssid, const char* pass);
extern esp_err_t net_wifi_stop(void);
extern esp_err_t net_mdns_start(const char* hostname);
extern esp_err_t net_mdns_stop(void);

#define NET_WIFI_RETRY_MAX 3

#if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && defined(ETH_EVENT)
static void net_eth_event_handler(void* arg, esp_event_base_t event_base,
                                  int32_t event_id, void* event_data)
{
    if (event_base != ETH_EVENT) return;

    switch (event_id) {
#ifdef ETHERNET_EVENT_CONNECTED
        case ETHERNET_EVENT_CONNECTED:
            ESP_LOGI(TAG, "Ethernet link up");
            g_net_status.eth_connected = true;
            g_net_status.current_mode = NET_MODE_ETHERNET;

            // Prefer Ethernet over WiFi
            net_wifi_stop();
            break;
#endif
#ifdef ETHERNET_EVENT_DISCONNECTED
        case ETHERNET_EVENT_DISCONNECTED:
            ESP_LOGW(TAG, "Ethernet link down");
            g_net_status.eth_connected = false;
            g_net_status.has_ip = false;
            // Fallback to WiFi STA
            {
                const sys_config_t* cfg = sys_get_config();
                net_wifi_start_sta(cfg->net.wifi_ssid, cfg->net.wifi_pass);
                g_net_status.current_mode = NET_MODE_WIFI_STA;
            }
            break;
#endif
        default:
            break;
    }
}
#endif

static void net_ip_event_handler(void* arg, esp_event_base_t event_base,
                                 int32_t event_id, void* event_data)
{
    if (event_base != IP_EVENT) return;

    switch (event_id) {
        case IP_EVENT_ETH_GOT_IP:
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
            snprintf(g_net_status.current_ip, sizeof(g_net_status.current_ip), IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "Got IP: %s", g_net_status.current_ip);
            g_net_status.has_ip = true;

            // Update mode depending on source
            if (event_id == IP_EVENT_ETH_GOT_IP) {
                g_net_status.current_mode = NET_MODE_ETHERNET;
            } else {
                g_net_status.current_mode = NET_MODE_WIFI_STA;
            }

            // Notify SYS_MOD that network is connected
            sys_send_event(SYS_EVT_NET_CONNECTED, NULL, 0);

            // Start mDNS responder with configured hostname
            net_mdns_start(sys_get_config()->net.hostname);
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "WiFi STA lost IP");
            g_net_status.has_ip = false;
            sys_send_event(SYS_EVT_NET_DISCONNECTED, NULL, 0);
            break;

        default:
            break;
    }
}

static void net_wifi_event_handler(void* arg, esp_event_base_t event_base,
                                   int32_t event_id, void* event_data)
{
    if (event_base != WIFI_EVENT) return;

    static int s_retry = 0;

    switch (event_id) {
        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi STA connected");
            g_net_status.wifi_connected = true;
            s_retry = 0;
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi STA disconnected");
            g_net_status.wifi_connected = false;

            if (s_retry < NET_WIFI_RETRY_MAX) {
                ESP_LOGI(TAG, "Retrying WiFi connect (%d/%d)", s_retry+1, NET_WIFI_RETRY_MAX);
                esp_wifi_connect();
                s_retry++;
            } else {
                ESP_LOGW(TAG, "WiFi failed after %d retries, starting AP mode", NET_WIFI_RETRY_MAX);
                const sys_config_t* cfg = sys_get_config();
                net_wifi_start_ap(cfg->net.hostname, ""); // use hostname as AP SSID if no AP config
                s_retry = 0;
            }
            break;

        default:
            break;
    }
}

esp_err_t net_init(const net_config_t* cfg) {
    ESP_LOGI(TAG, "Initializing network manager");

    // Use provided config or read from sys_config
    const sys_config_t* sys_cfg = sys_get_config();
    const net_config_t* net_cfg = cfg ? cfg : &sys_cfg->net;

    // Initialize TCP/IP stack
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %d", err);
        return err;
    }

    // Register event handlers
#if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && defined(ETH_EVENT)
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &net_eth_event_handler, NULL));
#endif
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &net_ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_wifi_event_handler, NULL));

    // Try to start Ethernet first
    err = net_eth_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Ethernet start failed (%d), trying WiFi STA", err);
        net_wifi_start_sta(net_cfg->wifi_ssid, net_cfg->wifi_pass);
    }

    // Wait briefly for link detection; if no link -> start WiFi
    vTaskDelay(pdMS_TO_TICKS(2000));
    if (!g_net_status.eth_connected) {
        ESP_LOGW(TAG, "No Ethernet link detected, starting WiFi STA");
        net_wifi_start_sta(net_cfg->wifi_ssid, net_cfg->wifi_pass);
    }

    // mDNS will start when IP is acquired (in IP event handler)

    return ESP_OK;
}

void net_get_status(net_status_t* status) {
    // API Contract: net_get_status(status) -> void
    if (status) {
        memcpy(status, &g_net_status, sizeof(g_net_status));
    }
}

void net_reload_config(void) {
    ESP_LOGI(TAG, "Reloading network config and applying changes");
    // For now, just restart interfaces according to new config
    const sys_config_t* cfg = sys_get_config();

    // If DHCP disabled and static IP provided, apply to netifs when available (TODO)

    // Try to reconnect WiFi with new settings
    if (cfg->net.wifi_ssid[0]) {
        net_wifi_start_sta(cfg->net.wifi_ssid, cfg->net.wifi_pass);
    }
}
