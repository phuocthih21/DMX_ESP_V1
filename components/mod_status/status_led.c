/*
 * @file status_led.c
 * @brief Implementation of status LED module using espressif/led_strip
 */

#include "status_led.h"
#include "status_types.h"
#include "led_patterns.c" /* local helpers - included directly to keep symbol visibility */
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "led_strip.h"
#include <string.h>

static const char *TAG = "STATUS_LED";
static led_strip_handle_t s_led_strip = NULL;

typedef struct {
    status_code_t current_code;
    status_code_t pending_code;
    uint8_t global_brightness;
    bool is_transitioning;
    float transition_progress;
    int64_t pattern_start_time;
    TaskHandle_t task_handle;
    bool initialized;
} status_state_t;

static status_state_t g_state = {
    .current_code = STATUS_BOOTING,
    .pending_code = STATUS_BOOTING,
    .global_brightness = 30,
    .is_transitioning = false,
    .transition_progress = 0.0f,
    .pattern_start_time = 0,
    .task_handle = NULL,
    .initialized = false
};

static void status_task_loop(void *arg);
static void process_transition(uint32_t delta_ms);
static rgb_color_t calculate_pattern_color(status_code_t code, int64_t elapsed_us);

esp_err_t status_init(int gpio_pin) {
    ESP_LOGI(TAG, "Initializing status LED on GPIO %d", gpio_pin);
    if (g_state.initialized) return ESP_OK;

    led_strip_config_t cfg = {
        .strip_gpio_num = gpio_pin,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB,
        .flags = { .invert_out = 0 },
    };
    led_strip_rmt_config_t rmt_cfg = { .resolution_hz = 10000000 };

    led_strip_handle_t strip = NULL;
    esp_err_t err = led_strip_new_rmt_device(&cfg, &rmt_cfg, &strip);
    if (err != ESP_OK || strip == NULL) {
        ESP_LOGE(TAG, "led_strip_new_rmt_device failed: %s", esp_err_to_name(err));
        return err;
    }

    s_led_strip = strip;
    g_state.pattern_start_time = esp_timer_get_time();

    BaseType_t task_ret = xTaskCreatePinnedToCore(status_task_loop, "status_led", 2048, NULL, 1, &g_state.task_handle, 0);
    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status task");
        if (s_led_strip) { led_strip_del(s_led_strip); s_led_strip = NULL; }
        return ESP_FAIL;
    }

    g_state.initialized = true;
    ESP_LOGI(TAG, "Status LED initialized");
    return ESP_OK;
}

void status_set_code(status_code_t code) {
    if (code >= STATUS_MAX) return;
    if (g_state.current_code == STATUS_OTA && code != STATUS_OTA) return;
    uint8_t cur_pri = status_get_priority(g_state.current_code);
    uint8_t new_pri = status_get_priority(code);
    if (new_pri <= cur_pri) {
        g_state.pending_code = code;
        g_state.is_transitioning = true;
        g_state.transition_progress = 0.0f;
    }
}

void status_set_brightness(uint8_t brightness) {
    if (brightness > 100) brightness = 100;
    g_state.global_brightness = brightness;
}

status_code_t status_get_code(void) {
    return g_state.current_code;
}

void status_trigger_identify(uint32_t duration_ms) {
    status_code_t old = g_state.current_code;
    g_state.current_code = STATUS_IDENTIFY;
    g_state.pending_code = STATUS_IDENTIFY;
    g_state.is_transitioning = false;
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    status_set_code(old);
}

void status_deinit(void) {
    if (g_state.task_handle) { vTaskDelete(g_state.task_handle); g_state.task_handle = NULL; }
    if (s_led_strip) { led_strip_clear(s_led_strip); led_strip_refresh(s_led_strip); led_strip_del(s_led_strip); s_led_strip = NULL; }
    g_state.initialized = false;
}

static void status_task_loop(void *arg) {
    (void)arg;
    int64_t last = esp_timer_get_time();
    while (1) {
        int64_t now = esp_timer_get_time();
        uint32_t dt = (uint32_t)((now - last) / 1000);
        last = now;
        process_transition(dt);
        status_code_t active = g_state.is_transitioning ? g_state.pending_code : g_state.current_code;
        int64_t elapsed = now - g_state.pattern_start_time;
        rgb_color_t color = calculate_pattern_color(active, elapsed);
        color = pattern_apply_brightness(color, g_state.global_brightness);
        if (g_state.is_transitioning) {
            float fade;
            if (g_state.transition_progress < 0.5f) fade = 1.0f - (g_state.transition_progress * 2.0f);
            else fade = (g_state.transition_progress - 0.5f) * 2.0f;
            color.r = (uint8_t)(color.r * fade);
            color.g = (uint8_t)(color.g * fade);
            color.b = (uint8_t)(color.b * fade);
        }
        if (s_led_strip) {
            esp_err_t r = led_strip_set_pixel(s_led_strip, 0, color.r, color.g, color.b);
            if (r == ESP_OK) led_strip_refresh(s_led_strip);
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

static void process_transition(uint32_t delta_ms) {
    if (!g_state.is_transitioning) return;
    const float TRANS_MS = 200.0f;
    g_state.transition_progress += (float)delta_ms / TRANS_MS;
    if (g_state.transition_progress >= 1.0f) {
        g_state.current_code = g_state.pending_code;
        g_state.is_transitioning = false;
        g_state.transition_progress = 1.0f;
        g_state.pattern_start_time = esp_timer_get_time();
    }
}

static rgb_color_t calculate_pattern_color(status_code_t code, int64_t elapsed_us) {
    rgb_color_t base = STATUS_COLORS[code];
    status_pattern_t pat = STATUS_PATTERNS[code];
    rgb_color_t out = base;
    switch (pat) {
        case PATTERN_BREATHING: {
            float phase = (float)elapsed_us * 0.000001f * 2.0f;
            uint8_t b = pattern_calc_breathing(phase);
            out.r = (base.r * b) / 255;
            out.g = (base.g * b) / 255;
            out.b = (base.b * b) / 255;
            break;
        }
        case PATTERN_BLINK_SLOW:
            if (!pattern_calc_blink(elapsed_us, 1.0f)) out = (rgb_color_t){0,0,0};
            break;
        case PATTERN_BLINK_FAST:
            if (!pattern_calc_blink(elapsed_us, 4.0f)) out = (rgb_color_t){0,0,0};
            break;
        case PATTERN_DOUBLE_BLINK:
            if (!pattern_calc_double_blink(elapsed_us)) out = (rgb_color_t){0,0,0};
            break;
        case PATTERN_STROBE:
            if (!pattern_calc_blink(elapsed_us, 10.0f)) out = (rgb_color_t){0,0,0};
            break;
        default:
            break;
    }
    return out;
}
