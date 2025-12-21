/**
 * @file crash_monitor.c
 * @brief Boot loop protection via NVS crash counter
 * 
 * Manages a persistent crash counter in NVS to detect and recover from
 * boot loops. The counter is incremented on every boot and reset to 0
 * after stable operation.
 */

#include "startup_types.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <time.h>

static const char* TAG = "CRASH_MON";

/* ========== NVS CRASH COUNTER MANAGEMENT ========== */

/**
 * @brief Read crash counter from NVS
 * @return Counter value, or 0 if not found/invalid
 */
static uint8_t boot_protect_read_counter(void)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open("boot_cfg", NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "boot_cfg namespace not found (first boot)");
        return 0;
    }

    boot_protect_data_t data;
    size_t size = sizeof(data);
    ret = nvs_get_blob(nvs, "crash_cnt", &data, &size);
    nvs_close(nvs);

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "crash_cnt key not found");
        return 0;
    }

    // Validate magic number
    if (data.magic != BOOT_PROTECT_MAGIC) {
        ESP_LOGW(TAG, "Invalid magic in crash_cnt data (corrupted)");
        return 0;
    }

    ESP_LOGI(TAG, "Read crash counter: %d", data.crash_counter);
    return data.crash_counter;
}

/**
 * @brief Write crash counter to NVS
 * @param counter Counter value to write
 */
static void boot_protect_write_counter(uint8_t counter)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open("boot_cfg", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open boot_cfg namespace: %s", esp_err_to_name(ret));
        return;
    }

    boot_protect_data_t data = {
        .crash_counter = counter,
        .last_boot_time = (uint32_t)time(NULL),
        .magic = BOOT_PROTECT_MAGIC
    };

    ret = nvs_set_blob(nvs, "crash_cnt", &data, sizeof(data));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write crash_cnt: %s", esp_err_to_name(ret));
        nvs_close(nvs);
        return;
    }

    ret = nvs_commit(nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit crash_cnt: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs);
    ESP_LOGD(TAG, "Wrote crash counter: %d", counter);
}

/**
 * @brief Increment crash counter (called at boot)
 * @return New counter value
 */
uint8_t boot_protect_increment(void)
{
    uint8_t counter = boot_protect_read_counter();
    counter++;
    boot_protect_write_counter(counter);
    
    if (counter >= BOOT_CRASH_THRESHOLD) {
        ESP_LOGW(TAG, "Crash counter at threshold: %d >= %d", 
                 counter, BOOT_CRASH_THRESHOLD);
    } else {
        ESP_LOGI(TAG, "Crash counter incremented to %d", counter);
    }
    
    return counter;
}

/**
 * @brief Reset crash counter to 0 (system is stable)
 */
void boot_protect_reset(void)
{
    boot_protect_write_counter(0);
    ESP_LOGI(TAG, "System marked as stable, crash counter reset");
}

/* ========== STABILITY TIMER ========== */

static esp_timer_handle_t s_stable_timer = NULL;
static bool s_stable_timer_running = false;

/**
 * @brief Timer callback - mark system as stable after 60s
 */
static void stable_timer_callback(void* arg)
{
    boot_protect_reset();
    s_stable_timer_running = false;
}

/**
 * @brief Start 60-second stability timer
 */
void boot_protect_start_stable_timer(void)
{
    if (s_stable_timer != NULL) {
        ESP_LOGW(TAG, "Stable timer already created");
        return;
    }

    const esp_timer_create_args_t timer_args = {
        .callback = &stable_timer_callback,
        .name = "boot_stable"
    };

    esp_err_t ret = esp_timer_create(&timer_args, &s_stable_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create stable timer: %s", esp_err_to_name(ret));
        return;
    }

    ret = esp_timer_start_once(s_stable_timer, BOOT_STABLE_TIME_MS * 1000);  // Convert to us
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start stable timer: %s", esp_err_to_name(ret));
        return;
    }

    s_stable_timer_running = true;
    ESP_LOGI(TAG, "Stable timer started (%d seconds)", BOOT_STABLE_TIME_MS / 1000);
}
