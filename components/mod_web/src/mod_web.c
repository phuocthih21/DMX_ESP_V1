/**
 * @file mod_web.c
 * @brief MOD_WEB Module Implementation
 * 
 * Main module initialization and lifecycle management.
 */

#include "mod_web.h"
#include "mod_web_server.h"
#include "mod_web_ws.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "MOD_WEB";

/* ========== MODULE STATE ========== */

static bool s_web_initialized = false;

/* ========== PUBLIC API ========== */

esp_err_t web_init(void)
{
    if (s_web_initialized) {
        ESP_LOGW(TAG, "MOD_WEB already initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Initializing MOD_WEB...");

    // Initialize WebSocket module first (for event registration)
    esp_err_t ret = mod_web_ws_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize WebSocket: %s", esp_err_to_name(ret));
        return ret;
    }

    // Initialize HTTP server
    ret = mod_web_server_start();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        mod_web_ws_deinit();
        return ret;
    }

    s_web_initialized = true;
    ESP_LOGI(TAG, "MOD_WEB initialized successfully");

    return ESP_OK;
}

esp_err_t web_stop(void)
{
    if (!s_web_initialized) {
        ESP_LOGW(TAG, "MOD_WEB not initialized");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Stopping MOD_WEB...");

    // Stop HTTP server
    esp_err_t ret = mod_web_server_stop();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    // Deinitialize WebSocket
    mod_web_ws_deinit();

    s_web_initialized = false;
    ESP_LOGI(TAG, "MOD_WEB stopped");

    return ESP_OK;
}

