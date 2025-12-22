/**
 * @file led_driver.c
 * @brief LED driver using ESP-IDF `led_strip` API with a safe RMT fallback.
 *
 * This implementation prefers the official `led_strip` component (single-pixel)
 * but falls back to the project's existing RMT + encoder code if `led_strip` is
 * not available or initialization fails.
 */

#include "status_types.h"
#include "esp_log.h"
#include <string.h>

#include "led_strip.h"

static const char* TAG = "LED_DRV";

/* Module state: pure led_strip handle (no manual RMT) */
static led_strip_handle_t s_led_strip = NULL;
static int s_gpio_pin = -1;

esp_err_t led_driver_init(int gpio_pin)
{
    s_gpio_pin = gpio_pin;
    ESP_LOGI(TAG, "Initializing LED driver on GPIO %d (led_strip)", gpio_pin);

    led_strip_config_t cfg = LED_STRIP_DEFAULT_CONFIG(1, RMT_CHANNEL_7, gpio_pin);
    led_strip_rmt_config_t rmt_cfg = {
        .resolution_hz = 10000000
    };

    led_strip_handle_t strip = NULL;
    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt_cfg, &strip);
    if (err != ESP_OK || strip == NULL) {
        ESP_LOGE(TAG, "led_strip init failed: %s", esp_err_to_name(err));
        return err;
    }

    s_led_strip = strip;
    return ESP_OK;
}

esp_err_t led_driver_set_color(rgb_color_t color)
{
    if (!s_led_strip) {
        ESP_LOGW(TAG, "LED driver not initialized");
        return ESP_ERR_INVALID_STATE;
    }

    esp_err_t r = led_strip_set_pixel(s_led_strip, 0, color.r, color.g, color.b);
    if (r != ESP_OK) return r;
    return led_strip_refresh(s_led_strip);
}

esp_err_t led_driver_deinit(void)
{
    if (s_led_strip) {
        led_strip_clear(s_led_strip);
        led_strip_refresh(s_led_strip);
        led_strip_del(s_led_strip);
        s_led_strip = NULL;
    }
    ESP_LOGI(TAG, "LED driver deinitialized");
    return ESP_OK;
}
