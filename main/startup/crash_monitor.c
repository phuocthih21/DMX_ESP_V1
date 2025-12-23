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
#include "esp_ota_ops.h"
#include "esp_app_format.h"
#include <time.h>
#include <string.h>

static const char* TAG = "CRASH_MON";

/* ========== NVS CRASH COUNTER MANAGEMENT ========== */

/**
 * @brief Helper - get SHA256 of currently running image (32 bytes)
 */
static esp_err_t get_running_image_sha(uint8_t out_sha[32])
{
    const esp_partition_t* part = esp_ota_get_running_partition();
    if (part == NULL) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_app_desc_t desc;
    esp_err_t ret = esp_ota_get_partition_description(part, &desc);
    if (ret != ESP_OK) {
        return ret;
    }

    memcpy(out_sha, desc.app_elf_sha256, 32);
    return ESP_OK;
}

/**
 * @brief Read crash protection blob from NVS
 * @param out_data Destination struct to fill (zeroed if missing/corrupt)
 * @return ESP_OK if valid data read, else ESP_FAIL
 */
static esp_err_t boot_protect_read_blob(boot_protect_data_t *out_data)
{
    nvs_handle_t nvs;
    esp_err_t ret;

    memset(out_data, 0, sizeof(*out_data));

    ret = nvs_open("boot_cfg", NVS_READONLY, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "boot_cfg namespace not found (first boot)");
        return ESP_FAIL;
    }

    size_t size = sizeof(*out_data);
    ret = nvs_get_blob(nvs, "crash_cnt", out_data, &size);
    nvs_close(nvs);

    if (ret != ESP_OK) {
        ESP_LOGD(TAG, "crash_cnt key not found");
        return ESP_FAIL;
    }

    if (out_data->magic != BOOT_PROTECT_MAGIC) {
        ESP_LOGW(TAG, "Invalid magic in crash_cnt data (corrupted)");
        return ESP_FAIL;
    }

    if (size < sizeof(*out_data)) {
        ESP_LOGW(TAG, "Old crash_cnt blob (size %d), treating as fresh", (int)size);
        /* Keep crash_counter as read (if any), but clear image_sha for compatibility */
        memset(out_data->image_sha, 0, sizeof(out_data->image_sha));
    }

    ESP_LOGI(TAG, "Read crash counter: %d", out_data->crash_counter);
    return ESP_OK;
}

/**
 * @brief Write crash protection blob to NVS
 * @param counter Counter value to write
 * @param image_sha 32-byte SHA of running image (may be NULL to write zeros)
 */
static void boot_protect_write_blob(uint8_t counter, const uint8_t image_sha[32])
{
    nvs_handle_t nvs;
    esp_err_t ret;

    ret = nvs_open("boot_cfg", NVS_READWRITE, &nvs);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open boot_cfg namespace: %s", esp_err_to_name(ret));
        return;
    }

    boot_protect_data_t data;
    memset(&data, 0, sizeof(data));
    data.crash_counter = counter;
    data.last_boot_time = (uint32_t)time(NULL);
    if (image_sha != NULL) {
        memcpy(data.image_sha, image_sha, sizeof(data.image_sha));
    }
    data.magic = BOOT_PROTECT_MAGIC;

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
 *
 * If the running image differs from the stored image SHA (i.e. new flash),
 * reset the counter to 0 and update the stored image SHA.
 */
uint8_t boot_protect_increment(void)
{
    boot_protect_data_t stored;
    bool have_stored = (boot_protect_read_blob(&stored) == ESP_OK);

    uint8_t curr_sha[32];
    esp_err_t sha_ret = get_running_image_sha(curr_sha);
    if (sha_ret == ESP_OK && have_stored) {
        if (memcmp(curr_sha, stored.image_sha, sizeof(curr_sha)) != 0) {
            ESP_LOGI(TAG, "New firmware image detected - resetting crash counter");
            boot_protect_write_blob(0, curr_sha);
            return 0;
        }
    } else if (sha_ret == ESP_OK && !have_stored) {
        /* First time - store current image sha and ensure counter at 0 */
        ESP_LOGI(TAG, "First-known image - storing image SHA and clearing counter");
        boot_protect_write_blob(0, curr_sha);
        return 0;
    } else if (sha_ret != ESP_OK) {
        ESP_LOGW(TAG, "Unable to obtain running image SHA: %s", esp_err_to_name(sha_ret));
        /* Fall through - increment based on whatever was read (or zero) */
    }

    uint8_t counter = have_stored ? stored.crash_counter : 0;
    counter++;

    /* Write back with current image SHA if available, else write zeros */
    if (sha_ret == ESP_OK) {
        boot_protect_write_blob(counter, curr_sha);
    } else {
        boot_protect_write_blob(counter, NULL);
    }

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
    uint8_t curr_sha[32];
    if (get_running_image_sha(curr_sha) == ESP_OK) {
        boot_protect_write_blob(0, curr_sha);
    } else {
        boot_protect_write_blob(0, NULL);
    }

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
