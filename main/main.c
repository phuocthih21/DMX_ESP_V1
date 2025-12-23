/*
 * @file main.c
 * @brief System orchestrator (MAIN) - initializes all modules and handles boot modes
 */

#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "startup.h"
#include "sys_setup.h"
#include "sys_mod.h"
#include "status_led.h"
#include "mod_net.h"

/* net_wifi_start_ap is not declared in mod_net public header; forward declare */
extern esp_err_t net_wifi_start_ap(const char* ssid, const char* pass);

#include "mod_proto.h"
#include "mod_dmx.h"
#include "mod_web.h"

static const char *TAG = "MAIN";

/* Deferred DMX initialization to avoid blocking core 0 */
static void dmx_deferred_init_task(void *arg)
{
    (void)arg;
    const sys_config_t* cfg = sys_get_config();
    const int max_attempts = 3;
    for (int attempt = 1; attempt <= max_attempts; ++attempt) {
        ESP_LOGI(TAG, "DMX deferred init attempt %d/%d", attempt, max_attempts);
        esp_err_t r = dmx_driver_init(cfg);
        if (r == ESP_OK) {
            ESP_LOGI(TAG, "DMX driver initialized (deferred)");
            esp_err_t s = dmx_start();
            if (s == ESP_OK) {
                ESP_LOGI(TAG, "DMX transmission started (deferred)");
            } else {
                ESP_LOGW(TAG, "DMX start failed (deferred): %s", esp_err_to_name(s));
            }
            vTaskDelete(NULL);
            return;
        }
        ESP_LOGW(TAG, "DMX deferred init failed: %s (attempt %d)", esp_err_to_name(r), attempt);
        vTaskDelay(pdMS_TO_TICKS(500 * attempt));
    }
    ESP_LOGW(TAG, "DMX deferred init failed after %d attempts; DMX disabled", max_attempts);
    vTaskDelete(NULL);
}

/* --- NORMAL MODE --- */
static void init_normal_mode(void)
{
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  ENTERING NORMAL MODE");
    ESP_LOGI(TAG, "========================================");

    // Network
    esp_err_t ret = net_init(NULL);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Network initialization failed: %s", esp_err_to_name(ret));
        status_set_code(STATUS_ERROR);
        ESP_LOGE(TAG, "Critical error - restarting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
        return;
    }
    ESP_LOGI(TAG, "Network initialized");

    // Protocol stack (ArtNet/sACN)
    ret = proto_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Protocol initialization failed: %s", esp_err_to_name(ret));
        status_set_code(STATUS_ERROR);
    } else {
        ESP_LOGI(TAG, "Protocol stack initialized");
    }

    // Web server
    ret = web_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web server initialization failed: %s", esp_err_to_name(ret));
        status_set_code(STATUS_ERROR);
    } else {
        ESP_LOGI(TAG, "Web server initialized");
    }

    // DMX deferred init on Core 1
    BaseType_t _res = xTaskCreatePinnedToCore(dmx_deferred_init_task, "dmx_init_deferred", 4096, NULL, tskIDLE_PRIORITY + 2, NULL, 1);
    if (_res != pdPASS) {
        ESP_LOGW(TAG, "Failed to spawn DMX deferred init task; DMX will remain disabled");
    } else {
        ESP_LOGI(TAG, "DMX initialization deferred to background task");
    }

    // Initial LED state: No network until events update it
    status_set_code(STATUS_NO_NET);

    // Start stability timer (marks system stable after configured interval)
    boot_protect_start_stable_timer();

    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  NORMAL MODE INITIALIZATION COMPLETE");
    ESP_LOGI(TAG, "========================================");
}

/* --- RESCUE MODE --- */
static void init_rescue_mode(void)
{
    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  ENTERING RESCUE MODE");
    ESP_LOGW(TAG, "========================================");

    // Start WiFi AP (use helper if available)
    esp_err_t ret = net_wifi_start_ap("DMX-RESCUE", "12345678");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start rescue AP: %s", esp_err_to_name(ret));
        status_set_code(STATUS_ERROR);
        ESP_LOGE(TAG, "Critical error - restarting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
        return;
    }
    ESP_LOGW(TAG, "WiFi AP started: SSID=DMX-RESCUE");

    // Web server (limited)
    ret = web_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Web server (rescue) init failed: %s", esp_err_to_name(ret));
        status_set_code(STATUS_ERROR);
    }

    // Indicate rescue mode with LED
    status_set_code(STATUS_NET_AP);

    ESP_LOGW(TAG, "========================================");
    ESP_LOGW(TAG, "  RESCUE MODE READY");
    ESP_LOGW(TAG, "========================================");
}

/* --- app_main --- */
void app_main(void)
{
    ESP_LOGI(TAG, "");
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "  DMX Node V4.0 - Firmware Boot");
    ESP_LOGI(TAG, "  Build: %s %s", __DATE__, __TIME__);
    ESP_LOGI(TAG, "========================================");
    ESP_LOGI(TAG, "");

    /* * 1. Initialize NVS Flash first 
     * Critical for config, crash monitor, and system stability
     */
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition needs erase; erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS flash initialized");

    /* * 2. Initialize Status LED IMMEDIATELY 
     * Moved up to ensure we can report errors during System Setup or Boot Checks 
     */
    ret = status_init(48); // GPIO 48
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "FATAL: Status LED init failed: %s", esp_err_to_name(ret));
        // Continue anyway, but logging is the only output mechanism now
    } else {
        ESP_LOGI(TAG, "Status LED initialized (GPIO 48)");
        status_set_code(STATUS_BOOTING);
    }

    /* * 3. Pre-Boot Check & Decision 
     */
    boot_mode_t mode = startup_decide_mode();
    if (mode == BOOT_MODE_FACTORY_RESET) {
        ESP_LOGI(TAG, "Factory reset requested; startup handles it and reboots");
        status_set_code(STATUS_ERROR); // Blink error/reset pattern
        vTaskDelay(pdMS_TO_TICKS(500)); // Allow LED to be seen briefly
        return;
    }

    /* * 4. Essential System Services 
     */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_LOGI(TAG, "Network interface initialized");

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_LOGI(TAG, "Event loop initialized");

    /* * 5. System Core Initialization (Config, Buffers, etc.) 
     */
    ESP_LOGI(TAG, "--- System Core Initialization ---");
    ret = sys_setup_all();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System setup failed: %s", esp_err_to_name(ret));
        // Now this call will actually work because LED is initialized!
        status_set_code(STATUS_ERROR); 
        ESP_LOGE(TAG, "Critical error - restarting in 5 seconds...");
        vTaskDelay(pdMS_TO_TICKS(5000));
        esp_restart();
        return;
    }
    ESP_LOGI(TAG, "System setup complete");

    /* * 6. Branch by Boot Mode 
     */
    switch (mode) {
        case BOOT_MODE_NORMAL:
            init_normal_mode();
            break;
        case BOOT_MODE_RESCUE:
            init_rescue_mode();
            break;
        case BOOT_MODE_FACTORY_RESET:
        default:
            ESP_LOGE(TAG, "Unrecognized boot mode: %d", mode);
            status_set_code(STATUS_ERROR);
            break;
    }

    /* * 7. Main loop - lightweight housekeeping 
     */
    ESP_LOGI(TAG, "Entering main loop...");
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000)); // 10s
        ESP_LOGD(TAG, "Heap: free=%lu, min_free=%lu", esp_get_free_heap_size(), esp_get_minimum_free_heap_size());
    }
}