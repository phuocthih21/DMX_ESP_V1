/**
 * @file mod_web_routes.c
 * @brief URI Route Registration
 * 
 * Registers all HTTP endpoints and WebSocket handlers.
 */

#include "mod_web_routes.h"
#include "mod_web_static.h"
#include "mod_web_api.h"
#include "mod_web_ws.h"
#include "esp_log.h"

static const char *TAG = "MOD_WEB_ROUTES";

/* ========== PUBLIC API ========== */

esp_err_t mod_web_register_routes(httpd_handle_t server)
{
    esp_err_t ret;

    // ========== Static File Handlers ==========
    
    // GET / -> index.html.gz
    httpd_uri_t uri_index = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = mod_web_static_handler_index,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_index);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register / handler");
        return ret;
    }

    // GET /app.js -> app.js.gz
    httpd_uri_t uri_app_js = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = mod_web_static_handler_js,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_app_js);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /app.js handler");
        return ret;
    }

    // GET /style.css -> style.css.gz
    httpd_uri_t uri_style_css = {
        .uri = "/style.css",
        .method = HTTP_GET,
        .handler = mod_web_static_handler_css,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_style_css);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /style.css handler");
        return ret;
    }
    // GET /favicon.ico -> small icon or empty response
    httpd_uri_t uri_favicon = {
        .uri = "/favicon.ico",
        .method = HTTP_GET,
        .handler = mod_web_static_handler_favicon,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_favicon);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /favicon.ico handler");
        return ret;
    }
    // ========== System API Handlers ==========
    // Per MOD_WEB.md: GET /api/sys/info, POST /api/dmx/config, POST /api/net/config
    
    // GET /api/sys/info
    httpd_uri_t uri_sys_info = {
        .uri = "/api/sys/info",
        .method = HTTP_GET,
        .handler = mod_web_api_system_info,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_sys_info);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/sys/info handler");
        return ret;
    }

    // POST /api/auth/login (new)
    httpd_uri_t uri_auth_login = {
        .uri = "/api/auth/login",
        .method = HTTP_POST,
        .handler = mod_web_api_auth_login,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_auth_login);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/auth/login handler");
        return ret;
    }

    // POST /api/auth/set_password (initial setup or change)
    httpd_uri_t uri_auth_setpw = {
        .uri = "/api/auth/set_password",
        .method = HTTP_POST,
        .handler = mod_web_api_auth_set_password,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_auth_setpw);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/auth/set_password handler");
        return ret;
    }

    // POST /api/sys/reboot (additional endpoint)
    httpd_uri_t uri_sys_reboot = {
        .uri = "/api/sys/reboot",
        .method = HTTP_POST,
        .handler = mod_web_api_system_reboot,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_sys_reboot);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/sys/reboot handler");
        return ret;
    }

    // POST /api/sys/factory (additional endpoint)
    httpd_uri_t uri_sys_factory = {
        .uri = "/api/sys/factory",
        .method = HTTP_POST,
        .handler = mod_web_api_system_factory,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_sys_factory);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/sys/factory handler");
        return ret;
    }

    // ========== DMX API Handlers ==========
    
    // POST /api/dmx/config (per MOD_WEB.md)
    httpd_uri_t uri_dmx_config = {
        .uri = "/api/dmx/config",
        .method = HTTP_POST,
        .handler = mod_web_api_dmx_config,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_dmx_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/dmx/config handler");
        return ret;
    }

    // OPTIONS /api/dmx/config (CORS preflight)
    httpd_uri_t uri_dmx_config_opt = {
        .uri = "/api/dmx/config",
        .method = HTTP_OPTIONS,
        .handler = mod_web_api_options,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_dmx_config_opt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPTIONS /api/dmx/config handler");
        return ret;
    }

    // GET /api/dmx/status (additional endpoint for status)
    httpd_uri_t uri_dmx_status = {
        .uri = "/api/dmx/status",
        .method = HTTP_GET,
        .handler = mod_web_api_dmx_status,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_dmx_status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/dmx/status handler");
        return ret;
    }

    // ========== Network API Handlers ==========
    
    // POST /api/net/config (per MOD_WEB.md)
    httpd_uri_t uri_net_config = {
        .uri = "/api/net/config",
        .method = HTTP_POST,
        .handler = mod_web_api_network_config,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_net_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/net/config handler");
        return ret;
    }

    // Also register legacy '/api/network/config' for backward compatibility
    httpd_uri_t uri_network_config = {
        .uri = "/api/network/config",
        .method = HTTP_POST,
        .handler = mod_web_api_network_config,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_network_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/network/config handler");
        return ret;
    }

    // OPTIONS /api/network/config (CORS preflight)
    httpd_uri_t uri_network_config_opt = {
        .uri = "/api/network/config",
        .method = HTTP_OPTIONS,
        .handler = mod_web_api_options,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_network_config_opt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPTIONS /api/network/config handler");
        return ret;
    }

    // Also add OPTIONS for the canonical /api/net/config
    httpd_uri_t uri_net_config_opt = {
        .uri = "/api/net/config",
        .method = HTTP_OPTIONS,
        .handler = mod_web_api_options,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_net_config_opt);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register OPTIONS /api/net/config handler");
        return ret;
    }
    // GET /api/net/status (additional endpoint for status)
    httpd_uri_t uri_net_status = {
        .uri = "/api/net/status",
        .method = HTTP_GET,
        .handler = mod_web_api_network_status,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_net_status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/net/status handler");
        return ret;
    }

    // Also register legacy '/api/network/status'
    httpd_uri_t uri_network_status = {
        .uri = "/api/network/status",
        .method = HTTP_GET,
        .handler = mod_web_api_network_status,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_network_status);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/network/status handler");
        return ret;
    }

    // GET /api/network/status/scan -> Wi-Fi scan (returns empty list until implemented)
    httpd_uri_t uri_network_scan = {
        .uri = "/api/network/status/scan",
        .method = HTTP_GET,
        .handler = mod_web_api_network_scan,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_network_scan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/network/status/scan handler");
        return ret;
    }

    // Alias: GET /api/net/status/scan
    httpd_uri_t uri_net_scan = {
        .uri = "/api/net/status/scan",
        .method = HTTP_GET,
        .handler = mod_web_api_network_scan,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_net_scan);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/net/status/scan handler");
        return ret;
    }

    // GET /api/net/failure - return last recorded network failure (if any)
    httpd_uri_t uri_net_fail = {
        .uri = "/api/net/failure",
        .method = HTTP_GET,
        .handler = mod_web_api_network_failure,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_net_fail);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/net/failure handler");
        return ret;
    }

    // Also register legacy '/api/network/failure'
    httpd_uri_t uri_network_fail = {
        .uri = "/api/network/failure",
        .method = HTTP_GET,
        .handler = mod_web_api_network_failure,
        .user_ctx = NULL
    };
    ret = httpd_register_uri_handler(server, &uri_network_fail);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /api/network/failure handler");
        return ret;
    }

    // OPTIONS handlers already registered next to their corresponding POST endpoints above.

    // ========== WebSocket Handler ==========
    
    // GET /ws/status -> WebSocket upgrade
    httpd_uri_t uri_ws = {
        .uri = "/ws/status",
        .method = HTTP_GET,
        .handler = mod_web_ws_handler,
        .user_ctx = NULL,
        .is_websocket = true
    };
    ret = httpd_register_uri_handler(server, &uri_ws);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to register /ws/status handler");
        return ret;
    }

    ESP_LOGI(TAG, "All routes registered successfully");
    return ESP_OK;
}

