/*
 * @file status_led.h
 * @brief Public API for status LED module
 */

#pragma once

#include "status_types.h"
#include "esp_err.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t status_init(int gpio_pin);
void status_set_code(status_code_t code);
void status_set_brightness(uint8_t brightness);
status_code_t status_get_code(void);
void status_trigger_identify(uint32_t duration_ms);
void status_deinit(void);

#ifdef __cplusplus
}
#endif
