/**
 * @file sys_snapshot.c
 * @brief Snapshot save/restore functionality
 */

#include "sys_mod.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char* TAG = "SYS_SNAP";

/* NVS configuration */
#define NVS_NAMESPACE_SNAPSHOTS "snapshots"

/* Forward declaration */
extern sys_state_t* sys_get_state(void);
extern sys_config_t* sys_get_config_mutable(void);

/* ========== SNAPSHOT OPERATIONS ========== */

esp_err_t sys_snapshot_record(int port_idx) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) {
        ESP_LOGE(TAG, "Invalid port index: %d", port_idx);
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Open snapshots namespace
    ret = nvs_open(NVS_NAMESPACE_SNAPSHOTS, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open snapshots namespace: %d", ret);
        return ret;
    }
    
    // Generate key name
    char key[16];
    snprintf(key, sizeof(key), "snap_port%d", port_idx);
    
    // Get current buffer
    sys_state_t* state = sys_get_state();
    uint8_t* buffer = state->dmx_buffers[port_idx];
    
    if (!buffer) {
        ESP_LOGE(TAG, "Buffer %d not allocated", port_idx);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_STATE;
    }
    
    // Save to NVS
    ret = nvs_set_blob(nvs_handle, key, buffer, DMX_UNIVERSE_SIZE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write snapshot %d: %d", port_idx, ret);
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit snapshot %d: %d", port_idx, ret);
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    
    // Update flag in config
    sys_config_t* cfg = sys_get_config_mutable();
    cfg->failsafe.has_snapshot = true;
    
    ESP_LOGI(TAG, "Snapshot recorded for port %d", port_idx);
    return ESP_OK;
}

esp_err_t sys_snapshot_restore(int port_idx, uint8_t* out_buffer) {
    if (port_idx < 0 || port_idx >= SYS_MAX_PORTS) {
        ESP_LOGE(TAG, "Invalid port index: %d", port_idx);
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!out_buffer) {
        ESP_LOGE(TAG, "NULL output buffer");
        return ESP_ERR_INVALID_ARG;
    }
    
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Open snapshots namespace (read-only)
    ret = nvs_open(NVS_NAMESPACE_SNAPSHOTS, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Snapshots namespace not found");
        return ESP_ERR_NOT_FOUND;
    }
    
    // Generate key name
    char key[16];
    snprintf(key, sizeof(key), "snap_port%d", port_idx);
    
    // Read from NVS
    size_t required_size = DMX_UNIVERSE_SIZE;
    ret = nvs_get_blob(nvs_handle, key, out_buffer, &required_size);
    
    if (ret != ESP_OK || required_size != DMX_UNIVERSE_SIZE) {
        ESP_LOGW(TAG, "Snapshot %d not found or size mismatch", port_idx);
        nvs_close(nvs_handle);
        return ESP_ERR_NOT_FOUND;
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Snapshot restored for port %d", port_idx);
    return ESP_OK;
}
