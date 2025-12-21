/**
 * @file button_check.c
 * @brief Boot button detection (GPIO 0)
 * 
 * Detects button hold duration during boot to allow user-initiated
 * rescue mode or factory reset.
 */

#include "startup_types.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// Import status codes for visual feedback
#include "mod_status.h"

static const char* TAG = "BOOT_BTN";

/**
 * @brief Check boot button and determine requested mode
 * 
 * Button timing:
 * - < 3s: Normal boot
 * - 3-10s: Rescue mode
 * - > 10s: Factory reset
 * 
 * Provides visual feedback via status LED during detection.
 * 
 * @return Boot mode requested by button, or BOOT_MODE_NORMAL if not pressed
 */
boot_mode_t check_button_on_boot(void)
{
    // Configure GPIO as input with pull-up
    gpio_config_t btn_cfg = {
        .pin_bit_mask = (1ULL << BOOT_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    gpio_config(&btn_cfg);

    // Initial debounce delay
    vTaskDelay(pdMS_TO_TICKS(50));

    // Check if button is pressed (active LOW)
    if (gpio_get_level(BOOT_BUTTON_PIN) == 1) {
        // Button not pressed
        ESP_LOGD(TAG, "Boot button not pressed");
        return BOOT_MODE_NORMAL;
    }

    ESP_LOGI(TAG, "Boot button detected, measuring hold duration...");
    status_set_code(STATUS_BOOTING);  // White breathing during detection

    // Wait for 3 seconds
    vTaskDelay(pdMS_TO_TICKS(RESCUE_HOLD_MS));

    // Check if still pressed after 3s
    if (gpio_get_level(BOOT_BUTTON_PIN) == 1) {
        // Released before 3s - ignore
        ESP_LOGI(TAG, "Button released early (< 3s), normal boot");
        return BOOT_MODE_NORMAL;
    }

    // Still pressed after 3s -> RESCUE MODE triggered
    ESP_LOGW(TAG, "Rescue mode requested (button held 3s)");
    status_set_code(STATUS_NET_AP);  // Blue breathing - rescue mode indicator

    // Wait additional 7 seconds (total 10s)
    vTaskDelay(pdMS_TO_TICKS(FACTORY_HOLD_MS - RESCUE_HOLD_MS));

    // Check if still pressed after 10s
    if (gpio_get_level(BOOT_BUTTON_PIN) == 0) {
        // Still pressed after 10s -> FACTORY RESET
        ESP_LOGE(TAG, "Factory reset requested (button held 10s)");
        status_set_code(STATUS_ERROR);  // Red - factory reset warning
        return BOOT_MODE_FACTORY_RESET;
    }

    // Released between 3-10s -> RESCUE MODE
    return BOOT_MODE_RESCUE;
}
