/**
 * @file sys_mod.c
 * @brief Event loop and main dispatcher
 */

#include "sys_mod.h"
#include "esp_log.h"
#include "esp_event.h"

static const char* TAG = "SYS_MOD";

/* Event loop handle */
static esp_event_loop_handle_t s_event_loop = NULL;

/* ========== EVENT SYSTEM ========== */

esp_err_t sys_event_loop_init(void) {
    esp_event_loop_args_t loop_args = {
        .queue_size = 32,
        .task_name = "sys_event",
        .task_priority = 5,
        .task_stack_size = 4096,
        .task_core_id = 0
    };
    
    esp_err_t ret = esp_event_loop_create(&loop_args, &s_event_loop);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create event loop: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "Event loop created");
    return ESP_OK;
}

esp_err_t sys_send_event(sys_event_id_t event_id, void* data, size_t data_size) {
    if (!s_event_loop) {
        ESP_LOGE(TAG, "Event loop not initialized");
        return ESP_ERR_INVALID_STATE;
    }
    
    esp_err_t ret = esp_event_post_to(s_event_loop, 
                                      0,  // base (reserved)
                                      event_id, 
                                      data, 
                                      data_size, 
                                      0); // no wait
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to post event %d: %d", event_id, ret);
    }
    
    return ret;
}
