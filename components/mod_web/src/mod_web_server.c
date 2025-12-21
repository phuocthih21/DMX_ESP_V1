/**
 * @file mod_web_server.c
 * @brief HTTP Server Setup and Configuration
 * 
 * Configures esp_http_server and registers all URI handlers.
 */

#include "mod_web_server.h"
#include "mod_web_routes.h"
#include "mod_web_auth.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"

static const char *TAG = "MOD_WEB_SERVER";

/* ========== SERVER STATE ========== */

static httpd_handle_t s_server_handle = NULL;

/* ========== SERVER CONFIGURATION ========== */

/**
 * @brief Configure HTTP server
 * 
 * Sets up server with appropriate stack size and limits.
 * Runs on Core 0, low priority.
 */
static httpd_config_t get_server_config(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    
    // Stack size: 4096 bytes (WEB_STACK_SIZE per MOD_WEB.md)
    config.stack_size = 4096;
    
    // Max clients: 4 (WEB_MAX_CLIENTS per MOD_WEB.md)
    config.max_open_sockets = 4;
    
    // Enable LRU purge for memory management
    config.lru_purge_enable = true;
    
    // Set core affinity (Core 0 for web tasks, Low Priority per ARCHITECTURE.md)
    config.core_id = 0;
    
    // Increase URI handlers limit
    config.max_uri_handlers = 16;
    
    return config;
}

/* ========== PUBLIC API ========== */

esp_err_t mod_web_server_start(void)
{
    if (s_server_handle != NULL) {
        ESP_LOGW(TAG, "HTTP server already running");
        return ESP_OK;
    }

    // Get server configuration
    httpd_config_t config = get_server_config();

    // Start HTTP server
    esp_err_t ret = httpd_start(&s_server_handle, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "HTTP server started on port %d", config.server_port);

    // Initialize auth subsystem
    mod_web_auth_init();

    // Register all URI handlers
    ret = mod_web_register_routes(s_server_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register routes: %s", esp_err_to_name(ret));
        httpd_stop(s_server_handle);
        s_server_handle = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "All routes registered successfully");
    return ESP_OK;
}

esp_err_t mod_web_server_stop(void)
{
    if (s_server_handle == NULL) {
        ESP_LOGW(TAG, "HTTP server not running");
        return ESP_OK;
    }

    esp_err_t ret = httpd_stop(s_server_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop HTTP server: %s", esp_err_to_name(ret));
        return ret;
    }

    s_server_handle = NULL;
    ESP_LOGI(TAG, "HTTP server stopped");

    return ESP_OK;
}

