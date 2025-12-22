/*
 * @file led_patterns.c
 * @brief Color/pattern definitions and helpers for status LED
 */

#include "status_types.h"
#include <math.h>
#include <stdint.h>
#include <stdbool.h>

const rgb_color_t STATUS_COLORS[] = {
    [STATUS_BOOTING]  = {30,30,30},
    [STATUS_NET_ETH]  = {0,50,0},
    [STATUS_NET_WIFI] = {0,40,40},
    [STATUS_NET_AP]   = {0,0,50},
    [STATUS_NO_NET]   = {50,50,0},
    [STATUS_DMX_WARN] = {50,25,0},
    [STATUS_OTA]      = {50,0,50},
    [STATUS_ERROR]    = {50,0,0},
    [STATUS_IDENTIFY] = {50,50,50},
    [STATUS_OFF]      = {0,0,0}
};

const status_pattern_t STATUS_PATTERNS[] = {
    [STATUS_BOOTING]  = PATTERN_BREATHING,
    [STATUS_NET_ETH]  = PATTERN_SOLID,
    [STATUS_NET_WIFI] = PATTERN_SOLID,
    [STATUS_NET_AP]   = PATTERN_DOUBLE_BLINK,
    [STATUS_NO_NET]   = PATTERN_BLINK_SLOW,
    [STATUS_DMX_WARN] = PATTERN_BLINK_FAST,
    [STATUS_OTA]      = PATTERN_STROBE,
    [STATUS_ERROR]    = PATTERN_BLINK_SLOW,
    [STATUS_IDENTIFY] = PATTERN_STROBE,
    [STATUS_OFF]      = PATTERN_SOLID
};

const uint8_t STATUS_PRIORITIES[] = {
    [STATUS_OTA]      = 0,
    [STATUS_ERROR]    = 1,
    [STATUS_IDENTIFY] = 2,
    [STATUS_DMX_WARN] = 3,
    [STATUS_NO_NET]   = 4,
    [STATUS_BOOTING]  = 5,
    [STATUS_NET_ETH]  = 6,
    [STATUS_NET_WIFI] = 7,
    [STATUS_NET_AP]   = 8,
    [STATUS_OFF]      = 9
};

uint8_t status_get_priority(status_code_t code) {
    if (code >= STATUS_MAX) return 255;
    return STATUS_PRIORITIES[code];
}

uint8_t pattern_calc_breathing(float phase) {
    while (phase > 2.0f * M_PI) phase -= 2.0f * M_PI;
    float val = expf(sinf(phase)) - 0.36787944f;
    val *= 108.0f;
    if (val < 0.0f) val = 0.0f;
    if (val > 255.0f) val = 255.0f;
    return (uint8_t)val;
}

bool pattern_calc_blink(int64_t elapsed_us, float frequency_hz) {
    float period_us = 1000000.0f / frequency_hz;
    float phase = fmodf((float)elapsed_us, period_us) / period_us;
    return (phase < 0.5f);
}

bool pattern_calc_double_blink(int64_t elapsed_us) {
    int64_t cycle = elapsed_us % 1000000;
    if (cycle < 100000) return true;
    if (cycle < 200000) return false;
    if (cycle < 300000) return true;
    return false;
}

rgb_color_t pattern_apply_brightness(rgb_color_t color, uint8_t brightness) {
    rgb_color_t out;
    out.r = (color.r * brightness) / 100;
    out.g = (color.g * brightness) / 100;
    out.b = (color.b * brightness) / 100;
    return out;
}
