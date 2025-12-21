/**
 * @file sys_nvs.c
 * @brief NVS persistence operations for configuration
 */

#include "sys_mod.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h>
#include <inttypes.h>

static const char* TAG = "SYS_NVS";

/* NVS configuration */
#define NVS_NAMESPACE "sys_cfg"
#define NVS_KEY_CONFIG "config"

/* Forward declarations */
extern sys_config_t* sys_get_config_mutable(void);
extern const sys_config_t* sys_get_default_config(void);
uint32_t sys_calculate_config_crc(const sys_config_t* cfg);

/* ========== NVS OPERATIONS ========== */

esp_err_t sys_load_config_from_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Open NVS namespace
    ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "NVS namespace not found, will use defaults");
        return ESP_ERR_NVS_NOT_FOUND;
    }
    
    // Read blob
    sys_config_t temp_config;
    size_t required_size = sizeof(sys_config_t);
    ret = nvs_get_blob(nvs_handle, NVS_KEY_CONFIG, &temp_config, &required_size);
    
    if (ret != ESP_OK || required_size != sizeof(sys_config_t)) {
        ESP_LOGW(TAG, "NVS read failed or size mismatch: %d (expected %d, got %d)", 
                 ret, sizeof(sys_config_t), required_size);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_SIZE;
    }
    
    // Validate magic number
    if (temp_config.magic_number != SYS_CONFIG_MAGIC) {
        ESP_LOGE(TAG, "Magic number mismatch: 0x%08" PRIx32 " (expected 0x%08" PRIx32 ")", 
                 temp_config.magic_number, (uint32_t)SYS_CONFIG_MAGIC);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_CRC;
    }
    
    // Validate CRC32
    uint32_t calculated_crc = sys_calculate_config_crc(&temp_config);
    if (calculated_crc != temp_config.crc32) {
        ESP_LOGE(TAG, "CRC mismatch: 0x%08" PRIx32 " (expected 0x%08" PRIx32 ")", 
                 calculated_crc, temp_config.crc32);
        nvs_close(nvs_handle);
        return ESP_ERR_INVALID_CRC;
    }
    
    // Copy to global config
    sys_config_t* cfg = sys_get_config_mutable();
    memcpy(cfg, &temp_config, sizeof(sys_config_t));
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Config loaded from NVS (Device: %s)", cfg->device_label);
    return ESP_OK;
}

esp_err_t sys_save_config_to_nvs(void) {
    nvs_handle_t nvs_handle;
    esp_err_t ret;
    
    // Get mutable config
    sys_config_t* cfg = sys_get_config_mutable();
    
    // Calculate and update CRC
    cfg->crc32 = sys_calculate_config_crc(cfg);
    
    // Open NVS namespace (read-write)
    ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %d", ret);
        return ret;
    }
    
    // Write blob
    ret = nvs_set_blob(nvs_handle, NVS_KEY_CONFIG, cfg, sizeof(sys_config_t));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write config: %d", ret);
        nvs_close(nvs_handle);
        return ret;
    }
    
    // Commit changes
    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %d", ret);
        nvs_close(nvs_handle);
        return ret;
    }
    
    nvs_close(nvs_handle);
    
    ESP_LOGI(TAG, "Config saved to NVS (CRC: 0x%08" PRIx32 ")", cfg->crc32);
    return ESP_OK;
}

esp_err_t sys_factory_reset(void) {
    ESP_LOGW(TAG, "Factory reset triggered");
    
    // Erase NVS namespace
    esp_err_t ret = nvs_flash_erase_partition("nvs");
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase NVS: %d", ret);
        return ret;
    }
    
    // Reinitialize NVS
    ret = nvs_flash_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinit NVS: %d", ret);
        return ret;
    }
    
    // Load default config
    sys_config_t* cfg = sys_get_config_mutable();
    const sys_config_t* defaults = sys_get_default_config();
    memcpy(cfg, defaults, sizeof(sys_config_t));
    
    // Save defaults to NVS
    ret = sys_save_config_to_nvs();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to save defaults: %d", ret);
        return ret;
    }
    
    ESP_LOGI(TAG, "Factory reset complete");
    return ESP_OK;
}
