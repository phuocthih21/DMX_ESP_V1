/**
 * @file status_types.h
 * @brief Type definitions for status indicator module
 * 
 * Defines status codes, RGB colors, and pattern types for the WS2812B LED
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief System status codes
 * 
 * Each code represents a specific system state with associated
 * color and blink pattern
 */
typedef enum {
    STATUS_BOOTING = 0,      ///< System booting - White breathing
    STATUS_NET_ETH,          ///< Ethernet connected - Green solid
    STATUS_NET_WIFI,         ///< WiFi connected - Cyan solid
    STATUS_NET_AP,           ///< WiFi AP mode (Rescue) - Blue double blink
    STATUS_NO_NET,           ///< No network - Yellow slow blink
    STATUS_DMX_WARN,         ///< DMX failsafe active - Orange fast blink
    STATUS_OTA,              ///< OTA update in progress - Purple strobe (HIGHEST PRIORITY)
    STATUS_ERROR,            ///< System error - Red slow blink
    STATUS_IDENTIFY,         ///< Device identify mode - White strobe
    STATUS_OFF,              ///< LED off
    STATUS_MAX               ///< Sentinel value
} status_code_t;

/**
 * @brief RGB color definition
 */
typedef struct {
    uint8_t r;  ///< Red component (0-255)
    uint8_t g;  ///< Green component (0-255)
    uint8_t b;  ///< Blue component (0-255)
} rgb_color_t;

/**
 * @brief LED pattern types
 */
typedef enum {
    PATTERN_SOLID = 0,       ///< Solid color (no blinking)
    PATTERN_BREATHING,       ///< Smooth breathing effect (sine wave)
    PATTERN_BLINK_SLOW,      ///< Slow blink at 1Hz
    PATTERN_BLINK_FAST,      ///< Fast blink at 4Hz
    PATTERN_DOUBLE_BLINK,    ///< Double blink pattern (2 flashes + pause)
    PATTERN_STROBE,          ///< Rapid strobe at 10Hz
    PATTERN_MAX              ///< Sentinel value
} status_pattern_t;
