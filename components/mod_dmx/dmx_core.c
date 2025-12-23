#include "mod_dmx.h"
#include "dmx_types.h"
#include "sys_mod.h"  // For sys_get_dmx_buffer, sys_snapshot_restore, sys_get_config, sys_get_state
#include "esp_log.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

static const char *TAG = "MOD_DMX";


// Forward declarations for backend implementations
esp_err_t dmx_rmt_init(int port_idx, int gpio_tx);
esp_err_t dmx_rmt_send_frame(int port_idx, const uint8_t* data, uint16_t len);

esp_err_t dmx_uart_init_port(int port_idx, uart_port_t uart_num, int tx_pin, int de_pin);
void IRAM_ATTR dmx_uart_send_frame(int port_idx, const uint8_t* data);

// Local port metadata
typedef enum {
    DMX_BACKEND_RMT = 0,
    DMX_BACKEND_UART = 1,
} dmx_backend_t;

typedef struct {
    dmx_backend_t backend;
    bool enabled;
    dmx_timing_t timing; // local cached timing
    bool in_failsafe;
    uint8_t snapshot[DMX_UNIVERSE_SIZE];
} dmx_port_ctx_t;

static dmx_port_ctx_t s_ports[DMX_PORT_COUNT];
static TaskHandle_t s_task = NULL;
static bool s_running = false;

static void IRAM_ATTR dmx_task_main(void *arg)
{
    TickType_t last = xTaskGetTickCount();
    const TickType_t period = pdMS_TO_TICKS(25); // default 40Hz

    ESP_LOGI(TAG, "DMX task started on core %d", xPortGetCoreID());

    while (s_running) {
        int64_t now = esp_timer_get_time();

        const sys_config_t* cfg = sys_get_config();

        for (int i = 0; i < DMX_PORT_COUNT; ++i) {
            if (!s_ports[i].enabled) continue;

            // Update timing from global config (hot-swap)
            dmx_timing_t cfg_t = cfg->ports[i].timing;
            if (memcmp(&cfg_t, &s_ports[i].timing, sizeof(dmx_timing_t)) != 0) {
                s_ports[i].timing = cfg_t;
            }

            // Failsafe handling (simple)
            int64_t last_activity = sys_get_last_activity(i);
            uint32_t timeout_us = (uint32_t)cfg->failsafe.timeout_ms * 1000U;
            const uint8_t *data_ptr = NULL;

            if ((now - last_activity) > (int64_t)timeout_us) {
                if (!s_ports[i].in_failsafe) {
                    ESP_LOGW(TAG, "Port %d entering failsafe", i);
                    s_ports[i].in_failsafe = true;
                }
                switch (cfg->failsafe.mode) {
                    case FAILSAFE_BLACKOUT:
                        memset(s_ports[i].snapshot, 0, DMX_UNIVERSE_SIZE);
                        data_ptr = s_ports[i].snapshot;
                        break;
                    case FAILSAFE_SNAPSHOT:
                        data_ptr = s_ports[i].snapshot;
                        break;
                    case FAILSAFE_HOLD:
                    default:
                        data_ptr = sys_get_dmx_buffer(i);
                        break;
                }
            } else {
                if (s_ports[i].in_failsafe) {
                    ESP_LOGI(TAG, "Port %d back to normal", i);
                    s_ports[i].in_failsafe = false;
                }
                data_ptr = sys_get_dmx_buffer(i);
            }

            // Send frame
            if (s_ports[i].backend == DMX_BACKEND_RMT) {
                dmx_rmt_send_frame(i, data_ptr, DMX_UNIVERSE_SIZE);
            } else {
                dmx_uart_send_frame(i, data_ptr);
            }
        }

        vTaskDelayUntil(&last, period);
    }

    ESP_LOGI(TAG, "DMX task stopping");
    vTaskDelete(NULL);
}

esp_err_t dmx_init(void)
{
    if (s_running || s_task) return ESP_OK;

    // Setup default port mapping: A/B = RMT, C/D = UART
    s_ports[DMX_PORT_A].backend = DMX_BACKEND_RMT;
    s_ports[DMX_PORT_B].backend = DMX_BACKEND_RMT;
    s_ports[DMX_PORT_C].backend = DMX_BACKEND_UART;
    s_ports[DMX_PORT_D].backend = DMX_BACKEND_UART;

    // Enabled flags from sys config
    const sys_config_t* cfg = sys_get_config();
    for (int i = 0; i < DMX_PORT_COUNT; ++i) {
        s_ports[i].enabled = cfg->ports[i].enabled;
        s_ports[i].timing = cfg->ports[i].timing;
        if (cfg->failsafe.has_snapshot) {
            esp_err_t ret = sys_snapshot_restore(i, s_ports[i].snapshot);
            if (ret != ESP_OK) {
                ESP_LOGW(TAG, "Failed to restore snapshot for port %d: %d", i, ret);
                memset(s_ports[i].snapshot, 0, DMX_UNIVERSE_SIZE);
            }
        } else {
            memset(s_ports[i].snapshot, 0, DMX_UNIVERSE_SIZE);
        }
    }

    // Init RMT ports only if enabled and configured for RMT
    esp_err_t ret;
    if (s_ports[DMX_PORT_A].enabled && s_ports[DMX_PORT_A].backend == DMX_BACKEND_RMT) {
        ret = dmx_rmt_init(DMX_PORT_A, GPIO_PORT_A_TX);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "init port A failed: %d", ret); return ret; }
    } else {
        ESP_LOGI(TAG, "Skipping RMT init for Port A (enabled=%d backend=%d)", s_ports[DMX_PORT_A].enabled, s_ports[DMX_PORT_A].backend);
    }

    if (s_ports[DMX_PORT_B].enabled && s_ports[DMX_PORT_B].backend == DMX_BACKEND_RMT) {
        ret = dmx_rmt_init(DMX_PORT_B, GPIO_PORT_B_TX);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "init port B failed: %d", ret); return ret; }
    } else {
        ESP_LOGI(TAG, "Skipping RMT init for Port B (enabled=%d backend=%d)", s_ports[DMX_PORT_B].enabled, s_ports[DMX_PORT_B].backend);
    }

    // Init UART ports only if enabled and configured for UART
    if (s_ports[DMX_PORT_C].enabled && s_ports[DMX_PORT_C].backend == DMX_BACKEND_UART) {
        ret = dmx_uart_init_port(DMX_PORT_C, UART_NUM_1, GPIO_PORT_C_TX, GPIO_PORT_C_DE);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "init port C failed: %d", ret); return ret; }
    } else {
        ESP_LOGI(TAG, "Skipping UART init for Port C (enabled=%d backend=%d)", s_ports[DMX_PORT_C].enabled, s_ports[DMX_PORT_C].backend);
    }

    if (s_ports[DMX_PORT_D].enabled && s_ports[DMX_PORT_D].backend == DMX_BACKEND_UART) {
        ret = dmx_uart_init_port(DMX_PORT_D, UART_NUM_2, GPIO_PORT_D_TX, GPIO_PORT_D_DE);
        if (ret != ESP_OK) { ESP_LOGE(TAG, "init port D failed: %d", ret); return ret; }
    } else {
        ESP_LOGI(TAG, "Skipping UART init for Port D (enabled=%d backend=%d)", s_ports[DMX_PORT_D].enabled, s_ports[DMX_PORT_D].backend);
    }

    ESP_LOGI(TAG, "DMX initialized");
    return ESP_OK;
}

esp_err_t dmx_start(void)
{
    if (s_running) return ESP_OK;

    s_running = true;

    BaseType_t res = xTaskCreatePinnedToCore(
        dmx_task_main,
        "dmx_engine",
        4096,
        NULL,
        configMAX_PRIORITIES - 2,
        &s_task,
        1 // core 1
    );

    if (res != pdPASS) {
        s_running = false;
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "DMX started");
    return ESP_OK;
}

esp_err_t dmx_stop(void)
{
    if (!s_running) return ESP_OK;

    s_running = false;
    // Allow task to exit
    vTaskDelay(pdMS_TO_TICKS(50));
    s_task = NULL;

    ESP_LOGI(TAG, "DMX stopped");
    return ESP_OK;
}

void dmx_apply_new_timing(int port, const dmx_timing_t* timing)
{
    if (port < 0 || port >= DMX_PORT_COUNT || !timing) {
        return;
    }
    
    // Timing is automatically read from g_sys_config each frame in the task loop.
    // This function exists for API Contract compliance and can be used to
    // force immediate update if needed. The actual update happens in the next frame.
    // For now, we just validate - the task loop will pick up the change.
    (void)port;
    (void)timing;
}

esp_err_t dmx_driver_init(const sys_config_t* cfg)
{
    // API Contract compliance: dmx_driver_init(cfg)
    // Actual implementation reads from g_sys_config internally,
    // so cfg parameter is accepted but not used directly.
    (void)cfg;
    return dmx_init();
}