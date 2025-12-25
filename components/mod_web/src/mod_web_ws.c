/**
 * @file mod_web_ws.c
 * @brief WebSocket Handler Implementation
 * 
 * Handles WebSocket connections and broadcasts realtime status updates.
 * Per web_socket_spec_v_1.md: One-way push, stateless, rate-limited.
 */

#include "mod_web_ws.h"
#include "mod_web_json.h"
#include "sys_mod.h"
#include "sys_event.h"
#include "mod_net.h"
#include <string.h>
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "MOD_WEB_WS";

/* ========== CONSTANTS ========== */

#define WS_MAX_CLIENTS 4
#define WS_MAX_MSG_RATE 20  // messages per second (per spec)
#define WS_SYSTEM_STATUS_INTERVAL_MS 1000  // 1 Hz
#define WS_DMX_STATUS_INTERVAL_MS 250      // 4 Hz (2-5 Hz per spec)

/* ========== CLIENT MANAGEMENT ========== */

typedef struct {
    httpd_handle_t hd;
    int fd;
    bool active;
} ws_client_t;

static ws_client_t s_clients[WS_MAX_CLIENTS];
static SemaphoreHandle_t s_clients_mutex = NULL;

/* ========== RATE LIMITING ========== */

static uint32_t s_msg_count = 0;
static int64_t s_last_reset_time = 0;

/* ========== PERIODIC UPDATE TASK ========== */

static TaskHandle_t s_periodic_task_handle = NULL;

/* ========== HELPER FUNCTIONS ========== */

/**
 * @brief Get current timestamp in milliseconds (from boot)
 */
static uint32_t get_timestamp_ms(void)
{
    return (uint32_t)(esp_timer_get_time() / 1000);
}

/**
 * @brief Calculate CPU load percentage from idle task stats
 * 
 * Returns approximate CPU load (0-100) by measuring idle task runtime.
 * Note: This is a simplified implementation using idle task counters.
 */
static uint8_t calculate_cpu_load(void)
{
    static uint32_t last_idle_time = 0;
    static int64_t last_check_time = 0;
    
    int64_t now = esp_timer_get_time();
    uint32_t idle_time = 0;
    
    // Get idle task runtime (simplified approach using task stats if available)
    // For more accurate measurement, would need CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
#if CONFIG_FREERTOS_USE_TRACE_FACILITY && CONFIG_FREERTOS_GENERATE_RUN_TIME_STATS
    TaskStatus_t *task_array;
    UBaseType_t num_tasks;
    uint32_t total_runtime;
    
    num_tasks = uxTaskGetNumberOfTasks();
    task_array = pvPortMalloc(num_tasks * sizeof(TaskStatus_t));
    
    if (task_array != NULL) {
        num_tasks = uxTaskGetSystemState(task_array, num_tasks, &total_runtime);
        
        // Find IDLE tasks and sum their runtime
        for (UBaseType_t i = 0; i < num_tasks; i++) {
            if (strstr(pcTaskGetName(task_array[i].xHandle), "IDLE") != NULL) {
                idle_time += task_array[i].ulRunTimeCounter;
            }
        }
        
        vPortFree(task_array);
        
        // Calculate CPU load percentage
        if (last_check_time > 0) {
            int64_t time_delta = now - last_check_time;
            uint32_t idle_delta = idle_time - last_idle_time;
            
            if (time_delta > 0 && total_runtime > 0) {
                uint8_t cpu_load = 100 - ((idle_delta * 100) / total_runtime);
                last_idle_time = idle_time;
                last_check_time = now;
                return (cpu_load > 100) ? 100 : cpu_load;
            }
        }
        
        last_idle_time = idle_time;
        last_check_time = now;
    }
#endif
    
    // Fallback: estimate based on free heap (very rough approximation)
    uint32_t free_heap = esp_get_free_heap_size();
    uint32_t min_free = esp_get_minimum_free_heap_size();
    
    if (free_heap > min_free && min_free > 0) {
        return (uint8_t)((100 * (free_heap - min_free)) / free_heap);
    }
    
    return 0; // Unknown
}

/**
 * @brief Calculate FPS for a DMX port based on activity timestamps
 * 
 * Tracks frame intervals and calculates actual FPS over a short window.
 */
static uint16_t calculate_port_fps(int port_idx)
{
    static int64_t last_activity[4] = {0};
    static uint16_t frame_count[4] = {0};
    static int64_t window_start[4] = {0};
    
    if (port_idx < 0 || port_idx >= 4) {
        return 0;
    }
    
    int64_t current_activity = sys_get_last_activity(port_idx);
    int64_t now = esp_timer_get_time();
    
    // If no activity or port disabled, return 0
    if (current_activity == 0) {
        frame_count[port_idx] = 0;
        window_start[port_idx] = 0;
        return 0;
    }
    
    // Check if there's been activity since last check
    if (current_activity != last_activity[port_idx]) {
        frame_count[port_idx]++;
        last_activity[port_idx] = current_activity;
        
        // Initialize window on first frame
        if (window_start[port_idx] == 0) {
            window_start[port_idx] = now;
        }
    }
    
    // Calculate FPS over a 1-second window
    int64_t window_duration = now - window_start[port_idx];
    if (window_duration >= 1000000) { // 1 second
        uint16_t fps = frame_count[port_idx];
        
        // Reset for next window
        frame_count[port_idx] = 0;
        window_start[port_idx] = now;
        
        return fps;
    }
    
    // If within window and no recent activity (> 100ms ago), likely 0 FPS
    if ((now - current_activity) > 100000) {
        return 0;
    }
    
    // Return estimate based on current count and elapsed time
    if (window_duration > 0 && frame_count[port_idx] > 0) {
        return (uint16_t)((frame_count[port_idx] * 1000000ULL) / window_duration);
    }
    
    return 0;
}

/**
 * @brief Check and reset rate limit counter
 * 
 * Per spec: Max 20 msg/sec
 */
static bool check_rate_limit(void)
{
    int64_t now = esp_timer_get_time();
    
    // Reset counter every second
    if (now - s_last_reset_time >= 1000000) {
        s_msg_count = 0;
        s_last_reset_time = now;
    }
    
    if (s_msg_count >= WS_MAX_MSG_RATE) {
        return false; // Rate limit exceeded
    }
    
    s_msg_count++;
    return true;
}

/**
 * @brief Add client to active list
 */
static int ws_add_client(httpd_handle_t hd, int fd)
{
    if (s_clients_mutex == NULL) {
        return -1;
    }
    
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);
    
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (!s_clients[i].active) {
            s_clients[i].hd = hd;
            s_clients[i].fd = fd;
            s_clients[i].active = true;
            xSemaphoreGive(s_clients_mutex);
            ESP_LOGI(TAG, "Client %d connected (fd=%d)", i, fd);
            return i;
        }
    }
    
    xSemaphoreGive(s_clients_mutex);
    ESP_LOGW(TAG, "Max clients reached");
    return -1;
}

/**
 * @brief Remove client from active list
 */
static void ws_remove_client(int fd)
{
    if (s_clients_mutex == NULL) {
        return;
    }
    
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);
    
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active && s_clients[i].fd == fd) {
            s_clients[i].active = false;
            ESP_LOGI(TAG, "Client %d disconnected (fd=%d)", i, fd);
            break;
        }
    }
    
    xSemaphoreGive(s_clients_mutex);
}

/**
 * @brief Broadcast message to all active clients
 * 
 * Per spec: Non-blocking, drop if queue full
 */
static void ws_broadcast_message(const char *message, size_t len)
{
    if (s_clients_mutex == NULL || message == NULL || len == 0) {
        return;
    }
    
    // Check rate limit
    if (!check_rate_limit()) {
        ESP_LOGW(TAG, "Rate limit exceeded, dropping message");
        return;
    }
    
    xSemaphoreTake(s_clients_mutex, portMAX_DELAY);
    
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
        if (s_clients[i].active) {
            // Send frame asynchronously (non-blocking)
            httpd_ws_frame_t ws_pkt = {
                .final = true,
                .fragmented = false,
                .type = HTTPD_WS_TYPE_TEXT,
                .payload = (uint8_t *)message,
                .len = len
            };
            
            esp_err_t ret = httpd_ws_send_frame_async(s_clients[i].hd, s_clients[i].fd, &ws_pkt);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to send to client %d: %s", i, esp_err_to_name(ret));
                // Mark as inactive if send fails
                s_clients[i].active = false;
            }
        }
    }
    
    xSemaphoreGive(s_clients_mutex);
}

/**
 * @brief Create WebSocket message envelope
 * 
 * Per spec: {"type": "...", "ts": ..., "data": {...}}
 */
static char *ws_create_envelope(const char *type, cJSON *data)
{
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", type);
    cJSON_AddNumberToObject(root, "ts", get_timestamp_ms());
    cJSON_AddItemToObject(root, "data", data);
    
    char *json_str = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    
    return json_str;
}

/* ========== MESSAGE BUILDERS ========== */

/**
 * @brief Build system.status message
 * 
 * Per spec: 1 Hz periodic
 */
static void ws_send_system_status(void)
{
    uint32_t uptime_sec = (uint32_t)(esp_timer_get_time() / 1000000);
    uint32_t free_heap = esp_get_free_heap_size();
    uint8_t cpu_load = calculate_cpu_load();
    
    // Build data object
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "cpu", cpu_load);
    cJSON_AddNumberToObject(data, "heap", free_heap);
    cJSON_AddNumberToObject(data, "uptime", uptime_sec);
    
    // Create envelope and broadcast
    char *json_str = ws_create_envelope("system.status", data);
    if (json_str) {
        ws_broadcast_message(json_str, strlen(json_str));
        free(json_str);
    } else {
        cJSON_Delete(data);
    }
}

/**
 * @brief Build dmx.port_status message
 * 
 * Per spec: 2-5 Hz per port
 */
static void ws_send_dmx_port_status(int port_idx)
{
    const sys_config_t *cfg = sys_get_config();
    if (cfg == NULL || port_idx < 0 || port_idx >= 4) {
        return;
    }
    
    // Copy to local variable to avoid packed member address warning
    dmx_port_cfg_t port_cfg = cfg->ports[port_idx];
    
    // Calculate actual FPS from activity tracking
    uint16_t fps = calculate_port_fps(port_idx);
    
    // Build data object
    cJSON *data = cJSON_CreateObject();
    cJSON_AddNumberToObject(data, "port", port_idx);
    cJSON_AddNumberToObject(data, "universe", port_cfg.universe);
    cJSON_AddBoolToObject(data, "enabled", port_cfg.enabled);
    cJSON_AddNumberToObject(data, "fps", fps);
    
    // Create envelope and broadcast
    char *json_str = ws_create_envelope("dmx.port_status", data);
    if (json_str) {
        ws_broadcast_message(json_str, strlen(json_str));
        free(json_str);
    } else {
        cJSON_Delete(data);
    }
}

/**
 * @brief Build network.link message
 * 
 * Per spec: Event-based
 */
static void ws_send_network_link(const char *iface, const char *status)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "iface", iface);
    cJSON_AddStringToObject(data, "status", status);
    
    char *json_str = ws_create_envelope("network.link", data);
    if (json_str) {
        ws_broadcast_message(json_str, strlen(json_str));
        free(json_str);
    } else {
        cJSON_Delete(data);
    }
}

/**
 * @brief Build system.event message
 * 
 * Per spec: Event-based
 */
static void ws_send_system_event(const char *code, const char *level)
{
    cJSON *data = cJSON_CreateObject();
    cJSON_AddStringToObject(data, "code", code);
    cJSON_AddStringToObject(data, "level", level);
    
    char *json_str = ws_create_envelope("system.event", data);
    if (json_str) {
        ws_broadcast_message(json_str, strlen(json_str));
        free(json_str);
    } else {
        cJSON_Delete(data);
    }
}

/* ========== EVENT HANDLER ========== */

/**
 * @brief SYS_MOD event callback
 * 
 * Maps SYS_MOD events to WebSocket messages per spec
 */
static void ws_sys_event_handler(const sys_evt_msg_t *evt, void *user_ctx)
{
    if (evt == NULL) {
        return;
    }
    
    switch (evt->type) {
        case SYS_EVT_CONFIG_APPLIED:
            ws_send_system_event("CONFIG_APPLIED", "info");
            break;
            
        case SYS_EVT_LINK_UP:
            ws_send_network_link("eth", "up");
            break;
            
        case SYS_EVT_LINK_DOWN:
            ws_send_network_link("eth", "down");
            break;
            
        case SYS_EVT_ERROR:
            ws_send_system_event("ERROR", "error");
            break;
            
        default:
            break;
    }
}

/* ========== PERIODIC TASK ========== */

/**
 * @brief Periodic update task
 * 
 * Sends system.status (1Hz) and dmx.port_status (4Hz)
 */
static void ws_periodic_task(void *pvParameters)
{
    TickType_t last_system_update = 0;
    TickType_t last_dmx_update = 0;
    
    while (1) {
        TickType_t now = xTaskGetTickCount();
        
        // System status: 1 Hz
        if (now - last_system_update >= pdMS_TO_TICKS(WS_SYSTEM_STATUS_INTERVAL_MS)) {
            ws_send_system_status();
            last_system_update = now;
        }
        
        // DMX port status: 4 Hz (2-5 Hz per spec)
        if (now - last_dmx_update >= pdMS_TO_TICKS(WS_DMX_STATUS_INTERVAL_MS)) {
            for (int i = 0; i < 4; i++) {
                ws_send_dmx_port_status(i);
            }
            last_dmx_update = now;
        }
        
        vTaskDelay(pdMS_TO_TICKS(100)); // 10 Hz loop
    }
}

/* ========== WEBSOCKET HANDLER ========== */

esp_err_t mod_web_ws_handler(httpd_req_t *req)
{
    if (req->method == HTTP_GET) {
        ESP_LOGI(TAG, "WebSocket connection request");
        
        // WebSocket upgrade is handled automatically by httpd when is_websocket=true
        // The handler is called after upgrade, so we can directly add the client
        int fd = httpd_req_to_sockfd(req);
        int client_idx = ws_add_client(req->handle, fd);
        
        if (client_idx >= 0) {
            ESP_LOGI(TAG, "WebSocket client %d connected", client_idx);
            return ESP_OK;
        } else {
            ESP_LOGE(TAG, "Failed to add client");
            return ESP_FAIL;
        }
    }
    
    // Handle WebSocket frames
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to receive frame: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Per spec: One-way push, client should not send messages
    // But we handle it gracefully
    if (ws_pkt.len > 0) {
        ESP_LOGD(TAG, "Received %d bytes from client (ignored per spec)", ws_pkt.len);
    }
    
    // Check if client disconnected
    if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) {
        int fd = httpd_req_to_sockfd(req);
        ws_remove_client(fd);
    }
    
    return ESP_OK;
}

/* ========== INITIALIZATION ========== */

/**
 * @brief Initialize WebSocket module
 */
esp_err_t mod_web_ws_init(void)
{
    // Create mutex for client list
    s_clients_mutex = xSemaphoreCreateMutex();
    if (s_clients_mutex == NULL) {
        ESP_LOGE(TAG, "Failed to create mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize client list
    memset(s_clients, 0, sizeof(s_clients));
    
    // Register SYS_MOD event callback
    if (sys_event_register_cb(ws_sys_event_handler, NULL) != 0) {
        ESP_LOGE(TAG, "Failed to register SYS_MOD event callback");
        vSemaphoreDelete(s_clients_mutex);
        s_clients_mutex = NULL;
        return ESP_FAIL;
    }
    
    // Create periodic update task
    BaseType_t ret = xTaskCreatePinnedToCore(
        ws_periodic_task,
        "ws_periodic",
        4096,  // Stack size
        NULL,
        5,     // Priority (low, below DMX)
        &s_periodic_task_handle,
        0      // Core 0
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create periodic task");
        sys_event_unregister_cb(ws_sys_event_handler, NULL);
        vSemaphoreDelete(s_clients_mutex);
        s_clients_mutex = NULL;
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "WebSocket module initialized");
    return ESP_OK;
}

/**
 * @brief Deinitialize WebSocket module
 */
void mod_web_ws_deinit(void)
{
    // Stop periodic task
    if (s_periodic_task_handle != NULL) {
        vTaskDelete(s_periodic_task_handle);
        s_periodic_task_handle = NULL;
    }
    
    // Unregister event callback
    sys_event_unregister_cb(ws_sys_event_handler, NULL);
    
    // Cleanup mutex
    if (s_clients_mutex != NULL) {
        vSemaphoreDelete(s_clients_mutex);
        s_clients_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "WebSocket module deinitialized");
}
