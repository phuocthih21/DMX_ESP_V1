/*
 * @file status_types.h
 * @brief Type definitions for status LED module
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/** Status codes */
typedef enum {
    STATUS_BOOTING = 0,
    STATUS_NET_ETH,
    STATUS_NET_WIFI,
    STATUS_NET_AP,
    STATUS_NO_NET,
    STATUS_DMX_WARN,
    STATUS_OTA,
    STATUS_ERROR,
    STATUS_IDENTIFY,
    STATUS_OFF,
    STATUS_MAX
} status_code_t;

/** RGB color */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/** Pattern types */
typedef enum {
    PATTERN_SOLID = 0,
    PATTERN_BREATHING,
    PATTERN_BLINK_SLOW,
    PATTERN_BLINK_FAST,
    PATTERN_DOUBLE_BLINK,
    PATTERN_STROBE,
    PATTERN_MAX
} status_pattern_t;
