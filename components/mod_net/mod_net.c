#include "mod_net.h"
#include "net_types.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_eth.h"
#include "sys_mod.h"
#include "esp_wifi.h"
#include "nvs.h"
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

    // Use provided config or read a snapshot from sys_config (safer for multi-step operations)
    sys_config_t cfg_snap;
    const net_config_t* net_cfg = NULL;
    if (sys_get_config_snapshot(&cfg_snap, pdMS_TO_TICKS(500)) == ESP_OK) {
        net_cfg = cfg ? cfg : &cfg_snap.net;
    } else {
        ESP_LOGW(TAG, "sys_get_config_snapshot timed out, falling back to live config");
        const sys_config_t* sys_cfg = sys_get_config();
        net_cfg = cfg ? cfg : &sys_cfg->net;
    }

    // Initialize TCP/IP stack
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s (%d)", esp_err_to_name(err), err);
        return err;
    }

    // If there's a recorded network failure from previous boot, surface it in the logs
    char _last_failure[256];
    esp_err_t _lf = net_get_last_failure(_last_failure, sizeof(_last_failure));
    if (_lf == ESP_OK) {
        ESP_LOGW(TAG, "Previous network failure recorded: %s", _last_failure);
    } else if (_lf != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read last network failure: %s", esp_err_to_name(_lf));
    } else {
        ESP_LOGD(TAG, "No previous network failure recorded");
    }

    // Also surface the last recorded "action" to help post-mortem diagnosis
    char _last_act[64];
    esp_err_t _la = net_get_last_action(_last_act, sizeof(_last_act));
    if (_la == ESP_OK) {
        ESP_LOGW(TAG, "Last network action before crash/stop: %s", _last_act);
    } else if (_la != ESP_ERR_NOT_FOUND) {
        ESP_LOGW(TAG, "Failed to read last network action: %s", esp_err_to_name(_la));
    } else {
        ESP_LOGD(TAG, "No previous network action recorded");
    }

    // Register event handlers
#if defined(CONFIG_ETH_SPI_ETHERNET_W5500) && defined(ETH_EVENT)
    {
        esp_err_t _er = esp_event_handler_register(ETH_EVENT, ESP_EVENT_ANY_ID, &net_eth_event_handler, NULL);
        if (_er != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register ETH_EVENT handler: %s", esp_err_to_name(_er));
        }
    }
#endif
    {
        esp_err_t _er = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &net_ip_event_handler, NULL);
        if (_er != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register IP_EVENT handler: %s", esp_err_to_name(_er));
        }
    }
    {
        esp_err_t _er = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &net_wifi_event_handler, NULL);
        if (_er != ESP_OK) {
            ESP_LOGW(TAG, "Failed to register WIFI_EVENT handler: %s", esp_err_to_name(_er));
        }
    }

    // Try to start Ethernet first
    net_write_last_action("eth_start");
    err = net_eth_start();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Ethernet start failed (%s), will try WiFi STA", esp_err_to_name(err));
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        net_write_last_action("wifi_start");

        // Re-read config snapshot before WiFi start
        sys_config_t cfg2;
        if (sys_get_config_snapshot(&cfg2, pdMS_TO_TICKS(500)) == ESP_OK) {
            if (cfg2.net.wifi_enabled) {
                ESP_LOGI(TAG, "NET: WiFi STA starting (SSID=%s)", cfg2.net.wifi_ssid[0] ? cfg2.net.wifi_ssid : "<empty>");
                net_wifi_start_sta(cfg2.net.wifi_ssid, cfg2.net.wifi_pass);
                for (int i = 0; i < 10 && !g_net_status.wifi_connected; ++i) vTaskDelay(pdMS_TO_TICKS(500));
                if (!g_net_status.wifi_connected) {
                    ESP_LOGW(TAG, "WiFi STA did not connect after 5s, falling back to AP");
                    net_write_last_action("ap_start");
                    net_wifi_start_ap(NULL, NULL);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
        } else {
            const sys_config_t* live = sys_get_config();
            if (live->net.wifi_enabled) {
                ESP_LOGI(TAG, "NET: WiFi STA starting (SSID=%s)", live->net.wifi_ssid[0] ? live->net.wifi_ssid : "<empty>");
                net_wifi_start_sta(live->net.wifi_ssid, live->net.wifi_pass);
                for (int i = 0; i < 10 && !g_net_status.wifi_connected; ++i) vTaskDelay(pdMS_TO_TICKS(500));
                if (!g_net_status.wifi_connected) {
                    ESP_LOGW(TAG, "WiFi STA did not connect after 5s, falling back to AP");
                    net_write_last_action("ap_start");
                    net_wifi_start_ap(NULL, NULL);
                    vTaskDelay(pdMS_TO_TICKS(5000));
                }
            }
        }
    }

    // Wait up to 5s for link detection (poll every 500ms); if no link -> start WiFi
    for (int i = 0; i < 10 && !g_net_status.eth_connected; ++i) {
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    if (!g_net_status.eth_connected) {
        ESP_LOGW(TAG, "No Ethernet link detected after 5s, starting WiFi STA");
        sys_send_event(SYS_EVT_ERROR, NULL, 0);
        // Use latest config snapshot before starting WiFi
        sys_config_t cfg2;
        if (sys_get_config_snapshot(&cfg2, pdMS_TO_TICKS(500)) == ESP_OK) {
            if (cfg2.net.wifi_enabled) {
                net_write_last_action("wifi_start");
                net_wifi_start_sta(cfg2.net.wifi_ssid, cfg2.net.wifi_pass);
            }
        } else {
            const sys_config_t* live = sys_get_config();
            if (live->net.wifi_enabled) {
                net_write_last_action("wifi_start");
                net_wifi_start_sta(live->net.wifi_ssid, live->net.wifi_pass);
            }
        }
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

void net_set_current_mode(net_mode_t mode) {
    g_net_status.current_mode = mode;
    /* Reset connection flags until events come in for the new mode */
    if (mode == NET_MODE_WIFI_STA || mode == NET_MODE_WIFI_AP) {
        g_net_status.wifi_connected = false;
    }
    if (mode == NET_MODE_ETHERNET) {
        g_net_status.eth_connected = false;
    }
}

void net_get_status(net_status_t* status) {
    // API Contract: net_get_status(status) -> void
    if (status) {
        memcpy(status, &g_net_status, sizeof(g_net_status));
    }
}

esp_err_t net_get_last_failure(char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;

    nvs_handle_t nvs;
    esp_err_t ret = nvs_open("err_log", NVS_READONLY, &nvs);
    if (ret != ESP_OK) return ret;

    size_t required = buf_len;
    /* Use same short key as write routine */
    ret = nvs_get_blob(nvs, "net_fail", buf, &required);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        /* Try very short fallback key if older fallback was used */
        required = buf_len;
        ret = nvs_get_blob(nvs, "nf", buf, &required);
    }
    nvs_close(nvs);

    if (ret == ESP_OK) return ESP_OK;
    if (ret == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NOT_FOUND;
    return ret;
}

esp_err_t net_write_last_action(const char *action)
{
    if (!action) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t r = nvs_open("err_log", NVS_READWRITE, &nvs);
    if (r != ESP_OK) return r;

    /* Keep key very short to avoid NVS key length errors */
    const char *key = "net_act"; // <= 7 chars
    r = nvs_set_str(nvs, key, action);
    if (r == ESP_OK) {
        r = nvs_commit(nvs);
        if (r == ESP_OK) ESP_LOGD(TAG, "Recorded last action: %s", action);
        else ESP_LOGW(TAG, "Failed to commit last action: %s", esp_err_to_name(r));
    } else {
        ESP_LOGW(TAG, "Failed to write last action: %s", esp_err_to_name(r));
    }

    nvs_close(nvs);
    return r;
}

esp_err_t net_get_last_action(char *buf, size_t buf_len)
{
    if (!buf || buf_len == 0) return ESP_ERR_INVALID_ARG;
    nvs_handle_t nvs;
    esp_err_t r = nvs_open("err_log", NVS_READONLY, &nvs);
    if (r != ESP_OK) return r;

    size_t required = buf_len;
    r = nvs_get_str(nvs, "net_act", buf, &required);
    nvs_close(nvs);

    if (r == ESP_OK) return ESP_OK;
    if (r == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NOT_FOUND;
    return r;
}

void net_reload_config(void) 
{
    ESP_LOGI(TAG, "Reloading network config and applying changes");
    // For now, just restart interfaces according to new config
    const sys_config_t* cfg = sys_get_config();

    // If DHCP disabled and static IP provided, apply to netifs when available (TODO)

    // Try to reconnect WiFi with new settings
    if (cfg->net.wifi_ssid[0]) {
        net_write_last_action("wifi_reload");
        net_wifi_start_sta(cfg->net.wifi_ssid, cfg->net.wifi_pass);
    }
}