#include "esp_log.h"
#include "esp_err.h"
#include "mdns.h"

static const char* TAG = "NET_mDNS";

esp_err_t net_mdns_start(const char* hostname) {
    ESP_LOGI(TAG, "Starting mDNS with hostname=%s", hostname ? hostname : "dmx-node");

    esp_err_t ret = mdns_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "mdns_init failed: %d", ret);
        return ret;
    }

    if (hostname) {
        mdns_hostname_set(hostname);
    }

    mdns_instance_name_set("DMX Node");

    // Advertise HTTP service so users can browse to http://<hostname>.local
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    ESP_LOGI(TAG, "mDNS started: %s.local", hostname ? hostname : "dmx-node");
    return ESP_OK;
}

esp_err_t net_mdns_stop(void) {
    ESP_LOGI(TAG, "Stopping mDNS");
    mdns_free();
    return ESP_OK;
}
