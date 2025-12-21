/**
 * @file sys_setup.c
 * @brief System initialization orchestration
 */

#include "sys_setup.h"
#include "sys_mod.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include <string.h>

static const char* TAG = "SYS_SETUP";

/* Forward declarations */
extern esp_err_t sys_load_config_from_nvs(void);
extern esp_err_t sys_save_config_to_nvs(void);
extern esp_err_t sys_buffer_init(void);
extern esp_err_t sys_event_loop_init(void);
extern sys_state_t* sys_get_state(void);
extern sys_config_t* sys_get_config_mutable(void);
extern const sys_config_t* sys_get_default_config(void);
extern void sys_lazy_save_callback(void* arg);

/* ========== INITIALIZATION ========== */

esp_err_t sys_mod_init(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "=== SYS_MOD Initialization ===");
    
    // Step 1: Initialize NVS Flash
    ESP_LOGI(TAG, "Step 1: Initializing NVS Flash");
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition corrupted, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "  ✓ NVS initialized");
    
    // Step 2: Create Mutex
    ESP_LOGI(TAG, "Step 2: Creating mutexes");
    sys_state_t* state = sys_get_state();
    state->config_mutex = (void*)xSemaphoreCreateMutex();
    if (!state->config_mutex) {
        ESP_LOGE(TAG, "Failed to create config mutex");
        return ESP_ERR_NO_MEM;
    }
    ESP_LOGI(TAG, "  ✓ Mutex created");
    
    // Step 3: Initialize event loop
    ESP_LOGI(TAG, "Step 3: Creating event loop");
    ret = sys_event_loop_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Event loop init failed: %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "  ✓ Event loop created");
    
    // Step 4: Load configuration
    ESP_LOGI(TAG, "Step 4: Loading configuration");
    ret = sys_load_config_from_nvs();
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS empty or corrupt, loading defaults");
        
        // Load default config
        sys_config_t* cfg = sys_get_config_mutable();
        const sys_config_t* defaults = sys_get_default_config();
        memcpy(cfg, defaults, sizeof(sys_config_t));
        
        // Save defaults to NVS
        ret = sys_save_config_to_nvs();
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to save defaults: %d", ret);
        } else {
            ESP_LOGI(TAG, "  ✓ Default config saved");
        }
    } else {
        ESP_LOGI(TAG, "  ✓ Config loaded from NVS");
    }
    
    // Step 5: Allocate DMX buffers
    ESP_LOGI(TAG, "Step 5: Allocating DMX buffers");
    ret = sys_buffer_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Buffer allocation failed: %d", ret);
        return ret;
    }
    ESP_LOGI(TAG, "  ✓ All buffers allocated");
    
    // Step 6: Create lazy save timer
    ESP_LOGI(TAG, "Step 6: Creating lazy save timer");
    const esp_timer_create_args_t timer_args = {
        .callback = &sys_lazy_save_callback,
        .name = "sys_lazy_save",
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .skip_unhandled_events = true
    };
    
    esp_timer_handle_t timer;
    ret = esp_timer_create(&timer_args, &timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create timer: %d", ret);
        return ret;
    }
    state->save_timer = (void*)timer;
    ESP_LOGI(TAG, "  ✓ Timer created");
    
    // Initialize state flags
    state->config_dirty = false;
    state->last_change_time = 0;
    state->ota_in_progress = false;
    state->ota_partition = NULL;
    state->ota_handle = 0;
    
    // Print final status
    const sys_config_t* cfg = sys_get_config();
    ESP_LOGI(TAG, "=== SYS_MOD Ready ===");
    ESP_LOGI(TAG, "Device: %s", cfg->device_label);
    ESP_LOGI(TAG, "Hostname: %s", cfg->net.hostname);
    ESP_LOGI(TAG, "Active ports:");
    for (int i = 0; i < SYS_MAX_PORTS; i++) {
        if (cfg->ports[i].enabled) {
            ESP_LOGI(TAG, "  Port %d: %s Universe %d (Break=%dus MAB=%dus %dHz)",
                     i,
                     cfg->ports[i].protocol == PROTOCOL_ARTNET ? "Art-Net" : "sACN",
                     cfg->ports[i].universe,
                     cfg->ports[i].timing.break_us,
                     cfg->ports[i].timing.mab_us,
                     cfg->ports[i].timing.refresh_rate);
        }
    }
    
    return ESP_OK;
}

esp_err_t sys_setup_all(void) {
    ESP_LOGI(TAG, "Starting system setup...");
    
    esp_err_t ret = sys_mod_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "System setup complete!");
    return ESP_OK;
}
