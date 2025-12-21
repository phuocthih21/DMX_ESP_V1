/**
 * @file sys_buffer.c
 * @brief DMX buffer management and activity tracking
 */

#include "sys_mod.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char* TAG = "SYS_BUF";

/* Forward declaration */
extern sys_state_t* sys_get_state(void);

/* ========== BUFFER INITIALIZATION ========== */

esp_err_t sys_buffer_init(void) {
    sys_state_t* state = sys_get_state();
    
    ESP_LOGI(TAG, "Allocating DMX buffers...");
    
    for (int i = 0; i < SYS_MAX_PORTS; i++) {
        // Allocate DMA-capable memory
        state->dmx_buffers[i] = heap_caps_malloc(DMX_UNIVERSE_SIZE, MALLOC_CAP_DMA);
        
        if (!state->dmx_buffers[i]) {
            ESP_LOGE(TAG, "Failed to allocate buffer for port %d", i);
            
            // Cleanup already allocated buffers
            for (int j = 0; j < i; j++) {
                free(state->dmx_buffers[j]);
                state->dmx_buffers[j] = NULL;
            }
            
            return ESP_ERR_NO_MEM;
        }
        
        // Initialize to zeros
        memset(state->dmx_buffers[i], 0, DMX_UNIVERSE_SIZE);
        
        // Initialize activity timestamp
        state->last_activity[i] = 0;
        
        ESP_LOGI(TAG, "Buffer %d allocated at %p", i, state->dmx_buffers[i]);
    }
    
    ESP_LOGI(TAG, "All DMX buffers allocated successfully");
    return ESP_OK;
}

/* ========== BUFFER ACCESS ========== */

uint8_t* sys_get_dmx_buffer(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) {
        ESP_LOGE(TAG, "Invalid port index: %d", port_idx);
        return NULL;
    }
    
    sys_state_t* state = sys_get_state();
    return state->dmx_buffers[port_idx];
}

/* ========== ACTIVITY TRACKING ========== */

void sys_notify_activity(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) {
        return; // Silent fail for performance
    }
    
    sys_state_t* state = sys_get_state();
    state->last_activity[port_idx] = esp_timer_get_time();
}

int64_t sys_get_last_activity(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) {
        return 0;
    }
    
    sys_state_t* state = sys_get_state();
    return state->last_activity[port_idx];
}
