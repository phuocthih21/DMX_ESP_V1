/**
 * @file led_patterns.c
 * @brief LED color and pattern definitions
 * 
 * Defines color palettes and pattern mappings for each status code
 */

#include "status_types.h"
#include <math.h>

/**
 * @brief Color definitions for each status code
 * 
 * Colors are set to moderate brightness (30-50) to avoid eye strain
 */
const rgb_color_t STATUS_COLORS[] = {
    [STATUS_BOOTING]   = {30, 30, 30},    // White (dim)
    [STATUS_NET_ETH]   = {0, 50, 0},      // Green
    [STATUS_NET_WIFI]  = {0, 40, 40},     // Cyan
    [STATUS_NET_AP]    = {0, 0, 50},      // Blue
    [STATUS_NO_NET]    = {50, 50, 0},     // Yellow
    [STATUS_DMX_WARN]  = {50, 25, 0},     // Orange
    [STATUS_OTA]       = {50, 0, 50},     // Purple/Magenta
    [STATUS_ERROR]     = {50, 0, 0},      // Red
    [STATUS_IDENTIFY]  = {50, 50, 50},    // White (bright)
    [STATUS_OFF]       = {0, 0, 0}        // Off
};

/**
 * @brief Pattern mapping for each status code
 */
const status_pattern_t STATUS_PATTERNS[] = {
    [STATUS_BOOTING]   = PATTERN_BREATHING,
    [STATUS_NET_ETH]   = PATTERN_SOLID,
    [STATUS_NET_WIFI]  = PATTERN_SOLID,
    [STATUS_NET_AP]    = PATTERN_DOUBLE_BLINK,
    [STATUS_NO_NET]    = PATTERN_BLINK_SLOW,
    [STATUS_DMX_WARN]  = PATTERN_BLINK_FAST,
    [STATUS_OTA]       = PATTERN_STROBE,
    [STATUS_ERROR]     = PATTERN_BLINK_SLOW,
    [STATUS_IDENTIFY]  = PATTERN_STROBE,
    [STATUS_OFF]       = PATTERN_SOLID
};

/**
 * @brief Priority levels for each status code
 * 
 * Lower number = higher priority
 * STATUS_OTA (0) cannot be overridden
 */
const uint8_t STATUS_PRIORITIES[] = {
    [STATUS_OTA]       = 0,  // Highest - cannot be overridden
    [STATUS_ERROR]     = 1,
    [STATUS_IDENTIFY]  = 2,
    [STATUS_DMX_WARN]  = 3,
    [STATUS_NO_NET]    = 4,
    [STATUS_BOOTING]   = 5,
    [STATUS_NET_ETH]   = 6,
    [STATUS_NET_WIFI]  = 7,
    [STATUS_NET_AP]    = 8,  // Lowest
    [STATUS_OFF]       = 9
};

/**
 * @brief Get priority level for a status code
 * 
 * @param code Status code
 * @return Priority level (0-9, lower is higher priority)
 */
uint8_t status_get_priority(status_code_t code) {
    if (code >= STATUS_MAX) {
        return 255;  // Invalid code has lowest priority
    }
    return STATUS_PRIORITIES[code];
}

/**
 * @brief Calculate brightness for breathing effect
 * 
 * Uses exponential sine wave for smooth, natural breathing
 * Formula: brightness = (exp(sin(phase)) - 0.368) * 108
 * 
 * @param phase Phase angle (0 to 2*PI)
 * @return Brightness value (0-255)
 */
uint8_t pattern_calc_breathing(float phase) {
    // Normalize phase to 0-2π range
    while (phase > 2.0f * M_PI) {
        phase -= 2.0f * M_PI;
    }
    
    // Exponential sine wave
    float val = expf(sinf(phase)) - 0.36787944f;
    val *= 108.0f;
    
    // Clamp to 0-255
    if (val < 0.0f) val = 0.0f;
    if (val > 255.0f) val = 255.0f;
    
    return (uint8_t)val;
}

/**
 * @brief Calculate ON/OFF state for blink pattern
 * 
 * @param elapsed_us Time elapsed since pattern start (microseconds)
 * @param frequency_hz Blink frequency in Hz
 * @return true for ON, false for OFF
 */
bool pattern_calc_blink(int64_t elapsed_us, float frequency_hz) {
    // Calculate period in microseconds
    float period_us = 1000000.0f / frequency_hz;
    
    // Calculate phase (0.0 to 1.0)
    float phase = fmodf((float)elapsed_us, period_us) / period_us;
    
    // ON for first 50% of cycle
    return (phase < 0.5f);
}

/**
 * @brief Calculate ON/OFF state for double blink pattern
 * 
 * Pattern: ON(100ms) OFF(100ms) ON(100ms) OFF(700ms)
 * Total cycle: 1000ms
 * 
 * @param elapsed_us Time elapsed since pattern start (microseconds)
 * @return true for ON, false for OFF
 */
bool pattern_calc_double_blink(int64_t elapsed_us) {
    // Cycle time: 1 second
    int64_t cycle_time = elapsed_us % 1000000;
    
    if (cycle_time < 100000) return true;        // 0-100ms: ON
    else if (cycle_time < 200000) return false;  // 100-200ms: OFF
    else if (cycle_time < 300000) return true;   // 200-300ms: ON
    else return false;                           // 300-1000ms: OFF
}

/**
 * @brief Apply brightness scaling to RGB color
 * 
 * @param color Input color
 * @param brightness Brightness percentage (0-100)
 * @return Scaled color
 */
rgb_color_t pattern_apply_brightness(rgb_color_t color, uint8_t brightness) {
    rgb_color_t result;
    result.r = (color.r * brightness) / 100;
    result.g = (color.g * brightness) / 100;
    result.b = (color.b * brightness) / 100;
    return result;
}
