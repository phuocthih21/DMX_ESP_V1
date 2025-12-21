#include "mod_dmx.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_attr.h"

static const char* TAG = "DMX_RMT_STUB";

static esp_err_t dmx_rmt_init_port(int port_idx, int gpio_tx) {
    ESP_LOGW(TAG, "RMT stub init for port %d gpio %d (internal stub)", port_idx, gpio_tx);
    // Minimal internal stub: report success so higher-level code can proceed if no real RMT exists.
    return ESP_OK;
}

static void IRAM_ATTR dmx_rmt_send_frame(int port_idx, const uint8_t* data) {
    // No-op internal stub (IRAM attr preserved)
    (void)port_idx;
    (void)data;
}
