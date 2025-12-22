/**
 * @file mod_status.c
 * @brief Status LED module main implementation
 * 
 * Manages LED state machine, transitions, and FreeRTOS task
 */

/* Deprecated file retained for compatibility. Use status_led.h/status_led.c instead. */

#include "status_led.h"

/* No implementation here; wrapper kept for legacy builds */

static const char* TAG = "MOD_STATUS";

#include "led_strip.h"

/* LED strip instance (IDF led_strip) */
static led_strip_t *s_led_strip = NULL;

/* External data from led_patterns.c */
extern const rgb_color_t STATUS_COLORS[];
extern const status_pattern_t STATUS_PATTERNS[];
extern uint8_t status_get_priority(status_code_t code);
extern uint8_t pattern_calc_breathing(float phase);
extern bool pattern_calc_blink(int64_t elapsed_us, float frequency_hz);
extern bool pattern_calc_double_blink(int64_t elapsed_us);
extern rgb_color_t pattern_apply_brightness(rgb_color_t color, uint8_t brightness);

/**
 * @brief Module state structure
 */
typedef struct {
    status_code_t current_code;       // Current active status code
    status_code_t pending_code;       // Pending status for transition
    
    uint8_t global_brightness;        // Global brightness (0-100)
    
    bool is_transitioning;            // Transition in progress flag
    float transition_progress;        // Transition progress (0.0 - 1.0)
    
    int64_t pattern_start_time;       // Pattern start timestamp (us)
    
    TaskHandle_t task_handle;         // FreeRTOS task handle
    
    bool initialized;                 // Initialization flag
} status_state_t;

static status_state_t g_state = {
    .current_code = STATUS_BOOTING,
    .pending_code = STATUS_BOOTING,
    .global_brightness = 30,  // Default 30% to avoid eye strain
    .is_transitioning = false,
    .transition_progress = 0.0f,
    .pattern_start_time = 0,
    .task_handle = NULL,
    .initialized = false
};

/* Forward declarations */
static void status_task_loop(void* arg);
static void process_transition(uint32_t delta_ms);
static rgb_color_t calculate_pattern_color(status_code_t code, int64_t elapsed_us);

/**
 * @brief Initialize status LED module
 */
esp_err_t status_init(int gpio_pin) {
    ESP_LOGI(TAG, "Initializing status LED module on GPIO %d", gpio_pin);

    // Initialize IDF led_strip for single pixel WS2812B
    led_strip_config_t cfg = LED_STRIP_DEFAULT_CONFIG(1, RMT_CHANNEL_7, gpio_pin);
    s_led_strip = led_strip_new_rmt_ws2812(&cfg);
    if (!s_led_strip) {
        ESP_LOGE(TAG, "Failed to initialize IDF led_strip on GPIO %d", gpio_pin);
        return ESP_FAIL;
    }

    // Initialize state
    g_state.pattern_start_time = esp_timer_get_time();

    // Create LED task on Core 0 with low priority
    BaseType_t task_ret = xTaskCreatePinnedToCore(
        status_task_loop,
        "status_led",
        2048,                    // Stack size
        NULL,                    // Parameters
        1,                       // Priority (low)
        &g_state.task_handle,
        0                        // Core 0 (DMX uses Core 1)
    );

    if (task_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status task");
        if (s_led_strip) { led_strip_delete(s_led_strip); s_led_strip = NULL; }
        return ESP_FAIL;
    }

    g_state.initialized = true;

    ESP_LOGI(TAG, "Status LED module initialized (led_strip)");
    return ESP_OK;
}

/**
 * @brief Set status code with priority checking
 */
void status_set_code(status_code_t code) {
    if (code >= STATUS_MAX) {
        ESP_LOGW(TAG, "Invalid status code: %d", code);
        return;
    }
    
    // Check if OTA is active (cannot be overridden)
    if (g_state.current_code == STATUS_OTA && code != STATUS_OTA) {
        ESP_LOGW(TAG, "Cannot override OTA status");
        return;
    }
    
    // Check priority
    uint8_t current_priority = status_get_priority(g_state.current_code);
    uint8_t new_priority = status_get_priority(code);
    
    if (new_priority <= current_priority) {
        // Accept change (equal or higher priority)
        ESP_LOGI(TAG, "Status change: %d -> %d (priority %d -> %d)",
                 g_state.current_code, code, current_priority, new_priority);
        
        g_state.pending_code = code;
        g_state.is_transitioning = true;
        g_state.transition_progress = 0.0f;
    } else {
        ESP_LOGD(TAG, "Status change ignored (priority %d < %d)",
                 new_priority, current_priority);
    }
}

/**
 * @brief Set global LED brightness
 */
void status_set_brightness(uint8_t brightness) {
    if (brightness > 100) {
        brightness = 100;
    }
    
    g_state.global_brightness = brightness;
    ESP_LOGI(TAG, "Brightness set to %d%%", brightness);
}

/**
 * @brief Get current status code
 */
status_code_t status_get_code(void) {
    return g_state.current_code;
}

/**
 * @brief Trigger identify mode temporarily
 */
void status_trigger_identify(uint32_t duration_ms) {
    status_code_t old_code = g_state.current_code;
    
    // Force identify mode
    g_state.current_code = STATUS_IDENTIFY;
    g_state.pending_code = STATUS_IDENTIFY;
    g_state.is_transitioning = false;
    
    ESP_LOGI(TAG, "Identify mode activated for %lu ms", duration_ms);
    
    // Wait for duration
    vTaskDelay(pdMS_TO_TICKS(duration_ms));
    
    // Restore previous status
    status_set_code(old_code);
    
    ESP_LOGI(TAG, "Identify mode ended");
}

/**
 * @brief Main status LED task loop
 */
static void status_task_loop(void* arg) {
    int64_t last_time = esp_timer_get_time();
    
    ESP_LOGI(TAG, "Status LED task started");
    
    while (1) {
        int64_t now = esp_timer_get_time();
        uint32_t delta_ms = (now - last_time) / 1000;
        last_time = now;
        
        // Step 1: Process transition
        process_transition(delta_ms);
        
        // Step 2: Determine active status code
        status_code_t active_code = g_state.is_transitioning ?
                                    g_state.pending_code :
                                    g_state.current_code;
        
        // Step 3: Calculate elapsed time for pattern
        int64_t elapsed = now - g_state.pattern_start_time;
        
        // Step 4: Calculate color based on pattern
        rgb_color_t final_color = calculate_pattern_color(active_code, elapsed);
        
        // Step 5: Apply global brightness
        final_color = pattern_apply_brightness(final_color, g_state.global_brightness);
        
        // Step 6: Apply transition fade if active
        if (g_state.is_transitioning) {
            float fade_alpha;
            if (g_state.transition_progress < 0.5f) {
                // Fade out (1.0 -> 0.0)
                fade_alpha = 1.0f - (g_state.transition_progress * 2.0f);
            } else {
                // Fade in (0.0 -> 1.0)
                fade_alpha = (g_state.transition_progress - 0.5f) * 2.0f;
            }
            
            final_color.r = (uint8_t)(final_color.r * fade_alpha);
            final_color.g = (uint8_t)(final_color.g * fade_alpha);
            final_color.b = (uint8_t)(final_color.b * fade_alpha);
        }
        
        // Step 7: Send color to LED using IDF led_strip
        if (s_led_strip) {
            esp_err_t r = led_strip_set_pixel(s_led_strip, 0, final_color.r, final_color.g, final_color.b);
            if (r == ESP_OK) {
                led_strip_refresh(s_led_strip);
            } else {
                ESP_LOGW(TAG, "led_strip_set_pixel failed: %s", esp_err_to_name(r));
            }
        }

        // Step 8: Delay for 50Hz update rate (20ms)
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * @brief Process status transition
 */
static void process_transition(uint32_t delta_ms) {
    if (!g_state.is_transitioning) {
        return;
    }
    
    const float TRANSITION_DURATION_MS = 200.0f;  // 200ms total transition
    
    g_state.transition_progress += (float)delta_ms / TRANSITION_DURATION_MS;
    
    if (g_state.transition_progress >= 1.0f) {
        // Transition complete
        g_state.current_code = g_state.pending_code;
        g_state.is_transitioning = false;
        g_state.transition_progress = 1.0f;
        g_state.pattern_start_time = esp_timer_get_time();
        
        ESP_LOGD(TAG, "Transition complete, now at status %d", g_state.current_code);
    }
}

/**
 * @brief Deinitialize status module and free resources
 */
void status_deinit(void)
{
    if (g_state.task_handle) {
        vTaskDelete(g_state.task_handle);
        g_state.task_handle = NULL;
    }
    if (s_led_strip) {
        led_strip_delete(s_led_strip);
        s_led_strip = NULL;
    }
    g_state.initialized = false;
}

/**
 * @brief Calculate color based on status code and pattern
 */
static rgb_color_t calculate_pattern_color(status_code_t code, int64_t elapsed_us) {
    rgb_color_t base_color = STATUS_COLORS[code];
    status_pattern_t pattern = STATUS_PATTERNS[code];
    rgb_color_t final_color = base_color;
    
    switch (pattern) {
        case PATTERN_SOLID:
            // No modification
            break;
            
        case PATTERN_BREATHING: {
            float phase = (float)elapsed_us * 0.000001f * 2.0f;  // ~2 seconds per cycle
            uint8_t breath = pattern_calc_breathing(phase);
            final_color.r = (base_color.r * breath) / 255;
            final_color.g = (base_color.g * breath) / 255;
            final_color.b = (base_color.b * breath) / 255;
            break;
        }
        
        case PATTERN_BLINK_SLOW:
            if (!pattern_calc_blink(elapsed_us, 1.0f)) {
                final_color = (rgb_color_t){0, 0, 0};  // OFF
            }
            break;
            
        case PATTERN_BLINK_FAST:
            if (!pattern_calc_blink(elapsed_us, 4.0f)) {
                final_color = (rgb_color_t){0, 0, 0};  // OFF
            }
            break;
            
        case PATTERN_DOUBLE_BLINK:
            if (!pattern_calc_double_blink(elapsed_us)) {
                final_color = (rgb_color_t){0, 0, 0};  // OFF
            }
            break;
            
        case PATTERN_STROBE:
            if (!pattern_calc_blink(elapsed_us, 10.0f)) {
                final_color = (rgb_color_t){0, 0, 0};  // OFF
            }
            break;
            
        default:
            break;
    }
    
    return final_color;
}
